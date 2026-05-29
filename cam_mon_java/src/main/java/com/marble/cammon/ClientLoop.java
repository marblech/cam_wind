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
                    int rc = CamMonNative.setPTZ(host, port, 123.45f, 10.0f, 1.5f, 0.5f, 100, 1, 1, 0x01, 0x02, 2000);
                    if (rc <= 0) {
                        timeouts++;
                        log.println("No response, rc=" + rc);
                    } else {
                        success++;
                        log.println("Reply bytes: " + rc);
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

}
