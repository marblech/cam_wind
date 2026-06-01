#include "cam_controller.h"
#include "protocol.h"
#include "cammon_api.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef int socklen_t;
using ssize_t = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace cammon;

struct CamController {
    std::thread thr;
    std::atomic<bool> running{false};
    std::mutex mtx;
    std::vector<uint8_t> last_packet;
    int listen_port{0};
    int sock{-1};
    bool is_multicast{false};
    std::string mcast_group;
};

static uint32_t read_le_u32(const std::vector<uint8_t>& buf, size_t idx) {
    return (uint32_t)buf[idx] | ((uint32_t)buf[idx+1] << 8) | ((uint32_t)buf[idx+2] << 16) | ((uint32_t)buf[idx+3] << 24);
}

// Minimal status parse for logging; non-fatal on failure
static bool parse_and_log_status(const std::vector<uint8_t>& buf) {
    if (buf.size() >= 51 && buf[0] == 0x1F && buf[1] == 0xF1) {
        // basic checksum check
        uint8_t cs = 0;
        for (size_t i = 2; i <= 47 && i < buf.size(); ++i) cs ^= buf[i];
        if (cs != buf[48]) return false;
        uint8_t addr = buf[2];
        uint16_t seq = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);
        float ir_focus = 0.0f;
        float vis_focus = 0.0f;
        float servo_az = 0.0f;
        float servo_el = 0.0f;
        if (buf.size() > 13) {
            ir_focus = *reinterpret_cast<const float*>(&buf[6]);
            vis_focus = *reinterpret_cast<const float*>(&buf[10]);
        }
        if (buf.size() > 35) {
            servo_az = *reinterpret_cast<const float*>(&buf[28]);
            servo_el = *reinterpret_cast<const float*>(&buf[32]);
        }
        std::cerr << "[CamController] Status addr=" << (int)addr << " seq=" << seq << " ir=" << ir_focus << " vis=" << vis_focus << " az=" << servo_az << " el=" << servo_el << "\n";
        return true;
    }
    // try Packet/ServoPacket parser if available
    try {
        auto p = Packet::deserialize(buf);
        if (p) {
            std::cerr << "[CamController] Packet parsed: addr=" << (int)p->addr << " func=" << (int)p->func << " ctrl=" << (int)p->ctrl << " data_len=" << p->data.size() << "\n";
            return true;
        }
        auto s = ServoPacket::deserialize_servo(buf);
        if (s) {
            std::cerr << "[CamController] Servo parsed: az=" << s->azimuth << " el=" << s->elevation << "\n";
            return true;
        }
    } catch (...) {
        // ignore parser exceptions
    }
    return false;
}

static void listener_loop(CamController* c) {
    // create socket and bind
#ifdef _WIN32
    static bool winsock_inited = false;
    if (!winsock_inited) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2), &wsa);
        winsock_inited = true;
    }
#endif
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("cam_controller socket");
        return;
    }

    c->sock = sock;

    // allow reuse of address so multicast bind can succeed if another
    // listener is present. On some systems SO_REUSEPORT may also be
    // desirable but SO_REUSEADDR is sufficient in most cases.
    int reuse = 1;
#ifdef _WIN32
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(c->listen_port);

        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("cam_controller bind");
    #ifdef _WIN32
        closesocket(sock);
    #else
        close(sock);
    #endif
        c->sock = -1;
        return;
        }
    // If this instance was configured to join a multicast group, do so now.
    if (c->is_multicast && !c->mcast_group.empty()) {
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(c->mcast_group.c_str());
        mreq.imr_interface.s_addr = INADDR_ANY;
        // If multiple interfaces are present, prefer a non-loopback,
        // non-point-to-point IPv4 address for membership so we don't join
        // on an unrelated interface (e.g., ppp0). Try to pick a suitable
        // local address via getifaddrs.
#ifndef _WIN32
        struct ifaddrs *ifap = NULL;
        if (getifaddrs(&ifap) == 0 && ifap) {
            for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr) continue;
                if (ifa->ifa_addr->sa_family != AF_INET) continue;
                int flags = ifa->ifa_flags;
                if (!(flags & IFF_UP)) continue;
                if (flags & IFF_LOOPBACK) continue;
                if (flags & IFF_POINTOPOINT) continue;
                struct sockaddr_in *s4 = (struct sockaddr_in*)ifa->ifa_addr;
                if (s4->sin_addr.s_addr == INADDR_ANY) continue;
                mreq.imr_interface.s_addr = s4->sin_addr.s_addr;
                break;
            }
            freeifaddrs(ifap);
        }
#endif
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
            perror("cam_controller IP_ADD_MEMBERSHIP");
            // Not fatal: continue running but mark not multicast
            c->is_multicast = false;
        } else {
            std::cerr << "[CamController] Joined multicast group " << c->mcast_group << "\n";
        }
    }

    // set recv timeout so we can check running flag periodically
    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
#ifdef _WIN32
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    std::cerr << "[CamController] Listening on UDP port " << c->listen_port << "\n";

    while (c->running.load()) {
        std::vector<uint8_t> buf(2048);
        sockaddr_in src;
        socklen_t slen = sizeof(src);
        ssize_t n = recvfrom(sock, reinterpret_cast<char*>(buf.data()), (socklen_t)buf.size(), 0, (struct sockaddr*)&src, &slen);
        if (n < 0) {
            // timeout or error, continue if running
            continue;
        }
        buf.resize((size_t)n);
        // store last packet
        {
            std::lock_guard<std::mutex> lk(c->mtx);
            c->last_packet = buf;
        }
        // parse/log
        parse_and_log_status(buf);
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    c->sock = -1;
    std::cerr << "[CamController] Listener exiting\n";
}

extern "C" {

CamController* cam_controller_create() {
    return new CamController();
}

void cam_controller_destroy(CamController* h) {
    if (!h) return;
    cam_controller_stop(h);
    delete h;
}

int cam_controller_start_ex(CamController* h, int port, const char* mcast_group) {
    if (!h) return -1;
    if (h->running.load()) return -2; // already running
    h->listen_port = port;
    if (mcast_group && mcast_group[0] != '\0') {
        h->is_multicast = true;
        h->mcast_group = mcast_group;
        std::cerr << "[CamController] Starting in multicast mode, group=" << h->mcast_group << " port=" << port << "\n";
    } else {
        h->is_multicast = false;
        h->mcast_group.clear();
    }

    h->running.store(true);
    try {
        h->thr = std::thread(listener_loop, h);
    } catch (...) {
        h->running.store(false);
        return -3;
    }
    return 0;
}

void cam_controller_stop(CamController* h) {
    if (!h) return;
    if (!h->running.load()) return;
    h->running.store(false);
        if (h->sock >= 0) {
        // If multicast was joined, try to drop membership before closing.
        if (h->is_multicast && !h->mcast_group.empty()) {
            struct ip_mreq mreq;
            mreq.imr_multiaddr.s_addr = inet_addr(h->mcast_group.c_str());
            mreq.imr_interface.s_addr = INADDR_ANY;
            if (setsockopt(h->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
                // best-effort; don't fail stop on this
                perror("cam_controller IP_DROP_MEMBERSHIP");
            } else {
                std::cerr << "[CamController] Left multicast group " << h->mcast_group << "\n";
            }
        }

        // closing socket will interrupt recvfrom
    #ifdef _WIN32
        closesocket(h->sock);
    #else
        close(h->sock);
    #endif
        h->sock = -1;
        }
    if (h->thr.joinable()) h->thr.join();
}

int cam_controller_get_last(CamController* h, uint8_t* buf, int buflen) {
    if (!h) return 0;
    std::lock_guard<std::mutex> lk(h->mtx);
    if (h->last_packet.empty()) return 0;
    int copy_len = (int)std::min((size_t)buflen, h->last_packet.size());
    std::memcpy(buf, h->last_packet.data(), copy_len);
    return copy_len;
}

int cam_controller_get_ptz(CamController* h, float* out_az, float* out_el, float* out_ir_focus, float* out_vis_focus) {
    if (!h) return 0;
    std::lock_guard<std::mutex> lk(h->mtx);
    if (h->last_packet.empty()) return 0;
    const std::vector<uint8_t>& buf = h->last_packet;
    // Expect positions per protocol: ir_focus bytes 6-9, vis_focus 10-13, servo_az 28-31, servo_el 32-35
    bool ok = true;
    float ir = 0.0f, vis = 0.0f, az = 0.0f, el = 0.0f;
    if (buf.size() >= 14) {
        std::memcpy(&ir, &buf[6], sizeof(float));
        std::memcpy(&vis, &buf[10], sizeof(float));
    } else ok = false;
    if (buf.size() >= 36) {
        std::memcpy(&az, &buf[28], sizeof(float));
        std::memcpy(&el, &buf[32], sizeof(float));
    } else ok = false;
    if (!ok) return 0;
    if (out_az) *out_az = az;
    if (out_el) *out_el = el;
    if (out_ir_focus) *out_ir_focus = ir;
    if (out_vis_focus) *out_vis_focus = vis;
    return 1;
}

int cam_controller_set_ptz(CamController* h, const char* host, int port,
                          float az, float el, float azs, float els,
                          uint16_t target_distance, uint8_t seq, uint8_t control,
                          uint8_t device_type, uint8_t packet_type,
                          uint8_t* resp_buf, int resp_buf_len, int timeout_ms) {
    // call into existing API
    return cammon_send_servo_command(host, port, az, el, azs, els, target_distance, seq, control, device_type, packet_type, resp_buf, resp_buf_len, timeout_ms);
}

} // extern C
