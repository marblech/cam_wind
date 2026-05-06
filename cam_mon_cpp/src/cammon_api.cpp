#include "protocol.h"

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

/// 发送 UDP 数据包并等待接收响应
/// 
/// 此函数创建一个 UDP 套接字，向目标地址发送数据并等待单个响应包。
/// 支持跨平台运行（Windows/Linux），自动处理平台特定的 socket 操作。
/// 
/// @param host 目标主机 IP 地址（如 "192.168.1.100"）
/// @param port 目标端口号
/// @param outbuf 发送数据缓冲区
/// @param outlen 发送数据长度
/// @param inbuf 接收缓冲区
/// @param inbuf_len 接收缓冲区最大长度
/// @param timeout_ms 超时时间（毫秒），0 表示使用默认超时
/// @return 成功时返回接收到的字节数（>0），失败返回负 error code
/// 
/// 错误码:
///   -1: 创建 socket 失败
///   -2: IP 地址转换失败
///   -3: 发送数据失败
///   -10~-XX: Windows WSAGetLastError() 返回的错误码
int cammon_send_udp_and_recv(const char* host, int port, const uint8_t* outbuf, int outlen, uint8_t* inbuf, int inbuf_len, int timeout_ms) {
#ifdef _WIN32
    // 确保 Winsock 已初始化
    if (!g_winsock_initialized) {
        ensure_winsock_init();
    }
    
    // 创建 UDP socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) return -1;
    
    // 配置目标地址
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &serv.sin_addr) != 1) {
        closesocket(sock);
        return -2;
    }
    
    // 发送数据
    int s = sendto(sock, reinterpret_cast<const char*>(outbuf), outlen, 0, (sockaddr*)&serv, sizeof(serv));
    if (s == SOCKET_ERROR) {
        closesocket(sock);
        return -3;
    }
    
    // 设置接收/发送超时
    if (timeout_ms > 0) {
        timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    }
    
    // 接收响应
    sockaddr_in peer{};
    socklen_t plen = sizeof(peer);
    int n = recvfrom(sock, reinterpret_cast<char*>(inbuf), inbuf_len, 0, (sockaddr*)&peer, &plen);
    if (n == SOCKET_ERROR) {
        int e = WSAGetLastError();
        closesocket(sock);
        return -10 - e;
    }
    
    closesocket(sock);
    return n;
#else
    // Linux/Unix 平台实现
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &serv.sin_addr) != 1) {
        close(sock);
        return -2;
    }
    
    // 发送数据
    ssize_t s = sendto(sock, outbuf, outlen, 0, (sockaddr*)&serv, sizeof(serv));
    if (s < 0) {
        close(sock);
        return -3;
    }
    
    // 设置接收超时
    if (timeout_ms > 0) {
        timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    
    // 接收响应
    sockaddr_in peer{};
    socklen_t plen = sizeof(peer);
    ssize_t n = recvfrom(sock, inbuf, inbuf_len, 0, (sockaddr*)&peer, &plen);
    if (n < 0) {
        int e = errno;
        close(sock);
        return -10 - e;
    }
    
    close(sock);
    return (int)n;
#endif
}

// ============================================================================
// 高层设备控制接口
// ============================================================================

/// 发送相机控制命令
/// 
/// 构建可见光相机命令数据包（使用 ADDR_CAMERA_VIS = 0x02 地址），
/// 通过 UDP 发送到目标设备并等待响应。
/// 
/// 调试信息输出到 stderr:
/// - 发送的参数值
/// - 序列化的数据包内容
/// - 接收结果
/// 
/// @param host 目标主机 IP 地址
/// @param port 目标端口号
/// @param func 功能码（如 0x01 查询状态、0x04 拍照等）
/// @param ctrl 控制码（如 0x01 启用、0x00 禁用等）
/// @param payload 数据载荷（可为 nullptr，此时 payload_len=0）
/// @param payload_len 数据载荷长度
/// @param resp_buf 接收缓冲区
/// @param resp_buf_len 接收缓冲区最大长度
/// @param timeout_ms 超时时间（毫秒）
/// @return 成功时返回接收到的字节数（>0），失败返回负 error code
/// 
/// 示例:
/// @code
/// uint8_t resp[2048];
/// int n = cammon_send_camera_command("192.168.1.100", 5000, 0x01, 0x01, nullptr, 0, resp, 2048, 1000);
/// if (n > 0) { /* 处理响应 */ }
/// @endcode
int cammon_send_camera_command(const char* host, int port, uint8_t func, uint8_t ctrl, const uint8_t* payload, int payload_len, uint8_t* resp_buf, int resp_buf_len, int timeout_ms) {
    // 调试输出：发送参数
    fprintf(stderr, "[C++ DEBUG] cammon_send_camera_command: host=%s port=%d func=%d ctrl=%d payload_len=%d timeout=%d\n", 
        host, port, func, ctrl, payload_len, timeout_ms);
    
    // 构建相机命令数据包
    cammon::Packet p = cammon::make_camera_command(func, ctrl, std::vector<uint8_t>(payload, payload + payload_len));
    auto out = p.serialize();
    
    // 调试输出：序列化后的数据包
    fprintf(stderr, "[C++ DEBUG] Camera packet serialized: size=%d, bytes=", (int)out.size());
    for (size_t i = 0; i < out.size() && i < 20; ++i) {
        fprintf(stderr, "%02X ", out[i]);
    }
    fprintf(stderr, "\n");
    
    // 发送 UDP 数据包并等待响应
    int result = cammon_send_udp_and_recv(host, port, out.data(), (int)out.size(), resp_buf, resp_buf_len, timeout_ms);
    fprintf(stderr, "[C++ DEBUG] cammon_send_udp_and_recv returned: %d\n", result);
    return result;
}

/// 发送舵机控制命令
/// 
/// 构建舵机控制数据包（72 字节帧），通过 UDP 发送到目标设备并等待响应。
/// 
/// 调试信息输出到 stderr:
/// - 发送的舵机参数（角度、速度、距离等）
/// - 序列化的数据包内容
/// - 接收到的响应内容
/// 
/// @param host 目标主机 IP 地址
/// @param port 目标端口号
/// @param az 方位角（度）
/// @param el 俯仰角（度）
/// @param azs 方位角速度（度/秒）
/// @param els 俯仰角速度（度/秒）
/// @param target_distance 目标距离（毫米）
/// @param seq 序列号
/// @param control 控制字节
/// @param device_type 设备类型（0x01 表示舵机控制器）
/// @param packet_type 数据包类型（0x02 表示定点报告）
/// @param resp_buf 接收缓冲区
/// @param resp_buf_len 接收缓冲区最大长度
/// @param timeout_ms 超时时间（毫秒）
/// @return 成功时返回接收到的字节数（>0），失败返回负 error code
/// 
/// 示例:
/// @code
/// uint8_t resp[2048];
/// int n = cammon_send_servo_command("192.168.1.100", 5000, 
///     45.0f, 30.0f, 10.0f, 10.0f, 1000, 1, 0x11, 0x01, 0x02, resp, 2048, 1000);
/// if (n > 0) { /* 处理响应 */ }
/// @endcode
int cammon_send_servo_command(const char* host, int port, float az, float el, float azs, float els, uint16_t target_distance, uint8_t seq, uint8_t control, uint8_t device_type, uint8_t packet_type, uint8_t* resp_buf, int resp_buf_len, int timeout_ms) {
    // 构建舵机数据包
    auto out = cammon::build_servo_packet(az, el, azs, els, target_distance, seq, control, device_type, packet_type);
    
    // 调试输出：发送的数据包
    fprintf(stderr, "[C++ DEBUG] Sending servo packet: size=%d, bytes=", (int)out.size());
    for (size_t i = 0; i < out.size() && i < 20; ++i) {
        fprintf(stderr, "%02X ", out[i]);
    }
    fprintf(stderr, "\n");
    
    // 发送 UDP 数据包并等待响应
    int result = cammon_send_udp_and_recv(host, port, out.data(), (int)out.size(), resp_buf, resp_buf_len, timeout_ms);
    fprintf(stderr, "[C++ DEBUG] cammon_send_udp_and_recv returned: %d\n", result);
    
    // 调试输出：响应内容
    if (result > 0) {
        fprintf(stderr, "[C++ DEBUG] Response bytes: ");
        for (int i = 0; i < result && i < 20; ++i) {
            fprintf(stderr, "%02X ", resp_buf[i]);
        }
        fprintf(stderr, "\n");
    }
    return result;
}