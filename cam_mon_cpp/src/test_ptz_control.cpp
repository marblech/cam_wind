/**
 * @file src/test_ptz_control.cpp
 * @brief PTZ 控制测试程序
 * 
 * 输入 P (Pan/方位角)、T (Tilt/俯仰角)、Z (Zoom/变焦) 值，
 * 调用 cammon 库控制摄像机转到指定位置并设置变焦。
 * @author marblech
 * @date 2026-06-24
 * @copyright SPDX: MIT OR as-project-specifies
 */

#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "cam_ajf_lib.h"
#include "protocol.h"

/**
 * @brief 打印使用说明
 */
static void print_usage(const char* prog_name) {
    std::cout << "PTZ Control Test - Camera position and zoom control" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Usage: " << prog_name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -H <host>        Target device IP (default: 193.0.1.94)" << std::endl;
    std::cout << "  -p <port>        Control port (default: 8080)" << std::endl;
    std::cout << "  -sp <port>       Status listener port (default: 4002)" << std::endl;
    std::cout << "  -P <azimuth>     Pan angle (deg, default: 0.0)" << std::endl;
    std::cout << "  -T <elevation>   Tilt angle (deg, default: 0.0)" << std::endl;
    std::cout << "  -Z <zoom>        Zoom (0-100, default: 0)" << std::endl;
    std::cout << "  -AS <az_speed>   Pan speed (deg/s, default: 1.5)" << std::endl;
    std::cout << "  -ES <el_speed>   Tilt speed (deg/s, default: 0.5)" << std::endl;
    std::cout << "  -s <seq>         Packet sequence number (default: 0x01)" << std::endl;
    std::cout << "  -c <ctrl>        Control byte (default: 0x11)" << std::endl;
    std::cout << "  -d <devtype>     Device type (default: 0x01)" << std::endl;
    std::cout << "  -i <devip>       Device IP (default: 0x00)" << std::endl;
    std::cout << "  -q <interval>    Query interval for status polling in seconds (default: 2)" << std::endl;
    std::cout << "  -i               Interactive mode: loop input PTZ values" << std::endl;
    std::cout << "  -s               Status only (do not send control commands)" << std::endl;
    std::cout << "  -v               Verbose output" << std::endl;
    std::cout << "  -h, --help       Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << prog_name << " -P 45.0 -T 30.0 -Z 50" << std::endl;
    std::cout << "      Move camera to Pan=45, Tilt=30, Zoom=50" << std::endl;
    std::cout << "  " << prog_name << " -H 172.17.88.15 -p 1234 -P 100 -T 30 -Z 10" << std::endl;
    std::cout << "      Move camera with custom IP and port" << std::endl;
    std::cout << "  " << prog_name << " -H 172.17.88.15 -p 1234 -P 100 -T 30 -Z 10 -s 0 -c 9 -d 48 -i 1" << std::endl;
    std::cout << "      Move camera with matching protocol parameters" << std::endl;
    std::cout << "  -q <interval>  Status query interval in seconds (default: 2)" << std::endl;
    std::cout << "  --interactive    Interactive mode: loop input PTZ values" << std::endl;
}

/**
 * @brief 交互式输入 PTZ 值
 */
static bool interactive_input(float& az, float& el, float& zoom) {
    std::cout << std::endl;
    std::cout << "Enter PTZ values (enter 'q' to quit, 'r' to reset to defaults):" << std::endl;
    std::cout << "  P (Pan, deg): ";
    
    std::string input;
    if (!(std::cin >> input)) {
        return false;
    }
    
    if (input == "q" || input == "Q") {
        return false;
    }
    if (input == "r" || input == "R") {
        az = 0.0f;
        el = 0.0f;
        zoom = 0.0f;
        return true;
    }
    
    try {
        az = std::stof(input);
    } catch (...) {
        std::cerr << "Invalid pan value!" << std::endl;
        return false;
    }
    
    std::cout << "  T (俯仰角, 度): ";
    if (!(std::cin >> input)) {
        return false;
    }
    try {
        el = std::stof(input);
    } catch (...) {
        std::cerr << "Invalid tilt value!" << std::endl;
        return false;
    }
    
    std::cout << "  Z (变焦, 0-100): ";
    if (!(std::cin >> input)) {
        return false;
    }
    try {
        zoom = std::stof(input);
        if (zoom < 0.0f) zoom = 0.0f;
        if (zoom > 100.0f) zoom = 100.0f;
    } catch (...) {
        std::cerr << "Invalid zoom value!" << std::endl;
        return false;
    }
    
    return true;
}

/**
 * @brief 打印十六进制报文
 */
static std::string print_hex_packet(const uint8_t* data, int len) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (int i = 0; i < len && i < 72; i++) {
        oss << std::setw(2) << (int)data[i] << " ";
    }
    return oss.str();
}

/**
 * @brief 状态轮询线程函数
 * 定期获取并打印摄像机 PTZ 状态
 */
static void status_polling_thread(cammon::CamAJFLib& cam, std::atomic<bool>& running, int interval_sec) {
    std::cout << "\n[状态轮询] 开始每 " << interval_sec << " 秒查询摄像机 PTZ 状态" << std::endl;
    
    while (running) {
        // 获取 PTZ 状态
        cammon::PTZStatus ptz = cam.get_ptz();
        
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_buf);
#endif
        
        std::cout << "\n[" << std::put_time(&tm_buf, "%H:%M:%S") << "] ";
        
        if (ptz.valid) {
            std::cout << "摄像机 PTZ 状态: "
                      << "方位角=" << std::fixed << std::setprecision(2) << ptz.azimuth << "°, "
                      << "俯仰角=" << ptz.elevation << "°, "
                      << "变焦=" << ptz.zoom << ", "
                      << "焦距=" << ptz.focus << "mm"
                      << std::endl;
        } else {
            std::cout << "摄像机 PTZ 状态: 无效数据" << std::endl;
        }
        
        // 等待间隔或被取消
        for (int i = 0; i < interval_sec * 10 && running; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    std::cout << "[状态轮询] 线程已停止" << std::endl;
}

int main(int argc, char* argv[]) {
    // 默认参数
    std::string host = "193.0.1.94";
    int port = 8080;
    int status_port = 4002;
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float zoom = 0.0f;
    float az_speed = 1.5f;
    float el_speed = 0.5f;
    
    // 协议参数（默认值，用户可通过命令行修改）
    uint8_t seq = 0x01;          // 序列号
    uint8_t ctrl = 0x09;         // 控制字节 (位置模式 1001b)
    uint8_t devtype = 0x01;      // 设备类型
    uint8_t devip = 0x00;        // 设备IP
    
    int query_interval = 2;      // 状态查询间隔（秒）
    bool interactive = false;
    bool status_only = false;
    bool verbose = false;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-H") {
            if (i + 1 < argc) {
                host = argv[++i];
            } else {
                std::cerr << "Error: -H requires an IP address" << std::endl;
                return 1;
            }
        } else if (arg == "-p") {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
                if (port <= 0) {
                    std::cerr << "Error: invalid port number" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: -p requires a port number" << std::endl;
                return 1;
            }
        } else if (arg == "-sp") {
            if (i + 1 < argc) {
                status_port = std::atoi(argv[++i]);
                if (status_port <= 0) {
                    std::cerr << "Error: invalid status port number" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: -sp requires a port number" << std::endl;
                return 1;
            }
        } else if (arg == "-P") {
            if (i + 1 < argc) {
                azimuth = std::atof(argv[++i]);
            } else {
                std::cerr << "Error: -P requires a pan (azimuth) value" << std::endl;
                return 1;
            }
        } else if (arg == "-T") {
            if (i + 1 < argc) {
                elevation = std::atof(argv[++i]);
            } else {
                std::cerr << "Error: -T requires a tilt (elevation) value" << std::endl;
                return 1;
            }
        } else if (arg == "-Z") {
            if (i + 1 < argc) {
                zoom = std::atof(argv[++i]);
            } else {
                std::cerr << "Error: -Z requires a zoom value" << std::endl;
                return 1;
            }
        } else if (arg == "-AS") {
            if (i + 1 < argc) {
                az_speed = std::atof(argv[++i]);
            } else {
                std::cerr << "Error: -AS requires pan speed value" << std::endl;
                return 1;
            }
        } else if (arg == "-ES") {
            if (i + 1 < argc) {
                el_speed = std::atof(argv[++i]);
            } else {
                std::cerr << "Error: -ES requires tilt speed value" << std::endl;
                return 1;
            }
        } else if (arg == "-s" && i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
            // 序列号参数（需要区分 -s 和 -s 单独使用）
            if (status_only) {
                // 之前已经设置了 status_only，这是值
                seq = (uint8_t)std::strtoul(argv[i + 1], nullptr, 10);
                i++;
            } else {
                status_only = true;
                i++;
            }
        } else if (arg == "--seq") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                seq = (uint8_t)std::strtoul(val.c_str(), nullptr, 0);  // 支持 0x 进制
            } else {
                std::cerr << "Error: --seq requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-c") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                ctrl = (uint8_t)std::strtoul(val.c_str(), nullptr, 0);
            } else {
                std::cerr << "Error: -c requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-d") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                devtype = (uint8_t)std::strtoul(val.c_str(), nullptr, 0);
            } else {
                std::cerr << "Error: -d requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-i") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                devip = (uint8_t)std::strtoul(val.c_str(), nullptr, 0);
            } else {
                std::cerr << "Error: -i requires a value" << std::endl;
                return 1;
            }
        } else if (arg == "-q") {
            if (i + 1 < argc) {
                query_interval = std::atoi(argv[++i]);
                if (query_interval <= 0) {
                    std::cerr << "Error: invalid query interval" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: -q requires an interval value" << std::endl;
                return 1;
            }
        } else if (arg == "--interactive") {
            interactive = true;
        } else if (arg == "-v") {
            verbose = true;
        } else {
            std::cerr << "未知选项: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "PTZ Control Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "目标设备: " << host << ":" << port << std::endl;
    std::cout << "状态端口: " << status_port << std::endl;
    std::cout << "协议参数: seq=0x" << std::hex << (int)seq 
              << ", ctrl=0x" << (int)ctrl
              << ", devtype=0x" << (int)devtype
              << ", devip=0x" << (int)devip
              << std::dec << std::endl;
    std::cout << "状态轮询间隔: " << query_interval << " 秒" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 创建并初始化 Camera AJF 库
    cammon::CamAJFLib cam;
    cammon::CameraConfig config(host, port, 2000, status_port);
    
    // 设置协议参数
    config.seq = seq;
    config.ctrl = ctrl;
    config.devtype = devtype;
    config.devip = devip;
    // 厂家抓包格式需要将舵机帧放入标准帧的 data 区进行传输
    // 启用此选项以生成外层标准帧 (0x0F 0xF0 ... 0xF0 0x0F)
    config.wrap_servo_in_standard_packet = true;
    
    if (verbose) {
        std::cout << "[详细] 配置: host=" << host << ", port=" << port 
                  << ", status_port=" << status_port << std::endl;
        std::cout << "[详细] 协议参数: seq=0x" << std::hex << (int)config.seq 
                  << ", ctrl=0x" << (int)config.ctrl
                  << ", devtype=0x" << (int)config.devtype
                  << ", devip=0x" << (int)config.devip << std::dec << std::endl;
    }
    
    if (!cam.initWithConfig(config)) {
        std::cerr << "Error: failed to initialize camera control library" << std::endl;
        return 1;
    }
    
    // 完全禁用状态监听与轮询（仅发送控制命令）
    // 如果需要恢复状态查询，请手动启用并确保设备在线
    if (verbose) {
        std::cout << "[Verbose] status listener permanently disabled (sending control only)" << std::endl;
    }
    
    // 保留 PTZ 回调注册（在未启用状态监听时不会被触发）
    cam.on_ptz_update([](float az, float el, float z, float f, bool valid) {
        if (valid) {
            std::cout << "[回调] PTZ 更新: az=" << az << "°, el=" << el 
                      << "°, zoom=" << z << ", focus=" << f << "mm" << std::endl;
        }
    });

    // 注册报文发送回调：打印实际发送的完整报文（包含外层帧头/帧尾）
    cam.on_packet_sent([](const uint8_t* data, int len) {
        if (!data || len <= 0) return;
        std::cout << "Sent packet (" << len << " bytes): ";
        std::cout << std::hex << std::uppercase << std::setfill('0');
        for (int i = 0; i < len; ++i) {
            std::cout << std::setw(2) << (int)data[i] << " ";
        }
        std::cout << std::dec << std::nouppercase << std::endl;
    });
    
    // 不创建状态轮询线程
    std::atomic<bool> running(false);
    std::thread poll_thread;
    
    if (status_only) {
        // 只显示当前 PTZ 状态
        std::cout << "Getting current PTZ status..." << std::endl;
        cammon::PTZStatus ptz = cam.get_ptz();
        if (ptz.valid) {
            std::cout << "Current PTZ status:" << std::endl;
            std::cout << "  Pan:  " << ptz.azimuth << " deg" << std::endl;
            std::cout << "  Tilt: " << ptz.elevation << " deg" << std::endl;
            std::cout << "  Zoom: " << ptz.zoom << std::endl;
            std::cout << "  Focus:" << ptz.focus << " mm" << std::endl;
        } else {
            std::cout << "No valid PTZ status available" << std::endl;
        }
        
        running = false;
        if (poll_thread.joinable()) {
            poll_thread.join();
        }
        cam.stop();
        cam.shutdown();
        return 0;
    }
    
    // 主循环
    int command_count = 0;
    do {
        if (interactive) {
            // Interactive mode: user input PTZ values
            if (!interactive_input(azimuth, elevation, zoom)) {
                std::cout << "User exited interactive mode" << std::endl;
                break;
            }
        }
        
        if (verbose) {
            std::cout << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "Sending PTZ command #" << (command_count + 1) << std::endl;
            std::cout << "  Pan:    " << azimuth << " deg" << std::endl;
            std::cout << "  Tilt:   " << elevation << " deg" << std::endl;
            std::cout << "  Zoom:   " << zoom << std::endl;
            std::cout << "  Pan speed:  " << az_speed << " deg/s" << std::endl;
            std::cout << "  Tilt speed: " << el_speed << " deg/s" << std::endl;
            std::cout << "========================================" << std::endl;
        } else {
            std::cout << "Set PTZ: P=" << azimuth << " deg, T=" << elevation
                      << " deg, Z=" << zoom << std::endl;
        }
        
        // 在发送 PTZ 控制命令前，构建并显示完整的舵机报文原文（72 字节）
        {
            cammon::CameraConfig cfg = cam.get_config();
            std::vector<uint8_t> packet = cammon::build_servo_packet(
                azimuth, elevation, az_speed, el_speed,
                /*target_distance=*/0,
                /*seq=*/cfg.seq,
                /*control=*/cfg.ctrl,
                /*device_type=*/cfg.devtype,
                /*packet_type=*/cammon::SERVO_PACKET_TYPE_POINT
            );

            // 填入当前焦距到包内，并重算校验（与库内相同的偏移和逻辑）
            cammon::PTZStatus current = cam.get_ptz();
            float vis_focus = current.focus;
            float ir_focus = current.focus;
            if (packet.size() >= 72) {
                packet[42] = 0x00; // 焦距单位 (0x00 = mm)
                uint32_t vbits = 0;
                std::memcpy(&vbits, &vis_focus, sizeof(vbits));
                packet[43] = static_cast<uint8_t>(vbits & 0xFF);
                packet[44] = static_cast<uint8_t>((vbits >> 8) & 0xFF);
                packet[45] = static_cast<uint8_t>((vbits >> 16) & 0xFF);
                packet[46] = static_cast<uint8_t>((vbits >> 24) & 0xFF);
                std::memcpy(&vbits, &ir_focus, sizeof(vbits));
                packet[47] = static_cast<uint8_t>(vbits & 0xFF);
                packet[48] = static_cast<uint8_t>((vbits >> 8) & 0xFF);
                packet[49] = static_cast<uint8_t>((vbits >> 16) & 0xFF);
                packet[50] = static_cast<uint8_t>((vbits >> 24) & 0xFF);
                uint8_t cs = 0;
                for (size_t i = 0; i < 71 && i < packet.size(); ++i) cs ^= packet[i];
                if (packet.size() > 71) packet[71] = cs;
            }

            // 打印完整报文到 stdout（便于用户在发出前查看）
            std::cout << "Raw packet before send (" << packet.size() << " bytes): ";
            std::cout << std::hex << std::uppercase << std::setfill('0');
            for (size_t i = 0; i < packet.size(); ++i) {
                std::cout << std::setw(2) << (int)packet[i] << " ";
            }
            std::cout << std::dec << std::nouppercase << std::endl;
        }

        // 发送 PTZ 控制命令
        bool success = cam.set_ptz(azimuth, elevation, zoom, az_speed, el_speed);
        
        if (success) {
            std::cout << "PTZ command sent successfully" << std::endl;
            command_count++;

            // 按用户要求：在发送成功后立即退出到此处（清理资源并返回）
            running = false;
            if (poll_thread.joinable()) {
                poll_thread.join();
            }
            cam.stop();
            cam.shutdown();
            return 0;
        } else {
            std::cerr << "PTZ command send failed" << std::endl;
        }
        
        // 非交互模式下循环结束后退出
        if (!interactive) {
            break;
        }
        
        std::cout << std::endl;
    } while (true);
    
    // 停止状态轮询线程
    running = false;
    if (poll_thread.joinable()) {
        poll_thread.join();
    }
    
    // 清理资源
    cam.stop();
    cam.shutdown();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "PTZ test finished, sent " << command_count << " commands" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}