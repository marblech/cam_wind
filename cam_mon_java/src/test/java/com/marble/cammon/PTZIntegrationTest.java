package com.marble.cammon;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class PTZIntegrationTest {
    @Test
    public void ptzRoundtrip() throws Exception {
        // start Java server simulator as separate process
        ProcessBuilder pb = new ProcessBuilder("java", "-cp", "target/classes", "com.marble.cammon.ServerSimulator", "4001");
        pb.redirectErrorStream(true);
        Process server = pb.start();
        Thread.sleep(500);

        // start native listener on port 5001
        boolean started = CamMonNative.startStatusListener(5001);
        assertTrue(started, "Failed to start native status listener");

        // craft a status packet with floats in little-endian at the expected offsets
        byte[] pkt = new byte[51];
        pkt[0] = 0x1F;
        pkt[1] = (byte)0xF1;
        float irFocus = 12.34f;
        float visFocus = 56.78f;
        float az = 11.0f;
        float el = -2.5f;
        ByteBuffer bb = ByteBuffer.wrap(pkt).order(ByteOrder.LITTLE_ENDIAN);
        // ir_focus at 6
        bb.position(6); bb.putFloat(irFocus);
        // vis_focus at 10
        bb.position(10); bb.putFloat(visFocus);
        // servo az at 28
        bb.position(28); bb.putFloat(az);
        // servo el at 32
        bb.position(32); bb.putFloat(el);

        // send to native listener
        DatagramSocket s = new DatagramSocket();
        InetAddress addr = InetAddress.getByName("127.0.0.1");
        DatagramPacket dp = new DatagramPacket(pkt, pkt.length, addr, 5001);
        s.send(dp);
        s.close();

        // give native listener time to receive and parse
        Thread.sleep(200);

        PTZStatus st = CamMonNative.getPTZ();
        assertNotNull(st, "getPTZ returned null");
        assertEquals(az, st.az, 0.001);
        assertEquals(el, st.el, 0.001);
        assertEquals(irFocus, st.irFocus, 0.001);
        assertEquals(visFocus, st.visFocus, 0.001);

        // Now call setPTZ to send servo command to server and expect reply
        int rc = CamMonNative.setPTZ("127.0.0.1", 4001, 45.0f, 10.0f, 1.0f, 1.0f, 100, 1, 1, 0x01, 0x02, 1000);
        assertTrue(rc > 0, "setPTZ did not get positive reply");

        // cleanup
        CamMonNative.stopStatusListener();
        server.destroyForcibly();
    }
}
