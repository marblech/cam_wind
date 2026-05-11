package com.marble.cammon;

import java.io.FileWriter;
import java.io.PrintWriter;

public class ClientLoop {

    public static void main(String[] args) throws Exception {
        String host = "127.0.0.1";
        int port = 33333;
        int iterations = 500;
        if (args.length > 0) host = args[0];
        if (args.length > 1) port = Integer.parseInt(args[1]);
        if (args.length > 2) iterations = Integer.parseInt(args[2]);

        String logPath = "client_loop_log.txt";
        try (PrintWriter log = new PrintWriter(new FileWriter(logPath, true))) {
            int success = 0;
            int timeouts = 0;
            for (int i = 1; i <= iterations; i++) {
                log.printf("=== Iter %d ===\n", i);
                System.out.println("Iter " + i + " sending...");
                try {
                    byte[] resp = CamMonNative.sendServoCommand(host, port, 123.45f, 10.0f, 1.5f, 0.5f, 100, 1, 1, 0x01, 0x02, 2000);
                    if (resp == null) {
                        timeouts++;
                        log.println("No response (null)");
                    } else {
                        success++;
                        log.println("Resp bytes: " + bytesToHex(resp));
                        Protocol.ServoPacket p = Protocol.ServoPacket.parseServo(resp);
                        if (p != null) {
                            log.printf("Parsed servo seq=%d az=%f el=%f\n", p.seq, p.azimuth, p.elevation);
                        } else {
                            log.println("Could not parse servo response");
                        }
                    }
                } catch (Exception ex) {
                    timeouts++;
                    log.println("Exception: " + ex.getMessage());
                }
                log.flush();
                Thread.sleep(50);
            }
            log.println("=== Summary ===");
            log.printf("iterations=%d success=%d timeouts=%d\n", iterations, success, timeouts);
            System.out.println("Done. iterations=" + iterations + " success=" + success + " timeouts=" + timeouts);
        }
    }

    private static String bytesToHex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) sb.append(String.format("%02X ", b));
        return sb.toString();
    }
}
