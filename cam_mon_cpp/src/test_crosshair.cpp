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

    // Up: tracker addr, func = crosshair show, control = move up
    int r = cammon_send_packet(host, port, ADDR_TRACK_VIS, static_cast<uint8_t>(TRACKER_FUNC_CROSSHAIR_SHOW), static_cast<uint8_t>(TRACKER_CTRL_UP), payload, 15, resp, sizeof(resp), 1000);
    std::cout << "Up result: " << r << std::endl;

    // Down
    r = cammon_send_packet(host, port, ADDR_TRACK_VIS, static_cast<uint8_t>(TRACKER_FUNC_CROSSHAIR_SHOW), static_cast<uint8_t>(TRACKER_CTRL_DOWN), payload, 15, resp, sizeof(resp), 1000);
    std::cout << "Down result: " << r << std::endl;

    // Left
    r = cammon_send_packet(host, port, ADDR_TRACK_VIS, static_cast<uint8_t>(TRACKER_FUNC_CROSSHAIR_SHOW), static_cast<uint8_t>(TRACKER_CTRL_LEFT), payload, 15, resp, sizeof(resp), 1000);
    std::cout << "Left result: " << r << std::endl;

    // Right
    r = cammon_send_packet(host, port, ADDR_TRACK_VIS, static_cast<uint8_t>(TRACKER_FUNC_CROSSHAIR_SHOW), static_cast<uint8_t>(TRACKER_CTRL_RIGHT), payload, 15, resp, sizeof(resp), 1000);
    std::cout << "Right result: " << r << std::endl;

    return 0;
}
