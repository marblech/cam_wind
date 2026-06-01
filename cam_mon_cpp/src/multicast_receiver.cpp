#include "multicast_receiver.h"
#include <cstdio>
#include <cstring>

#ifndef _WIN32
#define INVALID_SOCKET -1
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static bool g_winsock_initialized = false;

static void ensure_winsock_init() {
    if (!g_winsock_initialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            g_winsock_initialized = true;
        }
    }
}
#endif

namespace cammon {

// ============================================================================
// MulticastReceiver 实现
// ============================================================================

MulticastReceiver::MulticastReceiver()
    : sock_(INVALID_SOCKET)
    , initialized_(false)
    , running_(false)
    , packet_count_(0)
{
}

MulticastReceiver::~MulticastReceiver() {
    stop();
}

#ifdef _WIN32
// Windows 下获取第一个支持组播的接口地址（简化版）
static std::string detect_multicast_interface() {
    return "192.168.5.188"; // 默认返回一个接口，用户应手动指定
}
#else
// Linux 下自动检测支持组播的网络接口
static std::string detect_multicast_interface() {
    struct ifaddrs *addrs, *addr;
    if (getifaddrs(&addrs) != 0) {
        return "";
    }

    char iface_name[IFNAMSIZ] = {};
    int best_if_index = 0;

    for (addr = addrs; addr; addr = addr->ifa_next) {
        if (addr->ifa_flags & IFF_MULTICAST && 
            addr->ifa_flags & IFF_UP && 
            addr->ifa_addr && 
            addr->ifa_addr->sa_family == AF_INET) {
            
            // 跳过 loopback 和 docker 接口
            if (strncmp(addr->ifa_name, "lo", 2) == 0) continue;
            if (strncmp(addr->ifa_name, "docker", 6) == 0) continue;
            if (strncmp(addr->ifa_name, "veth", 4) == 0) continue;
            
            // 优先选择 eth0 或以太网接口
            if ((strncmp(addr->ifa_name, "eth", 3) == 0) && !best_if_index) {
                strncpy(iface_name, addr->ifa_name, sizeof(iface_name) - 1);
                best_if_index = if_nametoindex(addr->ifa_name);
            }
            
            // 如果还没找到，用第一个可用的
            if (!best_if_index) {
                strncpy(iface_name, addr->ifa_name, sizeof(iface_name) - 1);
                best_if_index = if_nametoindex(addr->ifa_name);
            }
        }
    }

    freeifaddrs(addrs);

    if (best_if_index) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, iface_name, IFNAMSIZ - 1);
        
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd >= 0) {
            if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
                char* ip = inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr);
                close(fd);
                return ip ? ip : "";
            }
            close(fd);
        }
    }
    
    return "";
}
#endif

bool MulticastReceiver::init(const char* bind_addr, const char* iface_addr) {
#ifdef _WIN32
    if (!g_winsock_initialized) ensure_winsock_init();
#endif

    // 确定接口地址：优先使用传入参数，否则自动检测
    std::string actual_iface_addr;
    if (iface_addr && strlen(iface_addr) > 0) {
        actual_iface_addr = iface_addr;
    } else {
        actual_iface_addr = detect_multicast_interface();
    }
    
    if (actual_iface_addr.empty()) {
        fprintf(stderr, "[MulticastReceiver] 无法检测到有效的网络接口\n");
        return false;
    }

    fprintf(stderr, "[MulticastReceiver] 检测到网络接口地址: %s\n", actual_iface_addr.c_str());

    // 创建 UDP 套接字
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ == INVALID_SOCKET) {
        fprintf(stderr, "[MulticastReceiver] 创建套接字失败\n");
        return false;
    }

    // 设置套接字选项：允许地址重用
    int broadcast = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

#ifdef _WIN32
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, 
               reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));
#endif

    // 设置接收超时
    timeval tv;
    tv.tv_sec = MULTICAST_RECV_TIMEOUT_MS / 1000;
    tv.tv_usec = (MULTICAST_RECV_TIMEOUT_MS % 1000) * 1000;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, 
               reinterpret_cast<const char*>(&tv), sizeof(tv));

    // 绑定套接字到组播端口
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(MULTICAST_GROUP_PORT);
    local.sin_addr.s_addr = inet_addr(bind_addr);

    if (bind(sock_, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
        fprintf(stderr, "[MulticastReceiver] 绑定套接字到端口 %d 失败: %d\n", 
                MULTICAST_GROUP_PORT, 
#ifdef _WIN32
                WSAGetLastError()
#else
                errno
#endif
        );
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        return false;
    }

    // 加入组播组 - 使用检测到的接口地址
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP_ADDR);
    mreq.imr_interface.s_addr = inet_addr(actual_iface_addr.c_str());

    if (setsockopt(sock_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
        fprintf(stderr, "[MulticastReceiver] 加入组播组 %s 失败: %d\n",
                MULTICAST_GROUP_ADDR,
#ifdef _WIN32
                WSAGetLastError()
#else
                errno
#endif
        );
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        return false;
    }

    bind_addr_ = bind_addr;
    iface_addr_ = iface_addr ? iface_addr : "";
    initialized_ = true;

    fprintf(stderr, "[MulticastReceiver] 初始化成功: 组播地址 %s, 端口 %d, 绑定地址 %s, 接口地址 %s\n",
            MULTICAST_GROUP_ADDR, MULTICAST_GROUP_PORT, bind_addr, 
            iface_addr_.empty() ? "默认" : iface_addr_.c_str());

    return true;
}

bool MulticastReceiver::start() {
    if (!initialized_) {
        fprintf(stderr, "[MulticastReceiver] 未初始化，无法启动\n");
        return false;
    }

    if (running_.load()) {
        fprintf(stderr, "[MulticastReceiver] 已经在运行中\n");
        return false;
    }

    running_.store(true);
    packet_count_.store(0);

    thread_ = std::thread(&MulticastReceiver::receiverThreadFunc, this);

    fprintf(stderr, "[MulticastReceiver] 接收线程已启动\n");
    return true;
}

void MulticastReceiver::stop() {
    if (!running_.load()) return;

    running_.store(false);

    if (thread_.joinable()) {
        thread_.join();
    }

    if (initialized_ && sock_ != INVALID_SOCKET) {
        // 离开组播组
        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP_ADDR);
        mreq.imr_interface.s_addr = iface_addr_.empty() ? INADDR_ANY : inet_addr(iface_addr_.c_str());

        setsockopt(sock_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                   reinterpret_cast<const char*>(&mreq), sizeof(mreq));

        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }

    fprintf(stderr, "[MulticastReceiver] 已停止，共接收 %d 个报文\n", packet_count_.load());
}

void MulticastReceiver::receiverThreadFunc() {
    char buf[MULTICAST_RECV_BUF_SIZE];

    while (running_.load()) {
        // 接收组播报文
        sockaddr_in sender{};
        socklen_t sender_len = sizeof(sender);

        ssize_t n = recvfrom(sock_, buf, static_cast<int>(sizeof(buf)), 0,
                            reinterpret_cast<sockaddr*>(&sender), &sender_len);

        if (n > 0) {
            packet_count_.fetch_add(1);

            // 获取发送方地址
            char sender_addr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender.sin_addr, sender_addr, sizeof(sender_addr));

            // 调用原始报文回调
            if (onRawPacket_) {
                onRawPacket_(
                    reinterpret_cast<const uint8_t*>(buf), 
                    static_cast<int>(n),
                    sender_addr,
                    ntohs(sender.sin_port)
                );
            }

            // 解析报文
            parseReceivedPacket(
                reinterpret_cast<const uint8_t*>(buf), 
                static_cast<int>(n),
                sender_addr, 
                ntohs(sender.sin_port)
            );
        } else if (n < 0) {
            // 检查是否是超时错误
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err != WSAETIMEDOUT) {
                fprintf(stderr, "[MulticastReceiver] 接收错误: %d\n", err);
            }
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "[MulticastReceiver] 接收错误: %s\n", strerror(errno));
            }
#endif
        }
    }
}

void MulticastReceiver::setOnLoadStatusReportHandler(
    std::function<void(const LoadStatusReport&)> handler) {
    onLoadStatusReport_ = handler;
}

void MulticastReceiver::setOnRawPacketHandler(
    std::function<void(const uint8_t*, int, const char*, int)> handler) {
    onRawPacket_ = handler;
}

bool MulticastReceiver::parseReceivedPacket(const uint8_t* buf, int len,
                                           const char* src_addr, int src_port) {
    fprintf(stderr, "[MulticastReceiver] 收到报文: len=%d, src=%s:%d\n", 
            len, src_addr, src_port);

    // 尝试解析为负载状态上报数据
    // 上报帧格式: [1F F1][地址][帧序号][数据][校验][F1 1F]
    // 总长度: 49 字节
    
    if (len >= static_cast<int>(REPORT_LOAD_STATUS_FRAME_LEN)) {
        // 检查帧头
        if (buf[0] == REPORT_HDR1 && buf[1] == REPORT_HDR2) {
            // 检查帧尾
            if (buf[len - 2] == REPORT_TAIL1 && buf[len - 1] == REPORT_TAIL2) {
                // 尝试反序列化
                std::vector<uint8_t> data(buf, buf + len);
                auto report = LoadStatusReport::deserialize(data);
                
                if (report.has_value()) {
                    fprintf(stderr, 
                        "[MulticastReceiver] 解析负载状态上报成功: "
                        "地址=0x%02X, 帧序号=%u, "
                        "红外焦距=%.1f, 白光焦距=%.1f, "
                        "方位脱靶量=%.2f, 俯仰脱靶量=%.2f, "
                        "伺服方位角=%.2f, 伺服俯仰角=%.2f\n",
                        static_cast<unsigned>(report->addr), static_cast<unsigned>(report->frame_seq),
                        report->ir_focal_length, report->vis_focal_length,
                        report->az_aberration, report->el_aberration,
                        report->servo_azimuth, report->servo_elevation);

                    // 调用负载状态上报回调
                    if (onLoadStatusReport_) {
                        onLoadStatusReport_(report.value());
                    }
                    return true;
                } else {
                    fprintf(stderr, "[MulticastReceiver] 负载状态上报反序列化失败\n");
                }
            } else {
                fprintf(stderr, "[MulticastReceiver] 帧尾不匹配: %02X %02X (期望 F1 1F)\n",
                        buf[len - 2], buf[len - 1]);
            }
        } else {
            fprintf(stderr, "[MulticastReceiver] 帧头不匹配: %02X %02X (期望 1F F1)\n",
                    buf[0], buf[1]);
        }
    } else {
        fprintf(stderr, "[MulticastReceiver] 报文长度不足: %d (期望 >= %zu)\n",
                len, REPORT_LOAD_STATUS_FRAME_LEN);
    }

    return false;
}

} // namespace cammon