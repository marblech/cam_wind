package com.marble.cammon;

public class TestServoApi {
    public static void main(String[] args) throws Exception {
        Protocol.ServoPacket s = new Protocol.ServoPacket();
        s.seq = 1;
        s.deviceType = 1;
        s.packetType = 2;
        s.deviceIp = 0;
        s.reservedConn = 0;
        s.control = 0x11;
        s.reservedCtrl = 0;
        s.azimuth = 123.45f;
        s.elevation = 10.0f;
        s.azSpeed = 1.5f;
        s.elSpeed = 0.5f;
        s.targetDistance = 100;
        s.reserved = new byte[33];
        s.reservedHorz = 0;
        s.reservedVert = 0;
        s.timestamp = 0;
        s.reservedState = 0;
        s.backup = 0;
        byte[] out = s.serializeServo();
        System.out.println("Serialized servo length=" + out.length);
        Protocol.ServoPacket parsed = Protocol.ServoPacket.parseServo(out);
        if (parsed == null) {
            System.err.println("parse failed");
            System.exit(2);
        }
        if (Math.abs(parsed.azimuth - s.azimuth) > 1e-5) { System.err.println("az mismatch"); System.exit(3); }
        if (Math.abs(parsed.elevation - s.elevation) > 1e-5) { System.err.println("el mismatch"); System.exit(4); }
        if (Math.abs(parsed.azSpeed - s.azSpeed) > 1e-5) { System.err.println("azs mismatch"); System.exit(5); }
        if (Math.abs(parsed.elSpeed - s.elSpeed) > 1e-5) { System.err.println("els mismatch"); System.exit(6); }
        if (parsed.targetDistance != s.targetDistance) { System.err.println("dist mismatch"); System.exit(7); }
        System.out.println("Servo API test passed");
    }
}
