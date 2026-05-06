package com.marble.cammon;

import java.net.DatagramPacket;
import java.net.DatagramSocket;

public class ServerSimulator {
    public static void main(String[] args) throws Exception {
        int port = 4001;
        if (args.length > 0) port = Integer.parseInt(args[0]);
        DatagramSocket sock = new DatagramSocket(port);
        System.out.println("Java simulator listening on UDP port " + port);
        byte[] buf = new byte[4096];
        while (true) {
            DatagramPacket p = new DatagramPacket(buf, buf.length);
            sock.receive(p);
            byte[] data = new byte[p.getLength()];
            System.arraycopy(p.getData(), p.getOffset(), data, 0, p.getLength());

            // Try servo frame first (0x7E header)
            Protocol.ServoPacket servo = Protocol.ServoPacket.parseServo(data);
            if (servo != null) {
                System.out.println(String.format("Sim: got servo seq=%d control=0x%02X az=%f el=%f", servo.seq, servo.control, servo.azimuth, servo.elevation));
                // Echo back the servo packet as ACK
                byte[] resp = servo.serializeServo();
                DatagramPacket out = new DatagramPacket(resp, resp.length, p.getAddress(), p.getPort());
                sock.send(out);
                continue;
            }

            // Try standard camera frame (0x0F F0)
            Protocol.Packet pkt = Protocol.parsePacket(data);
            if (pkt == null) {
                System.err.println("Received invalid packet");
                continue;
            }
            System.out.println(String.format("Sim: got addr=0x%02X func=0x%02X ctrl=0x%02X payload_len=%d", pkt.addr, pkt.func, pkt.ctrl, pkt.data.length));
            byte respFunc = (byte)(0x80 | pkt.func);
            byte[] resp = Protocol.buildCommand(pkt.addr, respFunc, pkt.ctrl, new byte[]{0x00});
            DatagramPacket out = new DatagramPacket(resp, resp.length, p.getAddress(), p.getPort());
            sock.send(out);
        }
    }
}
