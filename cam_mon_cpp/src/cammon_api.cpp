#include "protocol.h"
#include "cammon_api.h"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int ssize_t;
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <errno.h>
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "plog_init.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Windows 平台 Winsock 初始化辅助
// ============================================================================

/// 全局标志：记录 Winsock 是否已初始化（仅 Windows 使用）
#ifdef _WIN32
static bool g_winsock_initialized = false;
#endif

/// 确保 Winsock 已初始化（Windows 专用）
/// 
/// 在 Windows 平台上，首次调用此函数时会执行 WSAStartup 初始化 Winsock 2.2。
/// 后续调用将直接返回，不会重复初始化。
#ifdef _WIN32
static void ensure_winsock_init() {
    if (!g_winsock_initialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            g_winsock_initialized = true;
        }
    }
}
#endif

/// 清理 Winsock 资源（Windows 专用）
/// 
/// 调用 WSACleanup() 释放 Winsock 资源。仅在已初始化时执行。
#ifdef _WIN32
static void cleanup_winsock() {
    if (g_winsock_initialized) {
        WSACleanup();
        g_winsock_initialized = false;
    }
}
#endif

// ============================================================================
// 底层 UDP 通信
// ============================================================================

int cammon_send_udp_and_recv(const char* host, int port, const uint8_t* outbuf, int outlen, uint8_t* inbuf, int inbuf_len, int timeout_ms) {
#ifdef _WIN32
    if (!g_winsock_initialized) ensure_winsock_init();
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) return -1;
    PLOG_INFO << "[cammon_api] created socket=" << (int)sock;
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &serv.sin_addr) != 1) {
        closesocket(sock);
        return -2;
    }
    PLOG_INFO << "[cammon_api] target " << host << ':' << port;
    int s = sendto(sock, reinterpret_cast<const char*>(outbuf), outlen, 0, (sockaddr*)&serv, sizeof(serv));
    if (s == SOCKET_ERROR) {
        int err = WSAGetLastError();
        PLOG_ERROR << "[cammon_api] sendto failed, WSAGetLastError=" << err;
        closesocket(sock);
        return -3 - err;
    }
    // print local socket address/port used for send
    sockaddr_in local{}; int llen = sizeof(local);
    if (getsockname(sock, (sockaddr*)&local, &llen) == 0) {
        char buf[64] = {0};
        inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
        PLOG_INFO << "[cammon_api] local bind " << buf << ':' << ntohs(local.sin_port);
    }
    if (timeout_ms > 0) {
        timeval tv; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    }
    sockaddr_in peer{}; socklen_t plen = sizeof(peer);
    int n = recvfrom(sock, reinterpret_cast<char*>(inbuf), inbuf_len, 0, (sockaddr*)&peer, &plen);
    if (n == SOCKET_ERROR) {
        int e = WSAGetLastError();
        PLOG_ERROR << "[cammon_api] recvfrom failed, WSAGetLastError=" << e;
        closesocket(sock);
        return -10 - e;
    }
    closesocket(sock); return n;
#else
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    sockaddr_in serv{}; serv.sin_family = AF_INET; serv.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &serv.sin_addr) != 1) { close(sock); return -2; }
    ssize_t s = sendto(sock, outbuf, outlen, 0, (sockaddr*)&serv, sizeof(serv));
    if (s < 0) { perror("[cammon_api] sendto"); close(sock); return -3; }
    if (timeout_ms > 0) { timeval tv; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000; setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); }
    sockaddr_in peer{}; socklen_t plen = sizeof(peer);
    ssize_t n = recvfrom(sock, inbuf, inbuf_len, 0, (sockaddr*)&peer, &plen);
    if (n < 0) { int e = errno; perror("[cammon_api] recvfrom"); close(sock); return -10 - e; }
    close(sock); return (int)n;
#endif
}

// ============================================================================
// 高层设备控制接口
// ============================================================================

int cammon_send_camera_command(const char* host, int port, uint8_t func, uint8_t ctrl, const uint8_t* payload, int payload_len, uint8_t* resp_buf, int resp_buf_len, int timeout_ms) {
    // Build camera command (ADDR_CAMERA_VIS assumed inside make_camera_command)
    std::vector<uint8_t> data;
    if (payload && payload_len > 0) data.assign(payload, payload + payload_len);
    cammon::Packet p = cammon::make_camera_command(func, ctrl, data);
    auto out = p.serialize();
    // Debug
    PLOG_DEBUG << "[C++ DEBUG] cammon_send_camera_command: host=" << host << " port=" << port << " func=" << (int)func << " ctrl=" << (int)ctrl << " payload_len=" << payload_len << " timeout=" << timeout_ms;
    PLOG_DEBUG << "[C++ DEBUG] Camera packet serialized: size=" << (int)out.size();
    int result = cammon_send_udp_and_recv(host, port, out.data(), (int)out.size(), resp_buf, resp_buf_len, timeout_ms);
    PLOG_DEBUG << "[C++ DEBUG] cammon_send_udp_and_recv returned: " << result;
    return result;
}

int cammon_send_packet(const char* host, int port, uint8_t addr, uint8_t func, uint8_t ctrl, const uint8_t* payload, int payload_len, uint8_t* resp_buf, int resp_buf_len, int timeout_ms) {
    cammon::Packet p;
    p.addr = addr;
    p.func = func;
    p.ctrl = ctrl;
    if (payload && payload_len > 0) p.data = std::vector<uint8_t>(payload, payload + payload_len);
    auto out = p.serialize();
    return cammon_send_udp_and_recv(host, port, out.data(), (int)out.size(), resp_buf, resp_buf_len, timeout_ms);
}

int cammon_send_servo_command(const char* host, int port, float az, float el, float azs, float els, uint16_t target_distance, uint8_t seq, uint8_t control, uint8_t device_type, uint8_t packet_type, uint8_t* resp_buf, int resp_buf_len, int timeout_ms) {
    auto out = cammon::build_servo_packet(az, el, azs, els, target_distance, seq, control, device_type, packet_type);
    PLOG_DEBUG << "[C++ DEBUG] Sending servo packet: size=" << (int)out.size();
    {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setfill('0');
        for (size_t i = 0; i < out.size() && i < 200; ++i) {
            oss << std::setw(2) << (int)out[i] << ' ';
        }
        PLOG_DEBUG << "[C++ DEBUG] Outgoing bytes: " << oss.str();
    }
    int result = cammon_send_udp_and_recv(host, port, out.data(), (int)out.size(), resp_buf, resp_buf_len, timeout_ms);
    PLOG_DEBUG << "[C++ DEBUG] cammon_send_udp_and_recv returned: " << result;
    if (result > 0) {
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setfill('0');
        for (int i = 0; i < result && i < 20; ++i) {
            oss << std::setw(2) << (int)resp_buf[i] << ' ';
        }
        PLOG_DEBUG << "[C++ DEBUG] Response bytes: " << oss.str();
    }
    return result;
}

#ifdef __cplusplus
}
#endif
