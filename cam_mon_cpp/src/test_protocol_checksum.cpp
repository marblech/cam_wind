#include "protocol.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>

using namespace cammon;

static bool eq_vec(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) return false;
    return true;
}

static void print_hex(const std::vector<uint8_t>& v) {
    for (size_t i = 0; i < v.size(); ++i) {
        std::cout << std::hex << std::setfill('0') << std::setw(2) << int(v[i]) << " ";
    }
    std::cout << std::dec << std::endl;
}

int main() {
    // 1) checksum simple test
    {
        std::vector<uint8_t> v = {1,2,3,4,5};
        uint8_t cs = compute_checksum(v);
        uint8_t expected = 1+2+3+4+5;
        if (cs != expected) {
            std::cerr << "compute_checksum mismatch: got " << int(cs) << " expected " << int(expected) << "\n";
            return 1;
        }
    }

    // 2) Packet serialize/deserialize (debug)
    {
        Packet p;
        p.addr = ADDR_CAMERA_VIS;
        p.func = 0x01;
        p.ctrl = 0x02;
        p.data = {0x11, 0x22};
        auto bytes = p.serialize();
        std::cout << "Serialized Packet bytes (" << bytes.size() << "): ";
        print_hex(bytes);
        // attempt deserialize directly
        auto p2_opt = Packet::deserialize(bytes);
        if (!p2_opt.has_value()) {
            std::cerr << "Packet deserialize failed\n";
            // try to debug checksum calculation
            std::vector<uint8_t> csrange;
            csrange.push_back(bytes[2]); // addr
            csrange.push_back(bytes[3]); // func
            csrange.push_back(bytes[4]); // ctrl
            for (size_t i = 5; i + 3 < bytes.size(); ++i) { // until checksum position -1
                // find checksum index = size-3
            }
            size_t cs_idx = bytes.size() - 3;
            std::cout << "Checksum byte in frame: " << std::hex << int(bytes[cs_idx]) << std::dec << " at index " << cs_idx << std::endl;
            std::cout << "Address,func,ctrl: " << int(bytes[2]) << "," << int(bytes[3]) << "," << int(bytes[4]) << std::endl;
            std::vector<uint8_t> csrange2;
            csrange2.push_back(bytes[2]);
            csrange2.push_back(bytes[3]);
            csrange2.push_back(bytes[4]);
            for (size_t i = 5; i < cs_idx; ++i) csrange2.push_back(bytes[i]);
            uint8_t cs_calc = compute_checksum(csrange2);
            std::cout << "Calculated checksum (sum mod256): " << std::hex << int(cs_calc) << std::dec << std::endl;
            std::cout << "csrange length: " << csrange2.size() << std::endl;
            return 2;
        }
        Packet p2 = p2_opt.value();
        if (p2.addr != p.addr || p2.func != p.func || p2.ctrl != p.ctrl) {
            std::cerr << "Packet header fields mismatch\n";
            return 3;
        }
        // original data should be prefix of parsed data
        if (p2.data.size() < p.data.size()) {
            std::cerr << "Parsed data too short\n";
            return 4;
        }
        for (size_t i = 0; i < p.data.size(); ++i) {
            if (p2.data[i] != p.data[i]) {
                std::cerr << "Packet data mismatch at " << i << "\n";
                return 5;
            }
        }
    }

    // 3) Servo packet serialize/deserialize
    {
        auto buf = build_servo_packet(10.5f, -5.25f, 1.0f, -1.0f, 1500, 0x12, 0x11, 0x01, 0x02);
        auto s_opt = ServoPacket::deserialize_servo(buf);
        if (!s_opt.has_value()) {
            std::cerr << "ServoPacket deserialize failed\n";
            return 6;
        }
        ServoPacket s = s_opt.value();
        // verify some fields
        if (std::abs(s.azimuth - 10.5f) > 1e-4f) { std::cerr << "Servo azimuth mismatch\n"; return 7; }
        if (s.target_distance != 1500) { std::cerr << "Servo target_distance mismatch\n"; return 8; }
        // checksum validated by deserialize_servo; if returned, checksum ok
    }

    // 4) LoadStatusReport serialize/deserialize
    {
        LoadStatusReport r;
        r.addr = REPORT_ADDR_LOAD_STATUS_VIS;
        r.frame_seq = 0x1234;
        r.servo_work_mode = 0x9;
        r.ir_focal_length = 45.0f;
        r.vis_focal_length = 200.0f;
        r.servo_azimuth = 12.34f;
        r.servo_elevation = -2.5f;
        r.self_check_status = 0x00;
        r.vis_cam_image_status = 0x00;
        r.memory_track_time = 500;
        auto buf = r.serialize();
        std::cout << "Serialized LoadStatusReport (" << buf.size() << "): ";
        print_hex(buf);
        auto r2_opt = LoadStatusReport::deserialize(buf);
        if (!r2_opt.has_value()) {
            std::cerr << "LoadStatusReport deserialize failed\n";
            return 9;
        }
        auto r2 = r2_opt.value();
        if (r2.frame_seq != r.frame_seq) { std::cerr << "LoadStatusReport frame_seq mismatch\n"; return 10; }
        if (std::abs(r2.ir_focal_length - r.ir_focal_length) > 1e-4f) { std::cerr << "ir_focal_length mismatch\n"; return 11; }
        if (r2.memory_track_time != r.memory_track_time) { std::cerr << "memory_track_time mismatch\n"; return 12; }
    }

    std::cout << "All protocol checksum tests passed\n";
    return 0;
}