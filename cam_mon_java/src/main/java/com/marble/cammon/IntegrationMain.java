package com.marble.cammon;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * IntegrationMain - 直接通过 main 方法运行的集成测试入口（不依赖 JUnit）。
 *
 * Usage: java com.marble.cammon.IntegrationMain [simPort]
 * Exits with code 0 on success, non-zero on failure.
 */
public class IntegrationMain {
    public static void main(String[] args) throws Exception {
        int simPort = 4001;
        String mcast = null;
        if (args.length > 0) simPort = Integer.parseInt(args[0]);
        if (args.length > 1) mcast = args[1];

        ProcessBuilder pb = new ProcessBuilder("java", "-cp", "target/classes", "com.marble.cammon.ServerSimulator", Integer.toString(simPort));
        pb.redirectErrorStream(true);
        Process server = pb.start();
        System.out.println("Started ServerSimulator on port " + simPort);
        try {
            Thread.sleep(500);

            // start native listener on port 5001 (use multicast group if provided)
            boolean started = (mcast == null) ? CamMonNative.startStatusListener(5001) : CamMonNative.startStatusListener(5001, mcast);
            if (!started) {
                System.err.println("Failed to start native status listener");
                server.destroyForcibly();
                System.exit(2);
            }

            // craft a status packet with floats in little-endian at the expected offsets
            byte[] pkt = new byte[51];
            pkt[0] = 0x1F;
            pkt[1] = (byte)0xF1;
            float irFocus = 12.34f;
            float visFocus = 56.78f;
            float az = 11.0f;
            float el = -2.5f;
            ByteBuffer bb = ByteBuffer.wrap(pkt).order(ByteOrder.LITTLE_ENDIAN);
            bb.position(6); bb.putFloat(irFocus);
            bb.position(10); bb.putFloat(visFocus);
            bb.position(28); bb.putFloat(az);
            bb.position(32); bb.putFloat(el);

            DatagramSocket s = new DatagramSocket();
            InetAddress addr = InetAddress.getByName(mcast == null ? "127.0.0.1" : mcast);
            DatagramPacket dp = new DatagramPacket(pkt, pkt.length, addr, 5001);
            s.send(dp);
            s.close();

            Thread.sleep(200);

            PTZStatus st = CamMonNative.getPTZ();
            if (st == null) {
                System.err.println("getPTZ returned null");
                CamMonNative.stopStatusListener();
                server.destroyForcibly();
                System.exit(3);
            }
            System.out.println(String.format("PTZ from native: az=%f el=%f ir=%f vis=%f", st.az, st.el, st.irFocus, st.visFocus));

            int rc = CamMonNative.setPTZ("127.0.0.1", simPort, 45.0f, 10.0f, 1.0f, 1.0f, 100, 1, 1, 0x01, 0x02, 1000);
            if (rc <= 0) {
                System.err.println("setPTZ did not get positive reply, rc=" + rc);
                CamMonNative.stopStatusListener();
                server.destroyForcibly();
                System.exit(4);
            }
            System.out.println("setPTZ reply byteCount=" + rc);

            CamMonNative.stopStatusListener();
            server.destroyForcibly();
            System.out.println("IntegrationMain completed successfully.");
            System.exit(0);
        } catch (Exception e) {
            System.err.println("IntegrationMain failed: " + e.getMessage());
            server.destroyForcibly();
            System.exit(10);
        }
    }
}
