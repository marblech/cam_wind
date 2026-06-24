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
#include <cstring>
#include "cam_ajf_lib.h"

/**
 * @brief 打印使用说明
 */
static void print_usage(const char* prog_name) {
    std::cout << "PTZ Control Test - 摄像机位置与变焦控制" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "用法: " << prog_name << " [选项]" << std::endl;
    std::cout << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -H <host>        目标设备 IP 地址 (默认: 193.0.1.94)" << std::endl;
    std::cout << "  -p <port>        控制端口号 (默认: 8080)" << std::endl;
    std::cout << "  -P <azimuth>     方位角 Pan (度, 默认: 0.0)" << std::endl;
    std::cout << "  -T <elevation>   俯仰角 Tilt (度, 默认: 0.0)" << std::endl;
    std::cout << "  -Z <zoom>        变焦 Zoom (范围: 0-100, 默认: 0)" << std::endl;
    std::cout << "  -AS <az_speed>   方位角移动速度 度/秒, 默认: 1.5)" << std::endl;
    std::cout << "  -ES <el_speed>   俯仰角移动速度 度/秒, 默认: 0.5)" << std::endl;
    std::cout << "  -i               交互模式：循环输入 PTZ 值" << std::endl;
    std::cout << "  -s               只显示当前 PTZ 状态（不发送控制命令）" << std::endl;
    std::cout << "  -v               详细输出模式" << std::endl;
    std::cout << "  -h, --help       显示此帮助信息" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << prog_name << " -P 45.0 -T 30.0 -Z 50" << std::endl;
    std::cout << "      将摄像机转到方位角45度、俯仰角30度、变焦50的位置" << std::endl;
    std::cout << "  " << prog_name << " -i" << std::endl;
    std::cout << "      进入交互模式，可连续输入 PTZ 值" << std::endl;
    std::cout << "  " << prog_name << " -s" << std::endl;
    std::cout << "      只显示当前 PTZ 状态" << std::endl;
}

/**
 * @brief 交互式输入 PTZ 值
 */
static bool interactive_input(float& az, float& el, float& zoom) {
    std::cout << std::endl;
    std::cout << "请输入 PTZ 值（输入 'q' 退出，'r' 重置为默认值）:" << std::endl;
    std::cout << "  P (方位角, 度): ";
    
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
        std::cerr << "无效的方位角值!" << std::endl;
        return false;
    }
    
    std::cout << "  T (俯仰角, 度): ";
    if (!(std::cin >> input)) {
        return false;
    }
    try {
        el = std::stof(input);
    } catch (...) {
        std::cerr << "无效的俯仰角值!" << std::endl;
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
        std::cerr << "无效的变焦值!" << std::endl;
        return false;
    }
    
    return true;
}

int main(int argc, char* argv[]) {
    // 默认参数
    std::string host = "193.0.1.94";
    int port = 8080;
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float zoom = 0.0f;
    float az_speed = 1.5f;
    float el_speed = 0.5f;
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
                std::cerr << "错误: -H 需要后跟 IP 地址" << std::endl;
                return 1;
            }
        } else if (arg == "-p") {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
                if (port <= 0) {
                    std::cerr << "错误: 无效的端口号" << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "错误: -p 需要后跟端口号" << std::endl;
                return 1;
            }
        } else if (arg == "-P") {
            if (i + 1 < argc) {
                azimuth = std::atof(argv[++i]);
            } else {
                std::cerr << "错误: -P 需要后跟方位角值" << std::endl;
                return 1;
            }
        } else if (arg == "-T") {
            if (i + 1 < argc) {
                elevation = std::atof(argv[++i]);
            } else {
                std::cerr << "错误: -T 需要后跟俯仰角值" << std::endl;
                return 1;
            }
        } else if (arg == "-Z") {
            if (i + 1 < argc) {
                zoom = std::atof(argv[++i]);
            } else {
                std::cerr << "错误: -Z 需要后跟变焦值" << std::endl;
                return 1;
            }
        } else if (arg == "-AS") {
            if (i + 1 < argc) {
                az_speed = std::atof(argv[++i]);
            } else {
                std::cerr << "错误: -AS 需要后跟方位角速度" << std::endl;
                return 1;
            }
        } else if (arg == "-ES") {
            if (i + 1 < argc) {
                el_speed = std::atof(argv[++i]);
            } else {
                std::cerr << "错误: -ES 需要后跟俯仰角速度" << std::endl;
                return 1;
            }
        } else if (arg == "-i") {
            interactive = true;
        } else if (arg == "-s") {
            status_only = true;
        } else if (arg == "-v") {
            verbose = true;
        } else {
            std::cerr << "未知选项: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // 创建并初始化 Camera AJF 库
    cammon::CamAJFLib cam;
    cammon::CameraConfig config(host, port);
    
    if (verbose) {
        std::cout << "[详细] 配置: host=" << host << ", port=" << port << std::endl;
    }
    
    if (!cam.initWithConfig(config)) {
        std::cerr << "错误: 初始化摄像头控制库失败" << std::endl;
        return 1;
    }
    
    // 启动监听线程
    if (!cam.start()) {
        std::cerr << "错误: 启动状态监听线程失败" << std::endl;
        cam.shutdown();
        return 1;
    }
    
    if (verbose) {
        std::cout << "[详细] 状态监听线程已启动" << std::endl;
    }
    
    // 设置 PTZ 状态回调
    cam.on_ptz_update([](float az, float el, float z, float f, bool valid) {
        if (valid) {
            std::cout << "[状态更新] Azimuth=" << az << "°, Elevation=" << el 
                      << "°, Zoom=" << z << ", Focus=" << f << "mm" << std::endl;
        }
    });
    
    if (status_only) {
        // 只显示当前 PTZ 状态
        std::cout << "获取当前 PTZ 状态..." << std::endl;
        cammon::PTZStatus ptz = cam.get_ptz();
        if (ptz.valid) {
            std::cout << "当前 PTZ 状态:" << std::endl;
            std::cout << "  方位角 (Pan):  " << ptz.azimuth << "°" << std::endl;
            std::cout << "  俯仰角 (Tilt): " << ptz.elevation << "°" << std::endl;
            std::cout << "  变焦 (Zoom):   " << ptz.zoom << std::endl;
            std::cout << "  焦距 (Focus):  " << ptz.focus << "mm" << std::endl;
        } else {
            std::cout << "未获取到有效的 PTZ 状态数据" << std::endl;
        }
        
        cam.stop();
        cam.shutdown();
        return 0;
    }
    
    // 主循环
    int command_count = 0;
    do {
        if (interactive) {
            // 交互模式：用户输入 PTZ 值
            if (!interactive_input(azimuth, elevation, zoom)) {
                std::cout << "用户退出交互模式" << std::endl;
                break;
            }
        }
        
        if (verbose) {
            std::cout << std::endl;
            std::cout << "========================================" << std::endl;
            std::cout << "发送 PTZ 控制命令 #" << (command_count + 1) << std::endl;
            std::cout << "  P (Pan/方位角):    " << azimuth << "°" << std::endl;
            std::cout << "  T (Tilt/俯仰角):   " << elevation << "°" << std::endl;
            std::cout << "  Z (Zoom/变焦):     " << zoom << std::endl;
            std::cout << "  方位角速度:        " << az_speed << "°/s" << std::endl;
            std::cout << "  俯仰角速度:        " << el_speed << "°/s" << std::endl;
            std::cout << "========================================" << std::endl;
        } else {
            std::cout << "设置 PTZ: P=" << azimuth << "°, T=" << elevation 
                      << "°, Z=" << zoom << std::endl;
        }
        
        // 发送 PTZ 控制命令
        bool success = cam.set_ptz(azimuth, elevation, zoom, az_speed, el_speed);
        
        if (success) {
            std::cout << "✓ PTZ 控制命令发送成功" << std::endl;
            command_count++;
            
            if (!interactive) {
                // 非交互模式：等待一段时间让摄像机移动
                std::cout << "等待摄像机移动..." << std::endl;
                // 根据距离估算移动时间，至少等待1秒
                int wait_time = static_cast<int>(std::max(
                    std::abs(azimuth) / az_speed,
                    std::abs(elevation) / el_speed
                )) + 1;
                if (wait_time < 1) wait_time = 1;
                std::cout << "预计移动时间: " << wait_time << " 秒" << std::endl;
                
                // 简单休眠等待
                for (int i = 0; i < wait_time * 10; i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    std::cout << ".";
                    std::cout.flush();
                }
                std::cout << std::endl;
                
                // 获取移动后的 PTZ 状态
                std::cout << "获取移动后 PTZ 状态:" << std::endl;
                cammon::PTZStatus ptz = cam.get_ptz();
                if (ptz.valid) {
                    std::cout << "  方位角 (Pan):  " << ptz.azimuth << "°" << std::endl;
                    std::cout << "  俯仰角 (Tilt): " << ptz.elevation << "°" << std::endl;
                    std::cout << "  变焦 (Zoom):   " << ptz.zoom << std::endl;
                    std::cout << "  焦距 (Focus):  " << ptz.focus << "mm" << std::endl;
                } else {
                    std::cout << "  未获取到有效的 PTZ 状态数据" << std::endl;
                }
            }
        } else {
            std::cerr << "✗ PTZ 控制命令发送失败" << std::endl;
        }
        
        // 非交互模式下循环结束后退出
        if (!interactive) {
            break;
        }
        
        std::cout << std::endl;
    } while (true);
    
    // 清理资源
    cam.stop();
    cam.shutdown();
    
    std::cout << std::endl;
    std::cout << "PTZ 控制测试完成，共发送 " << command_count << " 条命令" << std::endl;
    
    return 0;
}