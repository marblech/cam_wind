#ifndef CAM_WIND_SRC_CAM_AJF_LIB_H
#define CAM_WIND_SRC_CAM_AJF_LIB_H

/**
 * @file src/cam_ajf_lib.h
 * @brief Camera AJF 高层接口封装库
 * @author marblech
 * @date 2026-06-02
 * @copyright SPDX: MIT OR as-project-specifies
 *
 * 本库封装了底层 UDP 通信细节，提供简洁的 C++ 接口用于：
 * - 初始化/反初始化
 * - 启动/停止状态监听线程
 * - 获取/设置 PTZ (Pan/Tilt/Zoom) 参数
 *
 * 使用示例:
 * @code
 * CamAJFLib cam;
 * cam.init("192.168.1.100", 4001);
 * cam.start();
 *
 * auto ptz = cam.get_ptz();
 * printf("Current: az=%f el=%f zoom=%f\n", ptz.azimuth, ptz.elevation, ptz.zoom);
 *
 * cam.set_ptz(90.0f, 30.0f, 5);
 *
 * cam.stop();
 * cam.shutdown();
 * @endcode
 */

#include <cstdint>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <vector>

#include "cammon_api.h"

namespace cammon {
/// PTZ 状态数据结构
struct PTZStatus {
    float azimuth = 0.0f;     ///< 方位角（度）
    float elevation = 0.0f;   ///< 俯仰角（度）
    float zoom = 0.0f;        ///< 变焦值
    float focus = 0.0f;       ///< 焦距（毫米）
    
    bool valid = false;       ///< 数据是否有效
    
    PTZStatus() = default;
};

/// 摄像机配置参数
struct CameraConfig {
    std::string host;         ///< 目标设备 IP 地址
    int port = 4001;          ///< 目标端口号
    int timeout_ms = 2000;    ///< 命令超时时间（毫秒）
    int status_port = 8080;   ///< 状态监听端口
    
    CameraConfig() = default;
    CameraConfig(const std::string& h, int p, int timeout = 2000, int status_port = 8080)
        : host(h), port(p), timeout_ms(timeout), status_port(status_port) {}
};

/// PTZ 回调函数类型定义
/// 当接收到状态报告时触发: (azimuth, elevation, zoom, focus, 数据是否有效)
using PTZCallback = std::function<void(float azimuth, float elevation, float zoom, float focus, bool valid)>;

/**
 * @brief Camera AJF 高层接口封装类
 * 
 * 封装了与摄像头/舵机控制器的 UDP 通信，提供 PTZ 控制功能。
 * 内部使用独立线程监听状态报告，并通过回调通知调用者。
 */
class CAMMON_API CamAJFLib {
public:
    /**
     * @brief 构造函数
     * 
     * 创建未初始化的实例，需要调用 init() 进行初始化。
     */
    CamAJFLib();
    
    /**
     * @brief 析构函数
     * 
     * 自动调用 shutdown() 清理资源。
     */
    ~CamAJFLib();
    
    /**
     * @brief 初始化库
     * 
     * 分配 UDP 套接字资源，为后续通信做准备。
     * 必须调用此函数或 initWithConfig() 后才能使用其他功能。
     * 
     * @param host 目标设备 IP 地址
     * @param port 控制命令端口号
     * @param timeout_ms 命令超时时间（毫秒）
     * @return true 初始化成功
     * @return false 初始化失败（套接字创建失败等）
     */
    bool init(const std::string& host, int port = 4001, int timeout_ms = 2000);
    
    /**
     * @brief 使用配置结构体初始化
     * 
     * @param config 配置参数
     * @return true 初始化成功
     * @return false 初始化失败
     */
    bool initWithConfig(const CameraConfig& config);
    
    /**
     * @brief 关闭库，释放所有资源
     * 
     * 自动停止监听线程，关闭 UDP 套接字。
     * 调用后需要重新调用 init() 才能使用。
     */
    void shutdown();
    
    /**
     * @brief 启动状态监听线程
     * 
     * 启动后台线程持续监听 UDP 状态报告，
     * 当接收到 PTZ 状态时会自动更新内部状态并触发回调。
     * 如果线程已在运行则不执行任何操作。
     * 
     * @return true 启动成功
     * @return false 启动失败（监听端口绑定失败或线程已存在）
     */
    bool start();
    
    /**
     * @brief 停止状态监听线程
     * 
     * 通知监听线程退出并等待其完成。
     * 不会关闭主通信套接字。
     */
    void stop();
    
    /**
     * @brief 获取当前 PTZ 状态
     * 
     * 返回最近一次从状态报告或命令响应中获取的 PTZ 数据。
     * 如果从未收到状态报告，valid 字段将为 false。
     * 
     * @return PTZStatus 当前 PTZ 状态
     */
    PTZStatus get_ptz();
    
    /**
     * @brief 设置 PTZ 到指定位置
     * 
     * 发送舵机控制命令将摄像机转到指定位置。
     * 使用最后记录的变焦值作为当前 zoom 值。
     * 
     * @param azimuth 目标方位角（度），通常范围 -180 ~ +180
     * @param elevation 目标俯仰角（度），通常范围 -90 ~ +90
     * @param az_speed 方位角移动速度（度/秒）
     * @param el_speed 俯仰角移动速度（度/秒）
     * @return true 命令发送成功
     * @return false 命令发送失败
     */
    bool set_ptz(float azimuth, float elevation, float az_speed = 1.5f, float el_speed = 0.5f);
    
    /**
     * @brief 设置 PTZ 到指定位置（含变焦）
     * 
     * 发送舵机控制命令将摄像机转到指定位置，同时设置变焦。
     * 
     * @param azimuth 目标方位角（度）
     * @param elevation 目标俯仰角（度）
     * @param zoom 变焦值
     * @param az_speed 方位角移动速度（度/秒）
     * @param el_speed 俯仰角移动速度（度/秒）
     * @return true 命令发送成功
     * @return false 命令发送失败
     */
    bool set_ptz(float azimuth, float elevation, float zoom, float az_speed = 1.5f, float el_speed = 0.5f);
    
    /**
     * @brief 设置变焦值
     * 
     * 发送相机变焦控制命令。
     * 
     * @param zoom 变焦值（0-100 或具体设备定义的范围）
     * @return true 命令发送成功
     * @return false 命令发送失败
     */
    bool set_zoom(float zoom);
    
    /**
     * @brief 设置焦距
     * 
     * 发送相机焦距控制命令。
     * 
     * @param focus 目标焦距值
     * @return true 命令发送成功
     * @return false 命令发送失败
     */
    bool set_focus(float focus);
    
    /**
     * @brief 注册 PTZ 状态回调
     * 
     * 当接收到状态报告并解析出 PTZ 数据时，会调用此回调函数。
     * 回调在监听线程中执行，请注意线程安全。
     * 
     * @param callback 回调函数
     */
    void on_ptz_update(PTZCallback callback);
    
    /**
     * @brief 获取当前配置
     * 
     * @return CameraConfig 当前配置
     */
    CameraConfig get_config() const { return config_; }

private:
    /// 状态监听线程函数
    void status_listener_thread();
    
    /// 解析状态报告并提取 PTZ 数据
    /// @return true 解析成功
    bool parse_status_report(const std::vector<uint8_t>& buf);
    
    /// 将当前 PTZ 状态广播到回调
    void broadcast_ptz_update();
    
    // 配置
    CameraConfig config_;
    
    // UDP 套接字
    int cmd_sock_ = -1;       // 命令发送/接收套接字
    int status_sock_ = -1;    // 状态监听套接字
    
    // 线程控制
    std::thread listener_thread_;
    std::atomic<bool> running_{false};
    
    // 共享状态
    mutable std::mutex ptz_mutex_;
    PTZStatus current_ptz_;
    
    // 回调
    mutable std::mutex callback_mutex_;
    PTZCallback ptz_callback_;
};

} // namespace cammon

#endif /* CAM_WIND_SRC_CAM_AJF_LIB_H */