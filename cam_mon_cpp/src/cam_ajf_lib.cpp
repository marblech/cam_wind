#include "cam_ajf_lib.h"
#include "protocol.h"
#include "cammon_api.h"  // 底层 API 声明

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#include <cstring>
#include <iostream>
#include <chrono>

namespace cammon {

// ============================================================================
// 辅助函数
// ============================================================================

#ifdef _WIN32
static int init_winsock() {
    static bool inited = false;
    if (!inited) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
        inited = true;
    }
    return 0;
}
#else
static int init_winsock() { return 0; }
#endif

static int create_udp_socket() {
    init_winsock();
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
        return -1;
    }
#else
    if (sock < 0) {
        return -1;
    }
    // 设置非阻塞模式，使 recvfrom 可超时退出
    #ifdef __APPLE__
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    #else
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    #endif
#endif
    return sock;
}

static bool close_udp_socket(int sock) {
#ifdef _WIN32
    if (sock == INVALID_SOCKET) return false;
    closesocket(sock);
    return true;
#else
    if (sock < 0) return false;
    close(sock);
    return true;
#endif
}

static uint16_t read_le_u16(const uint8_t* buf, size_t idx) {
    return (uint16_t)buf[idx] | ((uint16_t)buf[idx + 1] << 8);
}

static uint32_t read_le_u32(const uint8_t* buf, size_t idx) {
    return (uint32_t)buf[idx] | ((uint32_t)buf[idx + 1] << 8) |
           ((uint32_t)buf[idx + 2] << 16) | ((uint32_t)buf[idx + 3] << 24);
}

static float read_le_float(const uint8_t* buf, size_t idx) {
    uint32_t v = read_le_u32(buf, idx);
    float f;
    std::memcpy(&f, &v, sizeof(f));
    return f;
}

// ============================================================================
// CamAJFLib 实现
// ============================================================================

CamAJFLib::CamAJFLib() {
    init_winsock();
}

CamAJFLib::~CamAJFLib() {
    shutdown();
}

bool CamAJFLib::init(const std::string& host, int port, int timeout_ms) {
    CameraConfig config(host, port, timeout_ms);
    return initWithConfig(config);
}

bool CamAJFLib::initWithConfig(const CameraConfig& config) {
    // 如果已经初始化，先清理
    if (cmd_sock_ >= 0) {
        shutdown();
    }
    
    config_ = config;
    
    // 创建命令套接字
    cmd_sock_ = create_udp_socket();
    if (cmd_sock_ < 0) {
        std::cerr << "[CamAJFLib] Failed to create command socket\n";
        return false;
    }
    
    // 设置接收超时
    struct timeval tv;
    tv.tv_sec = config.timeout_ms / 1000;
    tv.tv_usec = (config.timeout_ms % 1000) * 1000;
    if (setsockopt(cmd_sock_, SOL_SOCKET, SO_RCVTIMEO, 
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
        std::cerr << "[CamAJFLib] Failed to set recv timeout\n";
    }
    
    std::cout << "[CamAJFLib] Initialized: host=" << config.host 
              << " port=" << config.port << " timeout=" << config.timeout_ms << "ms\n";
    return true;
}

void CamAJFLib::shutdown() {
    stop();
    
    if (cmd_sock_ >= 0) {
        close_udp_socket(cmd_sock_);
        cmd_sock_ = -1;
    }
    
    if (status_sock_ >= 0) {
        close_udp_socket(status_sock_);
        status_sock_ = -1;
    }
    
    std::cout << "[CamAJFLib] Shutdown complete\n";
}

bool CamAJFLib::start() {
    // 如果已经在运行，不重复启动
    if (running_) {
        std::cout << "[CamAJFLib] Listener already running\n";
        return false;
    }
    
    // 检查命令套接字是否已初始化
    if (cmd_sock_ < 0) {
        std::cerr << "[CamAJFLib] Not initialized, call init() first\n";
        return false;
    }
    
    // 创建状态监听套接字
    status_sock_ = create_udp_socket();
    if (status_sock_ < 0) {
        std::cerr << "[CamAJFLib] Failed to create status listener socket\n";
        return false;
    }
    
    // 绑定到状态监听端口
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config_.status_port);
    
    if (bind(status_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        std::cerr << "[CamAJFLib] bind failed, error=" << err << "\n";
#else
        perror("[CamAJFLib] bind");
#endif
        close_udp_socket(status_sock_);
        status_sock_ = -1;
        return false;
    }
    
    // 启动监听线程
    running_ = true;
    listener_thread_ = std::thread(&CamAJFLib::status_listener_thread, this);
    
    std::cout << "[CamAJFLib] Status listener started on port " << config_.status_port << "\n";
    return true;
}

void CamAJFLib::stop() {
    if (!running_) return;
    
    running_ = false;
    
    // 关闭状态套接字以唤醒阻塞的 recvfrom
    if (status_sock_ >= 0) {
        close_udp_socket(status_sock_);
        status_sock_ = -1;
    }
    
    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
    
    std::cout << "[CamAJFLib] Status listener stopped\n";
}

PTZStatus CamAJFLib::get_ptz() {
    std::lock_guard<std::mutex> lock(ptz_mutex_);
    PTZStatus result = current_ptz_;
    return result;
}

bool CamAJFLib::set_ptz(float azimuth, float elevation, float az_speed, float el_speed) {
    std::lock_guard<std::mutex> lock(ptz_mutex_);
    float zoom = current_ptz_.zoom;
    return set_ptz(azimuth, elevation, zoom, az_speed, el_speed);
}

bool CamAJFLib::set_ptz(float azimuth, float elevation, float zoom, float az_speed, float el_speed) {
    // 构建舵机控制数据包
    std::vector<uint8_t> packet = build_servo_packet(
        azimuth, elevation, az_speed, el_speed, 
        /*target_distance=*/0,  // 默认不使用距离
        /*seq=*/0x01,
        /*control=*/0x11,  // 位置控制标志
        /*device_type=*/0x01,
        /*packet_type=*/0x02
    );
    
    // 发送命令
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &serv.sin_addr);
    
    ssize_t sent = sendto(cmd_sock_, 
                          reinterpret_cast<const char*>(packet.data()), 
                          packet.size(), 
                          0, 
                          reinterpret_cast<sockaddr*>(&serv), 
                          sizeof(serv));
    
    if (sent < 0) {
        std::cerr << "[CamAJFLib] Failed to send servo command\n";
        return false;
    }
    
    // 尝试接收响应
    uint8_t buf[4096];
    sockaddr_in peer{};
    socklen_t plen = sizeof(peer);
    
    // 使用较短的超时等待响应
    struct timeval rtv;
    rtv.tv_sec = 0;
    rtv.tv_usec = 500000;  // 500ms
    setsockopt(cmd_sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rtv), sizeof(rtv));
    
    ssize_t n = recvfrom(cmd_sock_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&peer), &plen);
    
    // 恢复原始超时
    struct timeval orig_tv;
    orig_tv.tv_sec = config_.timeout_ms / 1000;
    orig_tv.tv_usec = (config_.timeout_ms % 1000) * 1000;
    setsockopt(cmd_sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&orig_tv), sizeof(orig_tv));
    
    if (n > 0) {
        std::vector<uint8_t> resp(buf, buf + n);
        auto servo_resp = ServoPacket::deserialize_servo(resp);
        if (servo_resp) {
            std::lock_guard<std::mutex> lock(ptz_mutex_);
            current_ptz_.azimuth = servo_resp->azimuth;
            current_ptz_.elevation = servo_resp->elevation;
            current_ptz_.valid = true;
            std::cout << "[CamAJFLib] PTZ response: az=" << servo_resp->azimuth 
                      << " el=" << servo_resp->elevation << "\n";
            broadcast_ptz_update();
            return true;
        }
    }
    
    // 没有响应但命令已发送
    std::cout << "[CamAJFLib] PTZ command sent (no response): az=" << azimuth << " el=" << elevation << "\n";
    return true;
}

bool CamAJFLib::set_zoom(float zoom) {
    // 构建相机控制命令：功能码 0x01, 控制码 0x05 (变焦)
    std::vector<uint8_t> payload(15, 0x00);
    // 将 zoom 值映射到 0-255 范围
    uint8_t zoom_val = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, zoom)));
    payload[0] = zoom_val;
    
    Packet pkt = make_camera_command(/*func=*/0x01, /*ctrl=*/0x05, payload);
    auto out = pkt.serialize();
    
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &serv.sin_addr);
    
    ssize_t sent = sendto(cmd_sock_, reinterpret_cast<const char*>(out.data()), out.size(), 0,
                          reinterpret_cast<sockaddr*>(&serv), sizeof(serv));
    
    if (sent < 0) {
        std::cerr << "[CamAJFLib] Failed to send zoom command\n";
        return false;
    }
    
    // 更新内部状态
    {
        std::lock_guard<std::mutex> lock(ptz_mutex_);
        current_ptz_.zoom = zoom;
    }
    
    std::cout << "[CamAJFLib] Zoom set to: " << zoom << "\n";
    return true;
}

bool CamAJFLib::set_focus(float focus) {
    // 构建相机控制命令：功能码 0x01, 控制码 0x06 (焦距)
    std::vector<uint8_t> payload(15, 0x00);
    // 将焦距值转换为浮点数存储
    uint32_t focus_bits = static_cast<uint32_t>(focus * 100);  // 精度 0.01
    payload[0] = focus_bits & 0xFF;
    payload[1] = (focus_bits >> 8) & 0xFF;
    payload[2] = (focus_bits >> 16) & 0xFF;
    payload[3] = (focus_bits >> 24) & 0xFF;
    
    Packet pkt = make_camera_command(/*func=*/0x01, /*ctrl=*/0x06, payload);
    auto out = pkt.serialize();
    
    sockaddr_in serv{};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &serv.sin_addr);
    
    ssize_t sent = sendto(cmd_sock_, reinterpret_cast<const char*>(out.data()), out.size(), 0,
                          reinterpret_cast<sockaddr*>(&serv), sizeof(serv));
    
    if (sent < 0) {
        std::cerr << "[CamAJFLib] Failed to send focus command\n";
        return false;
    }
    
    // 更新内部状态
    {
        std::lock_guard<std::mutex> lock(ptz_mutex_);
        current_ptz_.focus = focus;
    }
    
    std::cout << "[CamAJFLib] Focus set to: " << focus << "\n";
    return true;
}

void CamAJFLib::on_ptz_update(PTZCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    ptz_callback_ = std::move(callback);
}

// ============================================================================
// 私有方法实现
// ============================================================================

void CamAJFLib::status_listener_thread() {
    std::cout << "[CamAJFLib] Status listener thread started\n";
    
    while (running_) {
        uint8_t buf[2048];
        sockaddr_in src{};
        socklen_t slen = sizeof(src);
        
        // 设置短超时以便检查 running_ 标志
        struct timeval rtv;
        rtv.tv_sec = 0;
        rtv.tv_usec = 100000;  // 100ms
        setsockopt(status_sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rtv), sizeof(rtv));
        
        ssize_t n = recvfrom(status_sock_, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&src), &slen);
        
        if (n < 0) {
            // 超时或错误，继续循环
            continue;
        }
        
        // 解析状态报告（使用栈上缓冲区）
        parse_status_report(std::vector<uint8_t>(buf, buf + n));
    }
    
    std::cout << "[CamAJFLib] Status listener thread exited\n";
}

bool CamAJFLib::parse_status_report(const std::vector<uint8_t>& buf) {
    // 检查是否为状态报告格式 (0x1F 0xF1 开头)
    if (buf.size() < 51) return false;
    if (buf[0] != 0x1F || buf[1] != 0xF1) return false;
    if (buf[49] != 0xF1 || buf[50] != 0x1F) return false;
    
    // 验证校验和
    uint8_t cs = 0;
    for (size_t i = 2; i <= 47 && i < buf.size(); ++i) {
        cs ^= buf[i];
    }
    if (cs != buf[48]) {
        std::cerr << "[CamAJFLib] Status report checksum mismatch\n";
        return false;
    }
    
    // 提取 PTZ 数据 (按协议文档 Table 3-12)
    float ir_focus = read_le_float(buf.data(), 6);       // 红外焦距
    float vis_focus = read_le_float(buf.data(), 10);     // 可见光焦距
    float servo_az = read_le_float(buf.data(), 28);      // 伺服方位角
    float servo_el = read_le_float(buf.data(), 32);      // 伺服俯仰角
    
    // 更新内部 PTZ 状态
    {
        std::lock_guard<std::mutex> lock(ptz_mutex_);
        current_ptz_.azimuth = servo_az;
        current_ptz_.elevation = servo_el;
        current_ptz_.focus = vis_focus;
        current_ptz_.valid = true;
    }
    
    std::cout << "[CamAJFLib] Status: az=" << servo_az << " el=" << servo_el 
              << " focus=" << vis_focus << "\n";
    
    // 触发回调
    broadcast_ptz_update();
    
    return true;
}

void CamAJFLib::broadcast_ptz_update() {
    std::lock_guard<std::mutex> cb_lock(callback_mutex_);
    std::lock_guard<std::mutex> ptz_lock(ptz_mutex_);
    
    if (ptz_callback_) {
        ptz_callback_(current_ptz_.azimuth, current_ptz_.elevation, 
                      current_ptz_.zoom, current_ptz_.focus, current_ptz_.valid);
    }
}

} // namespace cammon