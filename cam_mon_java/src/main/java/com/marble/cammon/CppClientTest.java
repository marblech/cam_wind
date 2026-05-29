package com.marble.cammon;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

/**
 * C++ 控制器客户端测试 - Java 作为服务器，native 控制器发送 PTZ 命令。
 * 
 * 此测试类验证以下功能：
 * 1. Java 服务器可接收 native 控制器发送的 PTZ 数据包
 * 2. Java 服务器发送 ACK 后，`CamMonNative.setPTZ(...)` 可收到响应长度
 * 
 * 测试架构：Java 服务器 <- UDP <- C++ 控制器客户端
 * 
 * @author marble
 * @version 1.0
 */
public class CppClientTest {
    
    /** 服务器监听端口 */
    private static final int PORT = 6002;
    
    /** 本地回环地址 */
    private static final String LOCALHOST = "127.0.0.1";
    
    /** 服务器就绪标志（线程安全） */
    private static volatile boolean serverReady = false;
    
    /** 接收到的数据包计数（线程安全） */
    private static volatile int packetCount = 0;
    
    /** 服务器错误信息（线程安全） */
    private static volatile String serverError = null;
    
    public static void main(String[] args) throws Exception {
        System.out.println("=== C++ Client Library Test ===");
        System.out.println("Architecture: Java Server <- UDP -> C++ Client");
        System.out.println();
        
        // Start Java UDP server in a daemon thread
        Thread serverThread = new Thread(() -> runServer(), "JavaServer");
        serverThread.setDaemon(true);
        serverThread.start();
        
        // Wait for server to be fully ready
        long waitStart = System.currentTimeMillis();
        while (!serverReady && serverError == null && System.currentTimeMillis() - waitStart < 5000) {
            Thread.sleep(50);
        }
        
        if (serverError != null) {
            System.out.println("[FAIL] Server failed to start: " + serverError);
            return;
        }
        
        if (!serverReady) {
            System.out.println("[FAIL] Server did not become ready in time");
            return;
        }
        
        System.out.println("[Main] Server is ready, starting C++ client tests...");
        System.out.println();
        
        // Small delay to ensure server is fully bound
        Thread.sleep(200);
        
        testServoCommand();
        
        // Wait for server to finish
        serverThread.join(2000);
        
        System.out.println("\n=== Test Complete ===");
        System.out.println("Total packets received by server: " + packetCount);
    }
    
    /**
     * 测试舵机控制命令发送
     * 
     * 调用 CamMonNative.setPTZ 发送舵机控制命令，
     * 验证控制器接口能否成功发送 UDP 数据包并接收响应。
     */
    private static void testServoCommand() {
        System.out.println("[客户端] 调用 CamMonNative.setPTZ(...)");
        System.out.println("[客户端] 目标地址: " + LOCALHOST + ":" + PORT);
        System.out.println("[客户端] 参数: 方位角=45.0°, 俯仰角=30.0°, 方位速度=0.0°, 俯仰速度=0.0°, 距离=1000, 序列号=1, 控制=0xFF, 设备=0x01, 包类型=0x20");
        
        long sendStart = System.currentTimeMillis();
        int responseBytes = CamMonNative.setPTZ(
            LOCALHOST, PORT, 
            45.0f,   // 方位角（度）
            30.0f,   // 俯仰角（度）
            0.0f,    // 方位角速度（度/秒）
            0.0f,    // 俯仰角速度（度/秒）
            1000,    // 目标距离（毫米）
            1,       // 序列号
            0xFF,    // 控制字节
            0x01,    // 设备类型
            0x20,    // 包类型
            10000    // 10秒超时
        );
        long sendElapsed = System.currentTimeMillis() - sendStart;
        
        if (responseBytes <= 0) {
            System.out.println("[客户端] 失败: setPTZ 返回 " + responseBytes);
        } else {
            System.out.println("[客户端] 成功! 接收到 " + responseBytes + " 字节");
            System.out.println("[客户端] 耗时: " + sendElapsed + "毫秒");
        }
    }
    
    /**
     * 以十六进制格式打印字节数组
     * 
     * @param label 标签名称
     * @param data 要打印的字节数组
     */
    private static void printHex(String label, byte[] data) {
        System.out.println(label + " 十六进制:");
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < data.length; i++) {
            sb.append(String.format("%02X ", data[i] & 0xFF));
            if ((i + 1) % 16 == 0) sb.append("\n");
        }
        System.out.println(sb.toString().trim());
    }
    
    private static void runServer() {
        try (DatagramSocket sock = new DatagramSocket(PORT, InetAddress.getByName(LOCALHOST))) {
            sock.setSoTimeout(15000);
            serverReady = true;
            System.out.println("[Server] Listening on " + LOCALHOST + ":" + PORT);
            System.out.println("[Server] Waiting for packets...");
            
            byte[] buf = new byte[256];
            
            while (packetCount < 1) {
                DatagramPacket recv = new DatagramPacket(buf, buf.length);
                sock.receive(recv);
                packetCount++;
                
                System.out.println();
                System.out.println("[Server] *** PACKET #" + packetCount + " FROM C++ CLIENT ***");
                System.out.println("[Server] Source: " + recv.getAddress().getHostAddress() + ":" + recv.getPort());
                System.out.println("[Server] Length: " + recv.getLength() + " bytes");
                System.out.println("[Server] Raw data:");
                printHex("  ", buf, recv.getLength());
                
                parsePacket(buf, recv.getLength());
                byte[] response = constructResponse(buf, recv.getLength());
                
                DatagramPacket out = new DatagramPacket(response, response.length,
                    recv.getAddress(), recv.getPort());
                sock.send(out);
                System.out.println("[Server] Sent " + response.length + "-byte response");
            }
            
            System.out.println();
            System.out.println("[Server] All tests completed, shutting down");
            
        } catch (Exception e) {
            serverError = e.getMessage();
            System.out.println("[Server] ERROR: " + e.getMessage());
            e.printStackTrace();
        }
    }
    
    private static void parsePacket(byte[] buf, int len) {
        if (len >= 5) {
            System.out.println("[Server] Packet structure:");
            System.out.println("  hdr1: 0x" + String.format("%02X", buf[0] & 0xFF));
            System.out.println("  hdr2: 0x" + String.format("%02X", buf[1] & 0xFF));
            System.out.println("  [Controller PTZ packet]");
        }
    }
    
    private static byte[] constructResponse(byte[] request, int len) {
        byte[] response;
        
        // Check if it's a servo packet (0x20 packet type typically has different structure)
        if (len >= 28) {
            // Servo response (22 bytes)
            response = new byte[22];
            response[0] = 0x0F;
            response[1] = (byte)0xF0;
            response[2] = 0x02;  // addr
            response[3] = 0x20;  // func (servo)
            response[4] = 0x01;  // ctrl (ack)
            // Data: 12 bytes for servo response
            response[5] = (byte)0x00; response[6] = (byte)0x00;
            response[7] = (byte)0x00; response[8] = (byte)0x00;
            response[9] = (byte)0x00; response[10] = (byte)0x00;
            response[11] = (byte)0x00; response[12] = (byte)0x00;
            response[13] = (byte)0x00; response[14] = (byte)0x00;
            response[15] = (byte)0x00; response[16] = (byte)0x00;
            // Checksum
            response[17] = (byte)calculateChecksum(response, 5, 13);
            response[18] = (byte)0xF0;  // tail1
            response[19] = (byte)0x0F;  // tail2
            // Extra bytes for alignment
            response[20] = (byte)0x00;
            response[21] = (byte)0x00;
        } else {
            // Camera response (15 bytes)
            response = new byte[15];
            response[0] = 0x0F;
            response[1] = (byte)0xF0;
            response[2] = 0x00;  // addr
            response[3] = 0x01;  // func
            response[4] = 0x00;  // ctrl
            // 8 bytes data
            for (int i = 5; i < 13; i++) response[i] = 0x00;
            // Checksum
            response[13] = calculateChecksum(response, 5, 8);
            response[14] = 0x0F;  // tail
        }
        
        return response;
    }
    
    private static byte calculateChecksum(byte[] data, int start, int count) {
        byte cs = 0x00;
        for (int i = start; i < start + count && i < data.length; i++) {
            cs ^= data[i];
        }
        return cs;
    }
    
    private static void printHex(String label, byte[] data, int len) {
        System.out.println(label);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < len; i++) {
            sb.append(String.format("%02X ", data[i] & 0xFF));
            if ((i + 1) % 16 == 0) sb.append("\n");
        }
        System.out.println(sb.toString().trim());
    }
}