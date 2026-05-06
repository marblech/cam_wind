package com.marble.cammon;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

/**
 * 调试客户端 - 用于调试和测试 UDP 通信。
 * 
 * 此类提供两个测试用例：
 * 1. 纯 Java UDP 通信测试：验证 Java 原生 UDP 发送/接收功能
 * 2. C++ UDP 通信测试：通过 JNI 调用 C++ 库进行 UDP 通信
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
        
        // 测试1：纯 Java UDP 通信
        System.out.println("\n--- 测试1：纯 Java UDP ---");
        testJavaUDP();
        
        // 测试2：通过原生库进行 C++ UDP 通信
        System.out.println("\n--- 测试2：通过原生库进行 C++ UDP ---");
        testCppUDP();
    }
    
    /**
     * 测试纯 Java UDP 通信
     * 
     * 创建一个简单的回显服务器，然后使用 Java 原生 UDP 套接字
     * 发送数据包并接收响应，验证 Java UDP 通信功能正常。
     * 
     * @throws Exception 当网络通信或线程操作失败时
     */
    static void testJavaUDP() throws Exception {
        String host = "127.0.0.1";
        int port = 4002; // 使用不同端口避免冲突
        
        // 在后台线程中启动简单的回显服务器
        Thread serverThread = new Thread(() -> {
            try {
                DatagramSocket sock = new DatagramSocket(port);
                byte[] buf = new byte[256];
                DatagramPacket recv = new DatagramPacket(buf, buf.length);
                sock.receive(recv);
                System.out.println("[服务器] 接收到 " + recv.getLength() + " 字节: ");
                for (int i = 0; i < Math.min(20, recv.getLength()); i++) {
                    System.out.printf("%02X ", buf[i] & 0xFF);
                }
                System.out.println();
                
                // 发送响应 back
                final byte[] respData = "OK".getBytes();
                byte[] resp = respData;
                DatagramPacket out = new DatagramPacket(resp, resp.length, 
                    recv.getAddress(), recv.getPort());
                sock.send(out);
                sock.close();
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
        serverThread.start();
        
        // 等待服务器启动
        Thread.sleep(500);
        
        // 通过 Java UDP 发送
        DatagramSocket client = new DatagramSocket();
        byte[] payload = new byte[]{(byte)0x0F, (byte)0xF0, (byte)0x01, (byte)0x01};
        DatagramPacket out = new DatagramPacket(payload, payload.length, 
            InetAddress.getByName(host), port);
        client.send(out);
        System.out.println("[客户端] 发送了 " + payload.length + " 字节");
        
        // 接收响应
        byte[] respBuf = new byte[256];
        DatagramPacket in = new DatagramPacket(respBuf, respBuf.length);
        client.setSoTimeout(2000);
        try {
            client.receive(in);
            System.out.println("[客户端] 接收到 " + in.getLength() + " 字节，来自 " 
                + in.getAddress().getHostAddress() + ":" + in.getPort());
        } catch (Exception e) {
            System.out.println("[客户端] 接收超时: " + e.getMessage());
        }
        client.close();
        
        serverThread.join(3000);
    }
    
    /**
     * 测试通过 C++ 原生库进行 UDP 通信
     * 
     * 创建一个简单的回显服务器，然后使用 CamMonNative.sendCameraCommand
     * 通过 C++ 库发送 UDP 数据包并接收响应，验证 JNI 桥接功能正常。
     * 
     * @throws Exception 当网络通信或线程操作失败时
     */
    static void testCppUDP() throws Exception {
        String host = "127.0.0.1";
        int port = 4003; // 另一个用于 C++ 测试的端口
        
        // 启动回显服务器
        Thread serverThread = new Thread(() -> {
            try {
                DatagramSocket sock = new DatagramSocket(port);
                sock.setSoTimeout(5000);
                byte[] buf = new byte[256];
                DatagramPacket recv = new DatagramPacket(buf, buf.length);
                sock.receive(recv);
                System.out.println("[C++ 服务器] 接收到 " + recv.getLength() + " 字节");
                
                byte[] resp = new byte[]{(byte)0x0F, (byte)0xF1, (byte)0x00, (byte)0x01};
                DatagramPacket out = new DatagramPacket(resp, resp.length, 
                    recv.getAddress(), recv.getPort());
                sock.send(out);
                sock.close();
            } catch (Exception e) {
                System.out.println("[C++ 服务器] 错误: " + e.getMessage());
                e.printStackTrace();
            }
        });
        serverThread.start();
        
        Thread.sleep(500);
        
        // 通过 C++ 原生库发送
        byte[] payload = new byte[]{(byte)0x0F, (byte)0xF0, (byte)0x01, (byte)0x01};
        byte[] resp = CamMonNative.sendCameraCommand(host, port, (byte)0x01, (byte)0x01, payload, 2000);
        
        if (resp == null) {
            System.out.println("[客户端] C++ sendCameraCommand 返回 NULL");
        } else {
            System.out.println("[客户端] C++ sendCameraCommand 返回 " + resp.length + " 字节: ");
            for (int i = 0; i < Math.min(20, resp.length); i++) {
                System.out.printf("%02X ", resp[i] & 0xFF);
            }
            System.out.println();
        }
        
        serverThread.join(5000);
    }
}