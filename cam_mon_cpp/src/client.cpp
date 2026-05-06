#include "protocol.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <vector>

using namespace cammon;

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    int port = 4000;
    if (argc > 1) host = argv[1];
    if (argc > 2) port = atoi(argv[2]);
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    inet_pton(AF_INET, host, &serv.sin_addr);

    // mode: if argv[3] == "servo" send a servo control packet
    bool use_servo = false;
    if (argc > 3 && std::string(argv[3]) == "servo") use_servo = true;

    if (!use_servo) {
        // build a sample visible-camera command: e.g., function 0x01 control 0x01 with small payload
        std::vector<uint8_t> payload(15, 0x00);
        payload[0] = 5; // example: continuous zoom speed
        Packet p = make_camera_command(/*func=*/0x01, /*ctrl=*/0x01, payload);
        auto out = p.serialize();
        ssize_t sent = sendto(sock, out.data(), out.size(), 0, (sockaddr*)&serv, sizeof(serv));
        if (sent < 0) { perror("sendto"); return 1; }
        std::cout << "Client: sent " << sent << " bytes\n";
        uint8_t buf[4096];
        sockaddr_in peer{}; socklen_t plen=sizeof(peer);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&peer, &plen);
        if (n <= 0) { std::cerr << "no response\n"; return 1; }
        std::vector<uint8_t> in(buf, buf + n);
        auto resp = Packet::deserialize(in);
        if (!resp) { std::cerr << "invalid response\n"; return 1; }
        std::cout << "Client: received func=0x" << std::hex << int(resp->func) << std::dec << " addr=0x" << std::hex << int(resp->addr) << std::dec << " payload_len=" << resp->data.size() << "\n";
    } else {
        // build a servo control packet
        ServoPacket s;
        s.header = 0x7E;
        s.frame_len = 0x48;
        s.seq = 0x01;
        s.device_type = 0x01; // example
        s.packet_type = 0x02; // fixed-point report type
        s.device_ip = 0x00;
        s.reserved_conn = 0x00;
        s.control = 0x11; // example: position flag
        s.reserved_ctrl = 0x00;
        s.azimuth = 123.45f;
        s.elevation = 10.0f;
        s.az_speed = 1.5f;
        s.el_speed = 0.5f;
        s.target_distance = 100;
        s.reserved = std::vector<uint8_t>(33,0);
        s.reserved_horz = 0;
        s.reserved_vert = 0;
        s.timestamp = 0;
        s.reserved_state = 0;
        s.backup = 0;
        auto out = s.serialize_servo();
        ssize_t sent = sendto(sock, out.data(), out.size(), 0, (sockaddr*)&serv, sizeof(serv));
        if (sent < 0) { perror("sendto"); return 1; }
        std::cout << "Client: sent servo " << sent << " bytes\n";
        uint8_t buf[4096];
        sockaddr_in peer{}; socklen_t plen=sizeof(peer);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&peer, &plen);
        if (n <= 0) { std::cerr << "no response\n"; return 1; }
        std::vector<uint8_t> in(buf, buf + n);
        auto resp = ServoPacket::deserialize_servo(in);
        if (!resp) { std::cerr << "invalid servo response\n"; return 1; }
        std::cout << "Client: received servo seq=" << int(resp->seq) << " az=" << resp->azimuth << " el=" << resp->elevation << "\n";
    }
    close(sock);
    return 0;
}
