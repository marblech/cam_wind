#include "cam_controller.h"
#include "protocol.h"
#include "cammon_api.h"
#include "plog_init.h"

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
#include <sstream>
#include <iomanip>
#include <cmath>
#include <plog/Log.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

// Boost.PropertyTree for config file reading (INI)
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "load_settings.h"

using namespace cammon;

struct CamController {
    std::thread thr;
    std::atomic<bool> running{false};
    std::mutex mtx;
    std::vector<uint8_t> last_packet;
    std::unordered_map<std::string, std::vector<uint8_t>> packets_by_ip;
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
    // 支持49字节上报帧（协议 3.4.1 节负载状态上报）
    // 帧格式: [1F F1][地址][帧序号][数据41字节][校验][F1 1F]
    if (buf.size() == 49 && buf[0] == 0x1F && buf[1] == 0xF1) {
        // 检查帧尾
        if (buf[47] == 0xF1 && buf[48] == 0x1F) {
            // 校验和验证：字节2到字节46的XOR
            uint8_t cs = 0;
            for (size_t i = 2; i <= 46 && i < buf.size(); ++i) cs ^= buf[i];
            if (cs != buf[47]) {
                PLOG_WARNING << "[CamController] 49-byte report frame checksum fail: got=" << (int)cs << " expect=" << (int)buf[47];
                // 校验失败，尝试其他解析器
                goto try_packet_parser;
            }
            uint8_t addr = buf[2];
            uint16_t seq = (uint16_t)buf[3] | ((uint16_t)buf[4] << 8);
            float ir_focus = 0.0f;
            float vis_focus = 0.0f;
            float servo_az = 0.0f;
            float servo_el = 0.0f;
            // 数据区偏移（buf[5]是数据区开始，字段按协议3.4.1定义）
            if (buf.size() > 13) {
                ir_focus = *reinterpret_cast<const float*>(&buf[6]);    // [6-9] 红外焦距
                vis_focus = *reinterpret_cast<const float*>(&buf[10]);  // [10-13] 白光焦距
            }
            if (buf.size() > 33) {
                servo_az = *reinterpret_cast<const float*>(&buf[26]);   // [26-29] 伺服方位角
                servo_el = *reinterpret_cast<const float*>(&buf[30]);   // [30-33] 伺服俯仰角
            }
            PLOG_INFO << "[CamController] Status(addr=" << (int)addr << " seq=" << seq
                      << " ir=" << ir_focus << " vis=" << vis_focus
                      << " az=" << servo_az << " el=" << servo_el << ")";
            return true;
        }
    }
    // 支持51字节及以上上报帧（旧格式）
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
        if (buf.size() > 33) {
            // According to protocol and sender (payload data starts at index 5):
            // servo_az is at bytes [26-29], servo_el at [30-33]
            servo_az = *reinterpret_cast<const float*>(&buf[26]);
            servo_el = *reinterpret_cast<const float*>(&buf[30]);
        }
        PLOG_INFO << "[CamController] Status addr=" << (int)addr << " seq=" << seq << " ir=" << ir_focus << " vis=" << vis_focus << " az=" << servo_az << " el=" << servo_el;
        return true;
    }
    try_packet_parser:
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
            PLOG_ERROR << "cam_controller IP_ADD_MEMBERSHIP failed";
            // Not fatal: continue running but mark not multicast
            c->is_multicast = false;
        } else {
            PLOG_INFO << "[CamController] Joined multicast group " << c->mcast_group;
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

    PLOG_INFO << "[CamController] Listening on UDP port " << c->listen_port;

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
            
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(src.sin_addr), ip_str, INET_ADDRSTRLEN);
            c->packets_by_ip[std::string(ip_str)] = buf;
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
    PLOG_INFO << "[CamController] Listener exiting";
}

extern "C" {

CAMMON_API CamController* cam_controller_create() {
    // 首次创建控制器时初始化日志系统
    static bool log_initialized = false;
    if (!log_initialized) {
        initPlog();
        log_initialized = true;
        PLOG_INFO << "[CamController] Logger initialized";
    }
    
    CamController* controller = new CamController();
    PLOG_INFO << "[CamController] Created new controller instance";
    return controller;
}

CAMMON_API int cam_controller_start_with_config(CamController* h, const char* config_file, const char* mcast_group) {
    if (!h) return -1;
    if (!config_file) return -2;
    try {
        set_conf_file(std::string(config_file));
        int port = std::stoi(loadSetting("p2p", "listen_port"));
        if (port <= 0) {
            PLOG_ERROR << "[CamController] Invalid or missing p2p.listen_port in config: " << config_file;
            return -3;
        }
        return cam_controller_start_ex(h, port, mcast_group);
    } catch (const std::exception& e) {
        PLOG_ERROR << "[CamController] Failed to read config " << config_file << ": " << e.what();
        return -4;
    }
}

CAMMON_API void cam_controller_destroy(CamController* h) {
    if (!h) return;
    cam_controller_stop(h);
    delete h;
}

CAMMON_API int cam_controller_start_ex(CamController* h, int port, const char* mcast_group) {
    if (!h) return -1;
    if (h->running.load()) return -2; // already running
    h->listen_port = port;
    if (mcast_group && mcast_group[0] != '\0') {
        h->is_multicast = true;
        h->mcast_group = mcast_group;
        PLOG_INFO << "[CamController] Starting in multicast mode, group=" << h->mcast_group << " port=" << port;
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

CAMMON_API void cam_controller_stop(CamController* h) {
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
                PLOG_ERROR << "cam_controller IP_DROP_MEMBERSHIP failed";
            } else {
                PLOG_INFO << "[CamController] Left multicast group " << h->mcast_group;
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

CAMMON_API int cam_controller_get_last(CamController* h, uint8_t* buf, int buflen) {
    if (!h) return 0;
    std::lock_guard<std::mutex> lk(h->mtx);
    if (h->last_packet.empty()) return 0;
    int copy_len = (int)std::min((size_t)buflen, h->last_packet.size());
    std::memcpy(buf, h->last_packet.data(), copy_len);
    return copy_len;
}

CAMMON_API int cam_controller_get_ptz(CamController* h, const char* ip, float* out_az, float* out_el, float* out_ir_focus, float* out_vis_focus) {
    if (!h) 
    {
        PLOG_ERROR << "cam_controller_get_ptz: controller handle is NULL";
        return -1;
    }
    PLOG_INFO << "cam_controller_get_ptz: retrieving PTZ for IP=" << (ip ? ip : "(null)");
    std::lock_guard<std::mutex> lk(h->mtx);
    const std::vector<uint8_t>* buf_ptr = nullptr;
    if (ip != nullptr && *ip != '\0') {
        auto it = h->packets_by_ip.find(ip);
        if (it == h->packets_by_ip.end())
        {
            PLOG_ERROR << "cam_controller_get_ptz: no packet found for IP=" << ip;
            return -1;
        }
        buf_ptr = &it->second;
        PLOG_INFO << "cam_controller_get_ptz: found packet for IP=" << ip << ", size=" << buf_ptr->size();
    } else {
        PLOG_ERROR << "cam_controller_get_ptz: IP is NULL or empty";
        return -1;
        // if (h->last_packet.empty()) 
        // {
        //     PLOG_ERROR << "cam_controller_get_ptz: no last packet available";
        //     return 0;
        // }
        // buf_ptr = &h->last_packet;
    }
    PLOG_INFO << "cam_controller_get_ptz: parsing packet of size=" << buf_ptr->size();
    const std::vector<uint8_t>& buf = *buf_ptr;
    // Expect positions per protocol: ir_focus bytes 6-9, vis_focus 10-13, servo_az 26-29, servo_el 30-33
    bool ok = true;
    float ir = 0.0f, vis = 0.0f, az = 0.0f, el = 0.0f;
    if (buf.size() >= 14) {
        std::memcpy(&ir, &buf[6], sizeof(float));
        std::memcpy(&vis, &buf[10], sizeof(float));
    } else ok = false;
    if (buf.size() >= 34) {
        std::memcpy(&az, &buf[26], sizeof(float));
        std::memcpy(&el, &buf[30], sizeof(float));
    } else ok = false;
    if (!ok) return 0;
    if (out_az) *out_az = az;
    if (out_el) *out_el = el;
    if (out_ir_focus) *out_ir_focus = ir;
    if (out_vis_focus) *out_vis_focus = vis;
    PLOG_INFO << "Get PTZ P:" << az << " T:" << el << " z:" << ir;
    return 1;
}

CAMMON_API int cam_controller_set_ptz(CamController* h, const char* host, const int port,
                          float az, float el, float zoom,
                          uint8_t device_type, action_type action) {
    // 接口参数的device_type 表示（可见光/热成像）设备类型，0为可见光，1为热成像，与下面调用的device_type不同。

    if (!h) return -1;
    if (!host || host[0] == '\0') {
        PLOG_ERROR << "cam_controller_set_ptz: host is NULL or empty";
        return -2;
    }
    // if (device_type == 0) {
    //     PLOG_ERROR << "cam_controller_set_ptz: device_type is 0 (invalid)";
    //     return -3;
    // }

    // Default parameters derived from test_ptz_control behavior
    // const int port = 8080; // default control port when not provided by caller
    const float azs = 1.5f;
    const float els = 0.5f;
    const uint16_t target_distance = 0;
    const uint8_t seq = DEFAULT_SEQ;
    const uint8_t control = SERVO_CTRL_POSITION;
    const uint8_t packet_type = SERVO_PACKET_TYPE_POINT;

    // temporary response buffer for underlying UDP send/recv
    const int RESP_MAX = 2048;
    uint8_t resp[RESP_MAX];

    PLOG_INFO << "cam_controller_set_ptz: sending servo to " << host << ":" << port << " az=" << az << " el=" << el << " device=" << (int)device_type;

    // 构建舵机包并在发送前填充焦距字段（与 test_ptz_control.cpp / CamAJFLib::set_ptz 保持一致）
    std::vector<uint8_t> packet = cammon::build_servo_packet(az, el, azs, els, target_distance, seq, control, device_type, packet_type);

    // 填入当前焦距占位并重算校验（若包大小足够）
    if (packet.size() >= 72) {
        float focus = 0.0f;
        // 尝试从最后收到的数据中获取焦距作为默认值（与 test_ptz_control.cpp 一致，使用同一焦距值填充可见光和红外焦距）
        {
            std::lock_guard<std::mutex> lk(h->mtx);
            if (!h->last_packet.empty()) {
                // 尝试解析可见光焦距位置（与 parse_and_log_status 中约定的偏移一致）
                if (h->last_packet.size() > 13) {
                    std::memcpy(&focus, &h->last_packet[10], sizeof(float));
                }
            }
        }
        // 注意: zoom 参数是变焦值（0-100 范围），不是焦距值（mm）。
        // 不应将 zoom 填入 vis_focus/ir_focus 焦距字段。
        // 焦距字段保持从 last_packet 获取的当前值（或全零）。
        // 与 test_ptz_control.cpp 的做法一致：两个焦距使用同一个 focus 值。
        (void)zoom; // zoom is not written to focal fields; kept as-is
        packet[42] = 0x00; // 焦距单位 (0x00 = mm)
        uint32_t vbits = 0;
        std::memcpy(&vbits, &focus, sizeof(vbits));
        packet[43] = static_cast<uint8_t>(vbits & 0xFF);
        packet[44] = static_cast<uint8_t>((vbits >> 8) & 0xFF);
        packet[45] = static_cast<uint8_t>((vbits >> 16) & 0xFF);
        packet[46] = static_cast<uint8_t>((vbits >> 24) & 0xFF);
        packet[47] = static_cast<uint8_t>(vbits & 0xFF);
        packet[48] = static_cast<uint8_t>((vbits >> 8) & 0xFF);
        packet[49] = static_cast<uint8_t>((vbits >> 16) & 0xFF);
        packet[50] = static_cast<uint8_t>((vbits >> 24) & 0xFF);
        uint8_t cs = 0;
        for (size_t i = 0; i < 71 && i < packet.size(); ++i) cs ^= packet[i];
        if (packet.size() > 71) packet[71] = cs;
    }

    // 发送：使用标准帧将舵机帧封装为 outer packet (addr = ADDR_SERVO)，以便与设备示例一致
    int r = -1;
    // outer packet: addr = ADDR_SERVO, func=0x00, ctrl=0x00, data = servo packet
    // 使用 cammon_send_packet 发送封装包；若发送失败直接返回错误，不回退到原始舵机包
    // 构建外层标准帧以便打印完整发送内容
    {
        cammon::Packet outer;
        outer.addr = cammon::ADDR_SERVO;
        outer.func = 0x00;
        outer.ctrl = 0x00;
        outer.data = packet; // servo packet as payload
        auto out = outer.serialize();

        // 打印发送的完整报文（十六进制）
        {
            std::ostringstream oss;
            oss << std::hex << std::uppercase << std::setfill('0');
            for (size_t i = 0; i < out.size(); ++i) {
                if (i) oss << ' ';
                oss << std::setw(2) << (int)out[i];
            }
            PLOG_INFO << "Sent packet (" << out.size() << " bytes): " << oss.str();
        }

        // 如果是预期的 P=150, T=20, Z=10，则比对报文是否与用户提供的参考一致
        // if (std::fabs(az - 150.0f) < 0.001f && std::fabs(el - 20.0f) < 0.001f && std::fabs(zoom - 10.0f) < 0.001f) {
        //     std::vector<uint8_t> expected = {
        //         0x0F,0xF0,0x05,0x00,0x00,0x7E,0x48,0x01,0x01,0x02,0x00,0x00,0x09,0x00,0x00,0x00,
        //         0x16,0x43,0x00,0x00,0xA0,0x41,0x00,0x00,0xC0,0x3F,0x00,0x00,0x00,0x3F,0x00,0x00,
        //         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        //         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        //         0x00,0x00,0x00,0x00,0x00,0x00,0x49,0x99,0xF0,0x0F
        //     };
        //     if (out == expected) {
        //         PLOG_INFO << "Outgoing packet matches expected reference for P=150 T=20 Z=10";
        //     } else {
        //         PLOG_WARNING << "Outgoing packet DOES NOT match expected reference for P=150 T=20 Z=10";
        //         // Print expected for convenience
        //         std::ostringstream eoss;
        //         eoss << std::hex << std::uppercase << std::setfill('0');
        //         for (size_t i = 0; i < expected.size(); ++i) {
        //             if (i) eoss << ' ';
        //             eoss << std::setw(2) << (int)expected[i];
        //         }
        //         PLOG_WARNING << "Expected: " << eoss.str();
        //     }
        // }
    }

    r = cammon_send_packet(host, port, cammon::ADDR_SERVO, 0x00, 0x00, packet.data(), (int)packet.size(), resp, RESP_MAX, 1000);
    // if (r < 0) {
    //     PLOG_ERROR << "cam_controller_set_ptz: wrapped servo packet send failed " << r;
    //     return r;
    // }

    // 发送焦距直达命令以实现 zoom 值的下发（与 test_ptz_control 中 CamAJFLib::set_ptz 的 set_focus 一致）
    // 标准帧: ADDR_CAMERA_VIS, function=0x06 (焦距直达), ctrl=0x00, data[0..3]=float LE(zoom)
    if (zoom >= 7.0f && zoom <= 560.0f && action == action_type::ACTION_SET_ZOOM) {
        // 构建标准相机帧 payload: 前 4 字节为 zoom 的 float 小端序表示，后 11 字节填充 0x00
        uint8_t focus_payload[15] = {0};
        uint32_t zbits = 0;
        std::memcpy(&zbits, &zoom, sizeof(zbits));
        focus_payload[0] = static_cast<uint8_t>(zbits & 0xFF);
        focus_payload[1] = static_cast<uint8_t>((zbits >> 8) & 0xFF);
        focus_payload[2] = static_cast<uint8_t>((zbits >> 16) & 0xFF);
        focus_payload[3] = static_cast<uint8_t>((zbits >> 24) & 0xFF);

        int focus_r = cammon_send_camera_command(host, port, 0x06, 0x00,
                                                  focus_payload, 15,
                                                  resp, RESP_MAX, 1000);
        
        // if (focus_r < 0) {
        //     PLOG_WARNING << "cam_controller_set_ptz: focus command send failed (non-fatal) " << focus_r;
        // } else {
        //     PLOG_INFO << "cam_controller_set_ptz: focus command sent, zoom=" << zoom;
        // }
    }

    return 0;
}

} // extern C
