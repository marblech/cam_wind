#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

#include "cam_controller.h"

// Simple example demonstrating starting CamController in P2P or multicast mode.
// Usage:
//  ./cam_controller_example <port>           # point-to-point mode
//  ./cam_controller_example <port> <mcast>   # multicast mode (e.g. 239.255.43.21)

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [mcast_group]\n";
        return 1;
    }
    int port = std::atoi(argv[1]);
    const char* mcast = nullptr;
    if (argc >= 3) mcast = argv[2];

    CamController* c = cam_controller_create();
    if (!c) {
        std::cerr << "Failed to create CamController\n";
        return 1;
    }

    int r = cam_controller_start_ex(c, port, mcast);
    if (r != 0) {
        std::cerr << "cam_controller_start_ex failed: " << r << "\n";
        cam_controller_destroy(c);
        return 1;
    }

    std::cerr << "Started listener on port " << port;
    if (mcast) std::cerr << " (multicast group=" << mcast << ")";
    std::cerr << "\n";

    // run for a short period, printing last packet length and PTZ every second
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint8_t buf[2048];
        int n = cam_controller_get_last(c, buf, sizeof(buf));
        std::cerr << "Iteration " << i << ": last packet len=" << n << "\n";
        float az=0, el=0, ir=0, vis=0;
        if (cam_controller_get_ptz(c, &az, &el, &ir, &vis)) {
            std::cerr << " PTZ az=" << az << " el=" << el << " ir=" << ir << " vis=" << vis << "\n";
        }
    }

    cam_controller_stop(c);
    cam_controller_destroy(c);
    return 0;
}
