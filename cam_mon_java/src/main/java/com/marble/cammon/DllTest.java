package com.marble.cammon;

import java.net.DatagramSocket;
import java.net.DatagramPacket;
import java.nio.ByteBuffer;

/**
 * DLL 测试程序 - 验证 Java 通过 JNA 调用 C++ cammon.dll 的完整功能
 * 
 * 测试方案：
 * 1. 启动一个本地 UDP 回显服务器（模拟摄像头设备）
 * 2. 创建 CamMonNative 控制器并启动 UDP 监听
 * 3. 发送舵机控制命令到本地回显服务器
 * 4. 接收并解析返回的数据包
 */
public class DllTest {

    private static final int TEST_PORT = 4001;

    public static void main(String[] args) throws Exception {
        System.out.println("========== cammon.dll JNA 调用测试 ==========");
        System.out.println();

        // 第一步：启动本地 UDP 回显服务器（模拟摄像头/舵机设备）
        System.out.println("[1/5] 启动 UDP 回显服务器（端口 " + TEST_PORT + "）...");
        Thread echoServer = startEchoServer();
        Thread.sleep(500); // 等待服务器启动

        // 第二步：加载 DLL 并创建控制器
        System.out.println("[2/5] 加载 cammon.dll 并创建控制器...");
        boolean started = CamMonNative.startStatusListener(TEST_PORT);
        if (!started) {
            System.err.println("错误: 无法启动状态监听器");
            return;
        }
        System.out.println("成功: 状态监听器已启动");
        System.out.println();

        // 第三步：发送舵机控制命令
        System.out.println("[3/5] 发送舵机控制命令...");
        System.out.println("  目标: 127.0.0.1:" + TEST_PORT);
        System.out.println("  az=123.45, el=10.0, azSpeed=1.5, elSpeed=0.5");
        System.out.println("  targetDistance=1000mm, timeout=3000ms");
        
        int byteCount = CamMonNative.setPTZ(
            "127.0.0.1", TEST_PORT,
            123.45f, 10.0f,  // az, el
            1.5f, 0.5f,      // azSpeed, elSpeed
            1000,            // targetDistance (mm)
            1,               // seq
            1,               // control
            Protocol.SERVO_DEVICE_TYPE,
            Protocol.SERVO_PACKET_TYPE_POINT,
            3000             // timeout (ms)
        );
        System.out.println();

        // 第四步：获取并显示返回数据
        System.out.println("[4/5] 获取响应数据...");
        byte[] status = CamMonNative.getLastStatus();
        if (status != null && status.length > 0) {
            System.out.println("  接收字节数: " + status.length);
            System.out.println("  数据内容: " + bytesToHex(status));
            
            // 尝试解析 PTZ 数据（ServoPacket）
            Protocol.ServoPacket servo = Protocol.ServoPacket.parseServo(status);
            if (servo != null) {
                System.out.println("  解析舵机数据包:");
                System.out.println("    az (方位角):      " + String.format("%.2f", servo.azimuth) + "°");
                System.out.println("    el (俯仰角):      " + String.format("%.2f", servo.elevation) + "°");
                System.out.println("    azSpeed (方位角速度): " + String.format("%.2f", servo.azSpeed) + "°/s");
                System.out.println("    elSpeed (俯仰角速度): " + String.format("%.2f", servo.elSpeed) + "°/s");
                System.out.println("    targetDistance (目标距离): " + servo.targetDistance + "mm");
            } else {
                System.out.println("  数据不是有效的舵机包格式");
            }
        } else {
            System.out.println("  无响应数据（设备可能未返回数据）");
        }
        System.out.println();

        // 第五步：清理资源
        System.out.println("[5/5] 清理资源...");
        CamMonNative.stopStatusListener();
        stopEchoServer();
        echoServer.join(1000);
        System.out.println("成功: 测试完成");
        System.out.println();
        System.out.println("========== 测试结束 ==========");
    }

    /**
     * 启动 UDP 回显服务器
     */
    private static volatile boolean serverRunning = false;
    private static DatagramSocket echoSocket = null;

    private static Thread startEchoServer() {
        Thread t = new Thread(() -> {
            try {
                echoSocket = new DatagramSocket(TEST_PORT);
                byte[] buffer = new byte[2048];
                System.out.println("  UDP 回显服务器已启动，等待连接...");
                
                while (serverRunning) {
                    DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                    echoSocket.receive(packet);
                    
                    // 回显发送的数据（模拟设备响应）
                    byte[] responseData = buffer.clone();
                    // 修改响应数据中的某些字节以示区别（序列号+1）
                    if (responseData.length > 14) {
                        responseData[14] = (byte) (responseData[14] + 1);
                    }
                    DatagramPacket reply = new DatagramPacket(
                        responseData, responseData.length,
                        packet.getAddress(), packet.getPort()
                    );
                    echoSocket.send(reply);
                }
            } catch (Exception e) {
                if (serverRunning) {
                    System.err.println("  UDP 服务器错误: " + e.getMessage());
                }
            }
        });
        t.setDaemon(true);
        t.start();
        return t;
    }

    private static void stopEchoServer() {
        serverRunning = true;
        if (echoSocket != null && !echoSocket.isClosed()) {
            echoSocket.close();
        }
    }

    /**
     * 字节数组转十六进制字符串
     */
    private static String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02X ", b));
        }
        return sb.toString().trim();
    }
}