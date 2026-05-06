#include "protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

using namespace cammon;

static uint16_t read_le_u16(const std::vector<uint8_t>& buf, size_t idx) {
    return (uint16_t)buf[idx] | ((uint16_t)buf[idx+1] << 8);
}

static uint32_t read_le_u32(const std::vector<uint8_t>& buf, size_t idx) {
    return (uint32_t)buf[idx] | ((uint32_t)buf[idx+1] << 8) | ((uint32_t)buf[idx+2] << 16) | ((uint32_t)buf[idx+3] << 24);
}

static float read_le_float(const std::vector<uint8_t>& buf, size_t idx) {
    uint32_t v = read_le_u32(buf, idx);
    float f;
    std::memcpy(&f, &v, sizeof(float));
    return f;
}

// Parse the camera status report frame with header 0x1F 0xF1
// Frame layout (absolute offsets, per protocol):
// 0-1: header 0x1F 0xF1
// 2: address
// 3-4: frame seq (2 bytes, little-endian)
// 5-47: data (43 bytes)
// 48: checksum (XOR of address..data)
// 49-50: tail (0xF1 0x1F)

static bool parse_status_report(const std::vector<uint8_t>& buf) {
    if (buf.size() < 51) return false;
    if (buf[0] != 0x1F || buf[1] != 0xF1) return false;
    if (buf[49] != 0xF1 || buf[50] != 0x1F) return false;

    // compute checksum XOR of bytes 2..47
    uint8_t cs = 0;
    for (size_t i = 2; i <= 47 && i < buf.size(); ++i) cs ^= buf[i];
    if (cs != buf[48]) {
        std::cerr << "Status report checksum mismatch: computed=" << std::hex << (int)cs << " got=" << (int)buf[48] << std::dec << "\n";
        return false;
    }

    uint8_t addr = buf[2];
    uint16_t seq = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);

    // Extract some key fields per document Table 3-12 (absolute positions)
    // 红外焦距: bytes 6-9 (float)
    float ir_focus = read_le_float(buf, 6);
    // 白光焦距: bytes 10-13 (float)
    float vis_focus = read_le_float(buf, 10);
    // 伺服方位角: 28-31 (float)
    float servo_az = read_le_float(buf, 28);
    // 伺服俯仰角: 32-35 (float)
    float servo_el = read_le_float(buf, 32);
    // 自检/状态 byte at index 36
    uint8_t self_check = buf[36];
    // 可见光摄像机图像状态 at index 37
    uint8_t vis_image_state = buf[37];
    // 可见光摄像机通信状态 at 38
    uint8_t vis_comm_state = buf[38];
    // 温度 bytes at 39 and 40
    int8_t track_temp = (int8_t)buf[39];
    int8_t cam_temp = (int8_t)buf[40];

    std::cout << "--- Status Report Received ---\n";
    std::cout << "addr=" << (int)addr << " seq=" << seq << "\n";
    std::cout << "IR focus=" << ir_focus << " VIS focus=" << vis_focus << "\n";
    std::cout << "Servo az=" << servo_az << " el=" << servo_el << "\n";
    std::cout << "self_check=0x" << std::hex << (int)self_check << std::dec << " vis_image=" << (int)vis_image_state << " vis_comm=" << (int)vis_comm_state << "\n";
    std::cout << "track_temp=" << (int)track_temp << " cam_temp=" << (int)cam_temp << "\n";
    std::cout << "-------------------------------\n";
    return true;
}

int main(int argc, char** argv) {
    int port = 8080; // default per document
    if (argc > 1) port = std::atoi(argv[1]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    std::cout << "Listening UDP on port " << port << " for status reports...\n";

    while (true) {
        std::vector<uint8_t> buf(2048);
        sockaddr_in src;
        socklen_t slen = sizeof(src);
        ssize_t n = recvfrom(sock, buf.data(), buf.size(), 0, (struct sockaddr*)&src, &slen);
        if (n < 0) {
            perror("recvfrom");
            break;
        }
        buf.resize((size_t)n);
        char srcip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, srcip, sizeof(srcip));
        std::cout << "Received " << n << " bytes from " << srcip << ":" << ntohs(src.sin_port) << "\n";

        if (!parse_status_report(buf)) {
            // try parse as standard packet (0x0F 0xF0) using existing Packet parser
            auto p = Packet::deserialize(buf);
            if (p) {
                std::cout << "Parsed standard Packet: addr=" << (int)p->addr << " func=" << (int)p->func << " ctrl=" << (int)p->ctrl << " data_len=" << p->data.size() << "\n";
            } else {
                // try servo
                auto s = ServoPacket::deserialize_servo(buf);
                if (s) {
                    std::cout << "Parsed ServoPacket: az=" << s->azimuth << " el=" << s->elevation << "\n";
                } else {
                    std::cout << "Unknown/unsupported packet format\n";
                }
            }
        }
    }

    close(sock);
    return 0;
}
