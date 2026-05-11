package com.marble.cammon;

import java.util.Arrays;

public class Protocol {
    // Protocol constants
    public static final byte HDR1 = 0x0F;
    public static final byte HDR2 = (byte)0xF0;
    public static final byte TAIL1 = (byte)0xF0;
    public static final byte TAIL2 = 0x0F;
    public static final int DATA_LEN_DEFAULT = 15;

    // device addresses
    public static final byte ADDR_TRACK_VIS = 0x00;
    public static final byte ADDR_TRACK_IR  = 0x01;
    public static final byte ADDR_CAMERA_VIS = 0x02;
    public static final byte ADDR_CAMERA_IR  = 0x03;
    public static final byte ADDR_ENV_CTRL   = 0x04;
    public static final byte ADDR_SERVO      = 0x05;

    // --- Visible camera functions (Protocol v0.07 table mapping)
    public static final byte VC_FUNC_CONTINUOUS_ZOOM = 0x01;
    public static final byte VC_FUNC_STEP_ZOOM = 0x02;
    public static final byte VC_FUNC_CONTINUOUS_FOCUS = 0x03;
    public static final byte VC_FUNC_SINGLE_STEP_FOCUS = 0x04;
    public static final byte VC_FUNC_AUTO_FOCUS = 0x05;
    public static final byte VC_FUNC_FOCAL_DIRECT = 0x06;
    public static final byte VC_FUNC_FOCUS_DIRECT = 0x07;
    public static final byte VC_FUNC_DEFOG = 0x08;
    public static final byte VC_FUNC_BRIGHTNESS = 0x09;
    public static final byte VC_FUNC_CONTRAST = 0x0A;
    public static final byte VC_FUNC_IMAGE_ENHANCE = 0x0B;

    // Camera control semantics (common values)
    public static final byte CAM_CTRL_STOP = 0x00;
    public static final byte CAM_CTRL_CONT_PLUS = 0x01;
    public static final byte CAM_CTRL_CONT_MINUS = 0x02;
    public static final byte CAM_CTRL_ZOOM = 0x05;
    public static final byte CAM_CTRL_FOCAL = 0x06;

    // Tracker functions and controls
    public static final byte TRACKER_FUNC_DISPLAY_SWITCH = 0x00;
    public static final byte TRACKER_FUNC_CROSSHAIR_SHOW = 0x01;
    public static final byte TRACKER_FUNC_CROSSHAIR_UP = 0x02;
    public static final byte TRACKER_FUNC_CROSSHAIR_DOWN = 0x03;
    public static final byte TRACKER_FUNC_CROSSHAIR_LEFT = 0x04;
    public static final byte TRACKER_FUNC_CROSSHAIR_RIGHT = 0x05;

    public static final byte TRACKER_CTRL_SHOW = 0x01;
    public static final byte TRACKER_CTRL_UP = 0x02;
    public static final byte TRACKER_CTRL_DOWN = 0x03;
    public static final byte TRACKER_CTRL_LEFT = 0x04;
    public static final byte TRACKER_CTRL_RIGHT = 0x05;

    // Detection / Recognition
    public static final byte DETECT_FUNC_POINT_TARGET = 0x04;
    public static final byte DETECT_FUNC_SENSITIVITY_SET = 0x02;
    public static final byte DETECT_FUNC_DETECT_TO_TRACK_ON = 0x03;

    public static final byte RECOG_FUNC_ENABLE = 0x00;
    public static final byte RECOG_FUNC_AREA_KIND_TIME_SET = 0x01;
    public static final byte RECOG_FUNC_SENSITIVITY_SET = 0x02;
    public static final byte RECOG_FUNC_RECOG_TO_TRACK_ON = 0x03;

    // Identification category bitmask
    public static final byte ID_CAT_PERSON = 1 << 0;
    public static final byte ID_CAT_VEHICLE = 1 << 1;
    public static final byte ID_CAT_SHIP = 1 << 2;
    public static final byte ID_CAT_AIRCRAFT = 1 << 3;
    public static final byte ID_CAT_DRONE = 1 << 4;

    // Environment control
    public static final byte ENV_FUNC_WIPER = 0x01;
    public static final byte ENV_FUNC_HEATER = 0x02;

    // Scan functions
    public static final byte SCAN_FUNC_SET_MODE = 0x01;
    public static final byte SCAN_FUNC_PRESET_WRITE = 0x02;
    public static final byte SCAN_FUNC_PRESET_READ = 0x02;

    // Servo constants
    public static final byte SERVO_CTRL_POSITION = 0x11;
    public static final byte SERVO_DEVICE_TYPE = 0x01;
    public static final byte SERVO_PACKET_TYPE_POINT = 0x02;
    public static final byte DEFAULT_SEQ = 0x01;
    public static final byte DEFAULT_MOVE_AMOUNT = 0x0A;

    public static byte computeChecksum(byte[] data) {
        int sum = 0;
        for (byte b : data) sum = (sum + (b & 0xFF)) & 0xFF;
        return (byte)sum;
    }

    public static byte[] buildCommand(byte addr, byte func, byte ctrl, byte[] payload) {
        int dataLen = Math.max(DATA_LEN_DEFAULT, payload == null ? 0 : payload.length);
        int total = 2 + 1 + 1 + 1 + dataLen + 1 + 2; // hdr(2)+addr+func+ctrl+data+cs+tail(2)
        byte[] out = new byte[total];
        int idx = 0;
        out[idx++] = HDR1; out[idx++] = HDR2;
        out[idx++] = addr; out[idx++] = func; out[idx++] = ctrl;
        if (payload != null) {
            System.arraycopy(payload, 0, out, idx, payload.length);
            idx += payload.length;
        }
        // pad remaining data with 0
        int pad = dataLen - (payload == null ? 0 : payload.length);
        for (int i=0;i<pad;i++) out[idx++] = 0x00;
        // checksum over addr..data
        int csStart = 2; // addr position
        int csEnd = 2 + 1 + 1 + dataLen; // exclusive of checksum
        byte[] csrange = Arrays.copyOfRange(out, csStart, csEnd);
        byte cs = computeChecksum(csrange);
        out[idx++] = cs;
        out[idx++] = TAIL1; out[idx++] = TAIL2;
        return out;
    }

    public static class Packet {
        public byte addr, func, ctrl;
        public byte[] data;
        public byte checksum;
    }

    public static Packet parsePacket(byte[] buf) {
        if (buf.length < 23) return null;
        if (buf[0] != HDR1 || buf[1] != HDR2) return null;
        if (buf[buf.length-2] != TAIL1 || buf[buf.length-1] != TAIL2) return null;
        int idx = 2;
        Packet p = new Packet();
        p.addr = buf[idx++]; p.func = buf[idx++]; p.ctrl = buf[idx++];
        int csIdx = buf.length - 3;
        p.data = Arrays.copyOfRange(buf, idx, csIdx);
        p.checksum = buf[csIdx];
        // verify checksum
        byte[] csrange = Arrays.copyOfRange(buf, 2, csIdx);
        if (computeChecksum(csrange) != p.checksum) return null;
        return p;
    }

    // --- Servo frame support ---
    public static class ServoPacket {
        public byte header; // 0x7E
        public byte frameLen; // 0x48
        public byte seq;
        public byte deviceType;
        public byte packetType;
        public byte deviceIp;
        public byte reservedConn;
        public byte control;
        public byte reservedCtrl;
        public float azimuth;
        public float elevation;
        public float azSpeed;
        public float elSpeed;
        public int targetDistance; // uint16
        public byte[] reserved = new byte[33];
        public int reservedHorz; // uint16
        public int reservedVert; // uint16
        public long timestamp; // uint32
        public byte reservedState;
        public int backup; // uint16
        public byte checksum;

        public byte[] serializeServo() {
            int total = 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 4*4 + 2 + 33 + 2 + 2 + 4 + 1 + 2 + 1;
            byte[] out = new byte[total];
            int idx = 0;
            out[idx++] = (byte)0x7E;
            out[idx++] = (byte)0x48;
            out[idx++] = seq;
            out[idx++] = deviceType;
            out[idx++] = packetType;
            out[idx++] = deviceIp;
            out[idx++] = reservedConn;
            out[idx++] = control;
            out[idx++] = reservedCtrl;
            // little-endian float write
            intToLeBytes(Float.floatToIntBits(azimuth), out, idx); idx +=4;
            intToLeBytes(Float.floatToIntBits(elevation), out, idx); idx +=4;
            intToLeBytes(Float.floatToIntBits(azSpeed), out, idx); idx +=4;
            intToLeBytes(Float.floatToIntBits(elSpeed), out, idx); idx +=4;
            shortToLeBytes((short)targetDistance, out, idx); idx +=2;
            if (reserved != null) {
                int rlen = Math.min(reserved.length,33);
                System.arraycopy(reserved,0,out,idx,rlen); idx += rlen;
                for (int i=rlen;i<33;i++) out[idx++] = 0x00;
            } else {
                for (int i=0;i<33;i++) out[idx++] = 0x00;
            }
            shortToLeBytes((short)reservedHorz, out, idx); idx +=2;
            shortToLeBytes((short)reservedVert, out, idx); idx +=2;
            intToLeBytes((int)timestamp, out, idx); idx +=4;
            out[idx++] = reservedState;
            shortToLeBytes((short)backup, out, idx); idx +=2;
            // checksum: XOR of bytes from index 0 to 70 (include frame header 0x7E)
            byte cs = 0;
            for (int i=0;i<71;i++) cs ^= out[i];
            out[idx++] = cs;
            return out;
        }

        private static void intToLeBytes(int v, byte[] out, int idx) {
            out[idx] = (byte)(v & 0xFF);
            out[idx+1] = (byte)((v>>8) & 0xFF);
            out[idx+2] = (byte)((v>>16) & 0xFF);
            out[idx+3] = (byte)((v>>24) & 0xFF);
        }
        private static void shortToLeBytes(short v, byte[] out, int idx) {
            out[idx] = (byte)(v & 0xFF);
            out[idx+1] = (byte)((v>>8) & 0xFF);
        }

        public static ServoPacket parseServo(byte[] buf) {
            if (buf.length < 72) return null;
            if ((buf[0] & 0xFF) != 0x7E) return null;
            ServoPacket p = new ServoPacket();
            int idx = 0;
            p.header = buf[idx++];
            p.frameLen = buf[idx++];
            p.seq = buf[idx++];
            p.deviceType = buf[idx++];
            p.packetType = buf[idx++];
            p.deviceIp = buf[idx++];
            p.reservedConn = buf[idx++];
            p.control = buf[idx++];
            p.reservedCtrl = buf[idx++];
            p.azimuth = Float.intBitsToFloat(leBytesToInt(buf, idx)); idx +=4;
            p.elevation = Float.intBitsToFloat(leBytesToInt(buf, idx)); idx +=4;
            p.azSpeed = Float.intBitsToFloat(leBytesToInt(buf, idx)); idx +=4;
            p.elSpeed = Float.intBitsToFloat(leBytesToInt(buf, idx)); idx +=4;
            p.targetDistance = leBytesToShort(buf, idx); idx +=2;
            p.reserved = new byte[33]; System.arraycopy(buf, idx, p.reserved, 0, 33); idx +=33;
            p.reservedHorz = leBytesToShort(buf, idx); idx +=2;
            p.reservedVert = leBytesToShort(buf, idx); idx +=2;
            p.timestamp = leBytesToInt(buf, idx); idx +=4;
            p.reservedState = buf[idx++];
            p.backup = leBytesToShort(buf, idx); idx +=2;
            p.checksum = buf[idx++];
            // verify checksum (include frame header 0x7E)
            byte cs = 0; for (int i=0;i<71;i++) cs ^= buf[i];
            if (cs != p.checksum) return null;
            return p;
        }

        private static int leBytesToInt(byte[] b, int idx) {
            return (b[idx]&0xFF) | ((b[idx+1]&0xFF)<<8) | ((b[idx+2]&0xFF)<<16) | ((b[idx+3]&0xFF)<<24);
        }
        private static int leBytesToShort(byte[] b, int idx) {
            return (b[idx]&0xFF) | ((b[idx+1]&0xFF)<<8);
        }
    }
}
