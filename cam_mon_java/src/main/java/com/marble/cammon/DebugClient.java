package com.marble.cammon;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * 调试客户端 - 用于调试 Java ↔ JNI ↔ C++ 控制器链路。
 * 
 * 此类提供两个测试用例：
 * 1. 状态监听测试：Java 发送状态帧给 native 监听器并读取 PTZ
 * 2. PTZ 控制测试：Java 启动模拟服务端，native 通过控制器发送 PTZ 命令
 * 
 * 使用方式：直接运行 main 方法
 * 
 * @author marble
 * @version 1.0
 */
public class DebugClient {
    
    /**
     * 调试客户端主入口点
     * 
     * 按顺序执行两个测试：
     * 1. 纯 Java UDP 通信测试
     * 2. C++ 原生库 UDP 通信测试
     * 
     * @param args 命令行参数（未使用）
     * @throws Exception 当测试过程中发生异常时
     */
    public static void main(String[] args) throws Exception {
        System.out.println("=== 调试客户端 ===");
        
        System.out.println("\n--- 测试1：状态监听 ---");
        testStatusListener();
        
        System.out.println("\n--- 测试2：PTZ 控制 ---");
        testControllerPtz();
    }

    static void testStatusListener() throws Exception {
        int listenPort = 5003;
        boolean started = CamMonNative.startStatusListener(listenPort);
        if (!started) {
            throw new IllegalStateException("Failed to start native status listener");
        }

        byte[] pkt = new byte[51];
        pkt[0] = 0x1F;
        pkt[1] = (byte) 0xF1;
        ByteBuffer bb = ByteBuffer.wrap(pkt).order(ByteOrder.LITTLE_ENDIAN);
        bb.position(6);
        bb.putFloat(12.34f);
        bb.position(10);
        bb.putFloat(56.78f);
        bb.position(28);
        bb.putFloat(11.0f);
        bb.position(32);
        bb.putFloat(-2.5f);

        try (DatagramSocket socket = new DatagramSocket()) {
            DatagramPacket packet = new DatagramPacket(pkt, pkt.length, InetAddress.getByName("127.0.0.1"), listenPort);
            socket.send(packet);
        }

        Thread.sleep(200);
        PTZStatus status = CamMonNative.getPTZ();
        System.out.println("[状态监听] 读取结果: " + (status == null ? "null" :
            String.format("az=%.2f el=%.2f ir=%.2f vis=%.2f", status.az, status.el, status.irFocus, status.visFocus)));
        CamMonNative.stopStatusListener();
    }

    static void testControllerPtz() throws Exception {
        ProcessBuilder pb = new ProcessBuilder("java", "-cp", "target/classes", "com.marble.cammon.ServerSimulator", "4003");
        pb.redirectErrorStream(true);
        Process server = pb.start();
        Thread.sleep(500);
        try {
            int rc = CamMonNative.setPTZ("127.0.0.1", 4003, 45.0f, 10.0f, 1.0f, 1.0f, 100, 1, 1, 0x01, 0x02, 2000);
            System.out.println("[PTZ 控制] setPTZ rc=" + rc);
        } finally {
            server.destroyForcibly();
        }
    }
}