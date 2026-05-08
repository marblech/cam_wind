package com.marble.cammon;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class CrosshairControlTest {

    // Start the Java UDP ServerSimulator in a background thread and send tracker commands
    private void startServer(int port) {
        Thread t = new Thread(() -> {
            try {
                ServerSimulator.main(new String[]{String.valueOf(port)});
            } catch (Exception e) {
                e.printStackTrace();
            }
        }, "ServerSimulatorThread");
        t.setDaemon(true);
        t.start();
        try { Thread.sleep(200); } catch (InterruptedException e) { }
    }

    @Test
    public void testCrosshairUpDownLeftRight() {
        int port = 4000;
        startServer(port);

        // payload: first byte = amount (0x0A)
        byte[] payload = new byte[15];
        payload[0] = 0x0A;

        // Up (ctrl 0x02)
        byte[] respUp = CamMonNative.sendPacket("127.0.0.1", port, (byte)0x00, (byte)0x01, (byte)0x02, payload, 1000);
        assertNotNull(respUp, "Up response should not be null");
        assertTrue(respUp.length >= 23, "Up response should have minimum length");

        // Down (ctrl 0x03)
        byte[] respDown = CamMonNative.sendPacket("127.0.0.1", port, (byte)0x00, (byte)0x01, (byte)0x03, payload, 1000);
        assertNotNull(respDown);

        // Left (ctrl 0x04)
        byte[] respLeft = CamMonNative.sendPacket("127.0.0.1", port, (byte)0x00, (byte)0x01, (byte)0x04, payload, 1000);
        assertNotNull(respLeft);

        // Right (ctrl 0x05)
        byte[] respRight = CamMonNative.sendPacket("127.0.0.1", port, (byte)0x00, (byte)0x01, (byte)0x05, payload, 1000);
        assertNotNull(respRight);
    }
}
