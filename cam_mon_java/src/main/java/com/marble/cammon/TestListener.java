package com.marble.cammon;

/**
 * Simple test launcher to start the native status listener and print received data.
 */
public class TestListener {
    public static void main(String[] args) throws Exception {
        int port = 23232;
        String group = null;
        if (args.length >= 1) {
            port = Integer.parseInt(args[0]);
        }
        if (args.length >= 2) {
            group = args[1];
        }

        System.out.println("Starting listener on port " + port + " group " + group);
        boolean started = CamMonNative.startStatusListener(port, group);
        System.out.println("startStatusListener returned: " + started);
        if (!started) {
            System.err.println("Failed to start listener. Check native library load and permissions.");
            return;
        }

        Runtime.getRuntime().addShutdownHook(new Thread(() -> {
            System.out.println("Shutdown hook: stopping listener");
            CamMonNative.stopStatusListener();
        }));

        // Poll for status for 60 seconds, printing any received packets / PTZ data
        for (int i = 0; i < 60; i++) {
            byte[] last = CamMonNative.getLastStatus();
            PTZStatus ptz = CamMonNative.getPTZ();
            long now = System.currentTimeMillis();
            if (last != null) {
                System.out.println(now + ": lastStatus len=" + last.length);
            } else {
                System.out.println(now + ": lastStatus=null");
            }
            if (ptz != null) {
                System.out.printf("PTZ: az=%.2f el=%.2f ir=%.2f vis=%.2f%n", ptz.az, ptz.el, ptz.irFocus, ptz.visFocus);
            } else {
                System.out.println("PTZ=null");
            }
            Thread.sleep(1000);
        }

        System.out.println("Finished polling, stopping listener.");
        CamMonNative.stopStatusListener();
    }
}
