#include "protocol.h"
#include "plog_init.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <vector>

using namespace cammon;

int main(int argc, char** argv) {
    initPlog();
    int port = 4000;
    if (argc > 1) port = atoi(argv[1]);
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { PLOG_ERROR << "socket failed"; return 1; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) { PLOG_ERROR << "bind failed"; return 1; }
    PLOG_INFO << "Simulator listening on UDP port " << port;
    while (1) {
        uint8_t buf[4096];
        sockaddr_in peer{}; socklen_t plen = sizeof(peer);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&peer, &plen);
        if (n <= 0) continue;
        std::vector<uint8_t> in(buf, buf + n);
        // try parsing as standard 0x0F F0 frame
        auto pkt = Packet::deserialize(in);
        if (pkt) {
            PLOG_INFO << "Sim: got addr=0x" << std::hex << int(pkt->addr) << std::dec << " func=0x" << std::hex << int(pkt->func) << std::dec << " ctrl=0x" << std::hex << int(pkt->ctrl) << std::dec << " payload_len=" << pkt->data.size();
            Packet resp;
            resp.addr = pkt->addr; // echo
            resp.func = uint8_t(0x80 | pkt->func);
            resp.ctrl = pkt->ctrl;
            resp.data = std::vector<uint8_t>(15, 0x00);
            resp.data[0] = 0x00; // status ok
            auto out = resp.serialize();
            sendto(sock, out.data(), out.size(), 0, (sockaddr*)&peer, plen);
            continue;
        }
        // try parsing as servo frame (header 0x7E)
        auto servo = ServoPacket::deserialize_servo(in);
        if (servo) {
            PLOG_INFO << "Sim: got servo seq=" << int(servo->seq) << " control=0x" << std::hex << int(servo->control) << std::dec << " az=" << servo->azimuth << " el=" << servo->elevation;
            // craft a simple servo ACK: mirror seq and set a small ack flag in reserved_state
            ServoPacket resp;
            resp.header = 0x7E;
            resp.frame_len = 0x48;
            resp.seq = servo->seq;
            resp.device_type = servo->device_type;
            resp.packet_type = servo->packet_type;
            resp.device_ip = servo->device_ip;
            resp.reserved_conn = 0x00;
            resp.control = servo->control;
            resp.reserved_ctrl = 0x00;
            resp.azimuth = servo->azimuth;
            resp.elevation = servo->elevation;
            resp.az_speed = servo->az_speed;
            resp.el_speed = servo->el_speed;
            resp.target_distance = servo->target_distance;
            resp.reserved = std::vector<uint8_t>(33,0);
            resp.reserved_horz = 0;
            resp.reserved_vert = 0;
            resp.timestamp = servo->timestamp;
            resp.reserved_state = 0x00;
            resp.backup = 0x0000;
            auto out = resp.serialize_servo();
            sendto(sock, out.data(), out.size(), 0, (sockaddr*)&peer, plen);
            continue;
        }
        PLOG_WARNING << "Received invalid packet (unknown format)";
    }
    close(sock);
    return 0;
}
