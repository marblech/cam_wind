#include "protocol.h"
#include <thread>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <signal.h>

int main() {
    // Simple integration test: spawn simulator in a child process thread (uses system call)
    pid_t pid = fork();
    if (pid == 0) {
        // child: run simulator (assumes test runs from build directory where simulator binary exists)
        execlp("./cam_mon_simulator", "./cam_mon_simulator", NULL);
        // if exec fails, try direct binary name
        execlp("cam_mon_simulator", "cam_mon_simulator", NULL);
        _exit(127);
    }
    // parent
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    int r = system("./cam_mon_client 127.0.0.1 4000");
    if (r != 0) {
        std::cerr << "Integration test failed (client returned non-zero)\n";
        // kill child
        kill(pid, SIGKILL);
        return 2;
    }
    // success
    kill(pid, SIGKILL);
    std::cout << "Integration test passed\n";
    return 0;
}
