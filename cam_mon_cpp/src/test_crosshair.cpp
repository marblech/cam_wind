#include <iostream>
#include <vector>
#include <memory>
#include <cstring>
#include "cammon_api.h"

int main(int argc, char** argv) {
    const char* host = "127.0.0.1";
    int port = 4000;

    uint8_t resp[2048];

    // payload: move amount 0x0A
    uint8_t payload[15] = {0};
    payload[0] = 0x0A;

    // Up: addr=0x00 func=0x01 ctrl=0x02
    int r = cammon_send_packet(host, port, 0x00, 0x01, 0x02, payload, 15, resp, sizeof(resp), 1000);
    std::cout << "Up result: " << r << std::endl;

    // Down: ctrl=0x03
    r = cammon_send_packet(host, port, 0x00, 0x01, 0x03, payload, 15, resp, sizeof(resp), 1000);
    std::cout << "Down result: " << r << std::endl;

    // Left: ctrl=0x04
    r = cammon_send_packet(host, port, 0x00, 0x01, 0x04, payload, 15, resp, sizeof(resp), 1000);
    std::cout << "Left result: " << r << std::endl;

    // Right: ctrl=0x05
    r = cammon_send_packet(host, port, 0x00, 0x01, 0x05, payload, 15, resp, sizeof(resp), 1000);
    std::cout << "Right result: " << r << std::endl;

    return 0;
}
