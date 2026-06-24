#ifndef CAM_WIND_SRC_MULTICAST_RECEIVER_H
#define CAM_WIND_SRC_MULTICAST_RECEIVER_H

/**
 * @file src/multicast_receiver.h
 * @brief 组播接收器类声明
 * @author marblech
 * @date 2026-06-02
 * @copyright SPDX: MIT OR as-project-specifies
 */

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #ifndef ssize_t
    typedef std::ptrdiff_t ssize_t;
    #endif
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    typedef int SOCKET;
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

#include "protocol.h"
#include "cammon_api.h"

namespace cammon {

// ============================================================================
// 组播配置常量
// ============================================================================

/**
 * @brief 默认组播地址
 * 
 * 用于接收开发板状态上报报文的组播组地址
 */
static constexpr const char* MULTICAST_GROUP_ADDR = "239.255.43.21";

/**
 * @brief 默认组播端口
 * 
 * 用于接收开发板状态上报报文的端口
 */
static constexpr int MULTICAST_GROUP_PORT = 23232;

/**
 * @brief 组播接收缓冲区大小
 * 
 * 每次接收组播报文的最大缓冲区大小
 */
static constexpr int MULTICAST_RECV_BUF_SIZE = 4096;

/**
 * @brief 组播接收超时时间（毫秒）
 * 
 * recvfrom 调用的超时时间，用于定期检查是否退出
 */
static constexpr int MULTICAST_RECV_TIMEOUT_MS = 1000;

// ============================================================================
// 组播接收器类
// ============================================================================

/**
 * @brief 组播报文接收器
 * 
 * 用于接收 239.255.43.21:23232 组播地址的组播报文，
 * 并解析负载状态上报数据。
 * 
 * 使用方式：
 * @code
 * MulticastReceiver receiver;
 * receiver.setOnLoadStatusReportHandler([](const LoadStatusReport& report) {
 *     // 处理负载状态上报
 *     printf("方位角: %f\n", report.servo_azimuth);
 * });
 * receiver.start();
 * // ... 运行一段时间
 * receiver.stop();
 * @endcode
 */
class CAMMON_API MulticastReceiver {
public:
    /**
     * @brief 构造函数
     */
    MulticastReceiver();

    /**
     * @brief 析构函数
     */
    ~MulticastReceiver();

    /**
     * @brief 初始化组播接收器
     * 
     * 创建套接字并加入组播组。
     * 如果不指定接口地址，将使用默认网卡加入组播组。
     * 
     * @param bind_addr 绑定的本地地址，"0.0.0.0" 表示所有接口（默认）
     * @param iface_addr 组播接口地址，默认为空（使用系统默认）
     * @param multicast_addr 组播组地址，默认为 MULTICAST_GROUP_ADDR
     * @param multicast_port 组播端口，默认为 MULTICAST_GROUP_PORT
     * @return true 初始化成功，false 初始化失败
     */
    bool init(const char* bind_addr = "0.0.0.0",
              const char* iface_addr = "",
              const char* multicast_addr = MULTICAST_GROUP_ADDR,
              int multicast_port = MULTICAST_GROUP_PORT);

    /**
     * @brief 启动组播接收线程
     * 
     * 启动后台线程持续接收组播报文。
     * 需要在 init() 之后调用。
     * 
     * @return true 启动成功，false 启动失败
     */
    bool start();

    /**
     * @brief 停止组播接收
     * 
     * 停止接收线程并释放资源。
     */
    void stop();

    /**
     * @brief 设置负载状态上报回调函数
     * 
     * @param handler 回调函数，参数为解析后的 LoadStatusReport
     */
    void setOnLoadStatusReportHandler(
        std::function<void(const LoadStatusReport&)> handler);

    /**
     * @brief 设置原始报文回调函数
     * 
     * 用于接收尚未解析的原始组播报文数据。
     * 
     * @param handler 回调函数，参数为原始字节数据和发送方地址信息
     */
    void setOnRawPacketHandler(
        std::function<void(const uint8_t*, int, const char*, int)> handler);

    /**
     * @brief 获取当前接收状态
     * 
     * @return true 正在接收，false 未接收或已停止
     */
    bool isRunning() const { return running_.load(); }

    /**
     * @brief 获取已接收的报文计数
     * 
     * @return 接收的报文总数
     */
    int getPacketCount() const { return packet_count_.load(); }

    /**
     * @brief 获取组播地址
     * @return 组播地址字符串
     */
    const std::string& getMulticastAddr() const { return multicast_addr_; }

    /**
     * @brief 获取组播端口
     * @return 组播端口
     */
    int getMulticastPort() const { return multicast_port_; }

private:
    /**
     * @brief 接收线程函数
     */
    void receiverThreadFunc();

    /**
     * @brief 尝试解析接收到的报文
     * 
     * @param buf 接收到的数据缓冲区
     * @param len 数据长度
     * @param src_addr 发送方地址
     * @param src_port 发送方端口
     * @return true 成功解析并触发了回调
     */
    bool parseReceivedPacket(const uint8_t* buf, int len, 
                            const char* src_addr, int src_port);

    // 套接字相关
    SOCKET sock_;
    bool initialized_;
    std::atomic<bool> running_;
    std::atomic<int> packet_count_;

    // 线程相关
    std::thread thread_;

    // 回调函数
    std::function<void(const LoadStatusReport&)> onLoadStatusReport_;
    std::function<void(const uint8_t*, int, const char*, int)> onRawPacket_;

    // 组播配置
    std::string bind_addr_;
    std::string iface_addr_;
    std::string multicast_addr_;
    int multicast_port_;
};

} // namespace cammon

#endif /* CAM_WIND_SRC_MULTICAST_RECEIVER_H */