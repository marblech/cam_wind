package com.marble.cammon;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.util.Arrays;

/**
 * 客户端主程序 - 通过 JNI 调用 C++ 原生库进行摄像头监控通信。
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
        DatagramSocket sock = new DatagramSocket();
        InetAddress addr = InetAddress.getByName(host);
        boolean useServo = args.length > 2 && args[2].equals("servo");
        boolean useCrosshair = args.length > 2 && args[2].equals("crosshair");
        boolean usePTZ = args.length > 2 && args[2].equals("ptz");
        if (useCrosshair) {
            System.out.println("Running crosshair sequence...");
            CrosshairControl.runCrosshairSequence(host, port, false);
            return;
        } else if (usePTZ) {
            System.out.println("Running PTZ roundtrip...");
            PTZIntegrationRunner.runPTZRoundtrip(host, port);
            return;
        } else if (!useServo) {
            byte[] payload = new byte[15];
            payload[0] = 5; // example parameter (e.g., speed)
            // call native C++ library to send camera command
            byte[] resp = CamMonNative.sendCameraCommand(host, port, (byte)0x01, (byte)0x01, payload, 2000);
            if (resp == null) {
                System.err.println("No response or error from native sendCameraCommand");
                return;
            }
            Protocol.Packet pkt = Protocol.parsePacket(resp);
            if (pkt == null) { System.err.println("Invalid response"); return; }
            System.out.println(String.format("Client(native): received addr=0x%02X func=0x%02X payload_len=%d", pkt.addr, pkt.func, pkt.data.length));
        } else {
            // call native servo sender
            System.out.println("Sending servo command via native C++ library...");
            byte[] resp = CamMonNative.sendServoCommand(host, port, 123.45f, 10.0f, 1.5f, 0.5f, 100, 1, 1, 0x01, 0x02, 2000);
            if (resp == null) { System.err.println("No servo response or error"); return; }
            System.out.println("Received " + resp.length + " bytes from native: " + bytesToHex(resp));
            Protocol.ServoPacket parsed = Protocol.ServoPacket.parseServo(resp);
            if (parsed == null) { System.err.println("Invalid servo response"); return; }
            System.out.println(String.format("Client(native): received servo seq=%d az=%f el=%f", parsed.seq, parsed.azimuth, parsed.elevation));
        }
    }
}