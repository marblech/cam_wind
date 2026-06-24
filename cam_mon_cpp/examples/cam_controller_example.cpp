#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <csignal>

#include "cam_controller.h"

static volatile sig_atomic_t g_stop = 0;
static void sigint_handler(int /*sig*/) {
    g_stop = 1;
}

// Simple example demonstrating starting CamController in P2P or multicast mode.
// Usage:
//  ./cam_controller_example <port>           # point-to-point mode
//  ./cam_controller_example <port> <mcast>   # multicast mode (e.g. 239.255.43.21)

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [mcast_group]\n";
        std::cerr << "   or: " << argv[0] << " -c <config_file>    # use config file (INI)\n";
        return 1;
    }

    const char* mcast = nullptr;
    CamController* c = cam_controller_create();
    if (!c) {
        std::cerr << "Failed to create CamController\n";
        return 1;
    }

    std::signal(SIGINT, sigint_handler);
    std::signal(SIGTERM, sigint_handler);

    int r = 0;

    // If first arg is -c, use configuration file to start (p2p.listen_port in INI)
    if (std::strcmp(argv[1], "-c") == 0) {
        if (argc < 3) {
            std::cerr << "Missing config file after -c\n";
            cam_controller_destroy(c);
            return 1;
        }
        const char* config_file = argv[2];
        if (argc >= 4) mcast = argv[3];
        std::cerr << "Starting using config file: " << config_file << "\n";
        r = cam_controller_start_with_config(c, config_file, mcast);
        if (r != 0) {
            std::cerr << "cam_controller_start_with_config failed: " << r << "\n";
            cam_controller_destroy(c);
            return 1;
        }
        std::cerr << "Started listener using config " << config_file << "\n";
    } else {
        // Fallback: numeric port provided
        int port = std::atoi(argv[1]);
        if (argc >= 3) mcast = argv[2];
        r = cam_controller_start_ex(c, port, mcast);
        if (r != 0) {
            std::cerr << "cam_controller_start_ex failed: " << r << "\n";
            cam_controller_destroy(c);
            return 1;
        }
        std::cerr << "Started listener on port " << port;
        if (mcast) std::cerr << " (multicast group=" << mcast << ")";
        std::cerr << "\n";
    }

    // run until user requests exit (Ctrl+C). Print last packet length and PTZ every second
    int i = 0;
    while (!g_stop) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint8_t buf[2048];
        int n = cam_controller_get_last(c, buf, sizeof(buf));
        std::cerr << "Iteration " << i << ": last packet len=" << n << "\n";
        float az=0, el=0, ir=0, vis=0;
        if (cam_controller_get_ptz(c, &az, &el, &ir, &vis)) {
            std::cerr << " PTZ az=" << az << " el=" << el << " ir=" << ir << " vis=" << vis << "\n";
        }
        ++i;
    }
    std::cerr << "Received stop signal, shutting down...\n";

    cam_controller_stop(c);
    cam_controller_destroy(c);
    return 0;
}
