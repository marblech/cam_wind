package com.marble.cammon;

/**
 * 客户端主程序 - 通过 JNI 调用 `cam_controller` 能力。
 * 
 * 当前设计下，JNI 仅暴露控制器语义接口：
 * 1. 启动/停止状态监听
 * 2. 读取最近一次解析出的 PTZ 状态
 * 3. 发送 PTZ 控制命令
 * 
 * 使用方式：
 * - 发送 PTZ：`java com.marble.cammon.Client [host] [port] [az] [el]`
 * - 监听状态：`java com.marble.cammon.Client listen [listenPort]`
 * 
 * @author marble
 * @version 1.0
 */
public class Client {
    /**
     * 客户端主入口点
     * 
     * @param args 命令行参数
     * @throws Exception 当原生调用失败时抛出异常
     */
    public static void main(String[] args) throws Exception {
        if (args.length > 0 && "listen".equalsIgnoreCase(args[0])) {
            int listenPort = args.length > 1 ? Integer.parseInt(args[1]) : 5001;
            runListenerDemo(listenPort);
            return;
        }

        String host = "127.0.0.1";
        int port = 4001;
        float az = 123.45f;
        float el = 10.0f;
        if (args.length > 0) host = args[0];
        if (args.length > 1) port = Integer.parseInt(args[1]);
        if (args.length > 2) az = Float.parseFloat(args[2]);
        if (args.length > 3) el = Float.parseFloat(args[3]);

        System.out.println("Sending PTZ command via CamController JNI...");
        int rc = CamMonNative.setPTZ(host, port, az, el, 1.5f, 0.5f, 100, 1, 1, 0x01, 0x02, 2000);
        if (rc <= 0) {
            System.err.println("setPTZ failed, rc=" + rc);
            System.exit(1);
        }
        System.out.println("setPTZ succeeded, reply bytes=" + rc);
    }

    private static void runListenerDemo(int listenPort) throws Exception {
        System.out.println("Starting PTZ listener on UDP port " + listenPort + "...");
        boolean started = CamMonNative.startStatusListener(listenPort);
        if (!started) {
            System.err.println("Failed to start native status listener");
            System.exit(2);
        }

        Runtime.getRuntime().addShutdownHook(new Thread(CamMonNative::stopStatusListener));
        System.out.println("Listener started. Press Ctrl+C to exit.");
        while (true) {
            PTZStatus ptz = CamMonNative.getPTZ();
            if (ptz != null) {
                System.out.println(String.format("PTZ az=%.3f el=%.3f ir=%.3f vis=%.3f", ptz.az, ptz.el, ptz.irFocus, ptz.visFocus));
            }
            Thread.sleep(500);
        }
    }
}