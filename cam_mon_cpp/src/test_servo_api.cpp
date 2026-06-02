#include "protocol.h"
#include <vector>
#include <iostream>
#include <cmath>

using namespace cammon;

int main() {
    float az = 123.45f;
    float el = 10.0f;
    float azs = 1.5f;
    float els = 0.5f;
    uint16_t dist = 100;
    auto out = build_servo_packet(az, el, azs, els, dist, /*seq=*/1, /*control=*/0x11);
    std::cout << "Serialized servo length=" << out.size() << "\n";
    auto sp = ServoPacket::deserialize_servo(out);
    if (!sp) { std::cerr << "deserialize failed\n"; return 2; }
    // compare values with small epsilon
    if (std::fabs(sp->azimuth - az) > 1e-5) { std::cerr << "az mismatch: " << sp->azimuth << " vs " << az << "\n"; return 3; }
    if (std::fabs(sp->elevation - el) > 1e-5) { std::cerr << "el mismatch\n"; return 4; }
    if (std::fabs(sp->az_speed - azs) > 1e-5) { std::cerr << "azs mismatch\n"; return 5; }
    if (std::fabs(sp->el_speed - els) > 1e-5) { std::cerr << "els mismatch\n"; return 6; }
    if (sp->target_distance != dist) { std::cerr << "dist mismatch\n"; return 7; }
    std::cout << "Servo API test passed\n";
    return 0;
}
