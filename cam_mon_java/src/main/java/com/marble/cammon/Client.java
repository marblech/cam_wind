package com.marble.cammon;

// Client only uses JNA interfaces from CamMonNative; UDP fallback removed

/**
 * 客户端主程序 - 通过 JNA（Java Native Access）调用 C++ 原生库进行摄像头监控通信。
 * 
 * 此类演示了如何使用 CamMonNative 类发送相机控制和舵机控制命令。
 * 支持两种通信模式：
 * 1. 相机控制模式：发送可见光相机控制命令
 * 2. 舵机控制模式：发送舵机运动控制命令
 * 
 * 使用方式：
 * - 默认模式: java com.marble.cammon.Client [host] [port]
 * - 舵机模式: java com.marble.cammon.Client [host] [port] servo
 * 
 * @author marble
 * @version 1.0
 */
public class Client {
    
    /**
     * 将字节数组转换为十六进制字符串表示
     * 
     * @param bytes 要转换的字节数组
     * @return 十六进制字符串（每个字节后跟一个空格）
     */
    private static String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) sb.append(String.format("%02X ", b));
        return sb.toString();
    }

    /**
     * 客户端主入口点
     * 
     * 根据命令行参数决定发送相机控制命令还是舵机控制命令。
     * 默认发送相机控制命令，添加 "servo" 参数时发送舵机控制命令。
     * 
     * @param args 命令行参数 [host] [port] [servo]
     * @throws Exception 当网络通信或原生调用失败时抛出异常
     */
    public static void main(String[] args) throws Exception {
        String host = "127.0.0.1";
        int port = 4001;
        if (args.length > 0) host = args[0];
        if (args.length > 1) port = Integer.parseInt(args[1]);
        boolean useServo = args.length > 2 && args[2].equals("servo");
        boolean useCrosshair = args.length > 2 && args[2].equals("crosshair");
        boolean usePTZ = args.length > 2 && args[2].equals("ptz");
        if (useCrosshair) {
            System.out.println("Crosshair helper unavailable in this build; skipping.");
            return;
        } else if (usePTZ) {
            System.out.println("PTZ helper unavailable in this build; skipping.");
            return;
        } else if (!useServo) {
            System.err.println("Camera control via JNA is not available in this build. Use 'servo' mode to call CamMonNative.setPTZ.");
            return;
        } else {
            // Use JNA-based setPTZ (declared in CamMonNative)
            System.out.println("Sending servo command via CamMonNative.setPTZ...");
            int rc = CamMonNative.setPTZ(host, port, 123.45f, 10.0f, 1.5f, 0.5f, 100, 1, 1, Protocol.SERVO_DEVICE_TYPE, Protocol.SERVO_PACKET_TYPE_POINT, 2000);
            if (rc <= 0) { System.err.println("setPTZ failed or timed out, rc=" + rc); return; }
            System.out.println("setPTZ returned byteCount=" + rc);
            byte[] last = CamMonNative.getLastStatus();
            if (last != null) {
                System.out.println("Last status (bytes): " + bytesToHex(last));
            }
        }
    }

    // UDP fallback removed. Client now only uses JNA methods declared in CamMonNative.
}