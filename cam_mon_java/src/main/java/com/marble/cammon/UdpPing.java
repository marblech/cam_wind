package com.marble.cammon;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

public class UdpPing {
    public static void main(String[] args) throws Exception {
        String host = "127.0.0.1";
        int port = 4000;
        if (args.length > 0) host = args[0];
        if (args.length > 1) port = Integer.parseInt(args[1]);

        DatagramSocket sock = new DatagramSocket();
        InetAddress addr = InetAddress.getByName(host);
        // build a proper protocol command so ServerSimulator can parse and reply
        byte[] payload = Protocol.buildCommand(Protocol.ADDR_CAMERA_VIS, (byte)0x01, (byte)0x01, new byte[]{0x01});
        DatagramPacket out = new DatagramPacket(payload, payload.length, addr, port);
        System.out.println("UdpPing: sending " + payload.length + " bytes to " + host + ":" + port);
        sock.send(out);

        // try receive reply
        byte[] buf = new byte[4096];
        DatagramPacket in = new DatagramPacket(buf, buf.length);
        sock.setSoTimeout(2000);
        try {
            sock.receive(in);
            System.out.println("UdpPing: received " + in.getLength() + " bytes from " + in.getAddress() + ":" + in.getPort());
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < in.getLength(); i++) sb.append(String.format("%02X ", buf[i]));
            System.out.println("Data: " + sb.toString());
        } catch (Exception e) {
            System.out.println("UdpPing: no reply (" + e.getMessage() + ")");
        }
        sock.close();
    }
}
