package com.marble.cammon;

public class IntegrationTestRunner {
    public static void main(String[] args) throws Exception {
        // start simulator in a thread
        Thread sim = new Thread(() -> {
            try {
                ServerSimulator.main(new String[]{"4002"});
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
        sim.setDaemon(true);
        sim.start();
        // give simulator time to start
        Thread.sleep(300);
        // run client via controller PTZ interface
        Client.main(new String[]{"127.0.0.1", "4002", "45.0", "30.0"});
        System.out.println("Integration run finished");
    }
}
