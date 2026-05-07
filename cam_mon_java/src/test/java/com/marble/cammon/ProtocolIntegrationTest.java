package com.marble.cammon;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

import java.lang.ProcessBuilder;
import java.io.InputStream;

public class ProtocolIntegrationTest {
    @Test
    public void integrationRoundtrip() throws Exception {
        ProcessBuilder pb = new ProcessBuilder("java", "-Djava.library.path=../cam_mon_cpp/build", "-cp", "target/classes", "com.marble.cammon.ServerSimulator", "4001");
        pb.redirectErrorStream(true);
        Process p = pb.start();
        // give simulator time to start
        Thread.sleep(1000);
        // run client
        ProcessBuilder pc = new ProcessBuilder("java", "-Djava.library.path=../cam_mon_cpp/build", "-cp", "target/classes", "com.marble.cammon.Client", "127.0.0.1", "4001");
        Process pcproc = pc.start();
        int rc = pcproc.waitFor();
        // kill simulator
        p.destroyForcibly();
        assertEquals(0, rc, "Client returned non-zero");
    }
}
