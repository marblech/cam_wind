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
#include <cmath>
#include <algorithm>
#include "cam_ajf_lib.h"

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
    std::cout << "  -P <azimuth>     Pan angle (deg, default: 0.0)" << std::endl;
    std::cout << "  -T <elevation>   Tilt angle (deg, default: 0.0)" << std::endl;
    std::cout << "  -Z <zoom>        Zoom (0-100, default: 0)" << std::endl;
    std::cout << "  -AS <az_speed>   Pan speed (deg/s, default: 1.5)" << std::endl;
    std::cout << "  -ES <el_speed>   Tilt speed (deg/s, default: 0.5)" << std::endl;
    std::cout << "  -i               Interactive mode: loop input PTZ values" << std::endl;
    std::cout << "  -s               Status only (do not send control commands)" << std::endl;
    std::cout << "  -v               Verbose output" << std::endl;
    std::cout << "  -h, --help       Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << prog_name << " -P 45.0 -T 30.0 -Z 50" << std::endl;
    std::cout << "      Move camera to Pan=45, Tilt=30, Zoom=50" << std::endl;
    std::cout << "  " << prog_name << " -i" << std::endl;
    std::cout << "      Enter interactive mode to input PTZ values repeatedly" << std::endl;
    std::cout << "  " << prog_name << " -s" << std::endl;
    std::cout << "      Show current PTZ status only" << std::endl;
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
        std::cerr << "Error: failed to initialize camera control library" << std::endl;
        return 1;
    }
    
    // 启动监听线程
    if (!cam.start()) {
        std::cerr << "Error: failed to start status listener thread" << std::endl;
        cam.shutdown();
        return 1;
    }
    
    if (verbose) {
        std::cout << "[Verbose] status listener started" << std::endl;
    }
    
    // 设置 PTZ 状态回调
    cam.on_ptz_update([](float az, float el, float z, float f, bool valid) {
        if (valid) {
            std::cout << "[Status] Azimuth=" << az << " deg, Elevation=" << el
                      << " deg, Zoom=" << z << ", Focus=" << f << " mm" << std::endl;
        }
    });
    
    if (status_only) {
        // Show current PTZ status only
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
        
        // 发送 PTZ 控制命令
        bool success = cam.set_ptz(azimuth, elevation, zoom, az_speed, el_speed);
        
        if (success) {
            std::cout << "PTZ command sent successfully" << std::endl;
            command_count++;
            
            if (!interactive) {
                // 非交互模式：等待一段时间让摄像机移动
                std::cout << "Waiting for camera movement..." << std::endl;
                // 根据距离估算移动时间，至少等待1秒
                int wait_time = static_cast<int>(std::max(
                    std::abs(azimuth) / az_speed,
                    std::abs(elevation) / el_speed
                )) + 1;
                if (wait_time < 1) wait_time = 1;
                std::cout << "Estimated move time: " << wait_time << " seconds" << std::endl;
                
                // 简单休眠等待
                for (int i = 0; i < wait_time * 10; i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    std::cout << ".";
                    std::cout.flush();
                }
                std::cout << std::endl;
                
                // Get PTZ status after movement
                std::cout << "PTZ status after move:" << std::endl;
                cammon::PTZStatus ptz = cam.get_ptz();
                if (ptz.valid) {
                    std::cout << "  Pan:  " << ptz.azimuth << " deg" << std::endl;
                    std::cout << "  Tilt: " << ptz.elevation << " deg" << std::endl;
                    std::cout << "  Zoom: " << ptz.zoom << std::endl;
                    std::cout << "  Focus:" << ptz.focus << " mm" << std::endl;
                } else {
                    std::cout << "  No valid PTZ status available" << std::endl;
                }
            }
        } else {
            std::cerr << "PTZ command send failed" << std::endl;
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
    std::cout << "PTZ test finished, sent " << command_count << " commands" << std::endl;
    
    return 0;
}