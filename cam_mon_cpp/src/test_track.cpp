/**************************************************************************
* 版权所有(C), 2026，海华电子企业（中国）有限公司
* 文件名：		test_track.cpp
* 作  者：		marblech
* 版  本：		v1.0.0
* 日  期：		2026/07/16
* 文件描述:		cam_controller_set_ptz_track 单完测试程序
*				测试舵机跟踪命令的构建和发送功能，包括：
*				- 构建并显示舵机跟踪数据包（72字节）
*				- 调用 cam_controller_set_ptz_track 发送跟踪命令
*				- 测试不同的 diffPan/diffTilt/zoom/action 参数组合
* 函数列表： 	main		主函数
*				print_usage	打印使用说明
* 修改历史:
* 修改日期：2026/07/16
* 修改者：marblech
* 修改内容：初始版本创建
***************************************************************************/

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
#include "cam_controller.h"
#include "protocol.h"

/**
 * @brief 打印使用说明
 */
static void print_usage(const char* prog_name) {
    std::cout << "PTZ Track Test - 舵机跟踪命令测试程序" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Usage: " << prog_name << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -H <host>        Target device IP (default: 127.0.0.1)" << std::endl;
    std::cout << "  -p <port>        Control port (default: 8080)" << std::endl;
    std::cout << "  -sp <port>       Status listener port (default: 4002)" << std::endl;
    std::cout << "  -P <diffPan>     Pan diff angle (deg, default: 0.5)" << std::endl;
    std::cout << "  -T <diffTilt>    Tilt diff angle (deg, default: 0.3)" << std::endl;
    std::cout << "  -Z <zoom>        Zoom (7.0-560.0 for focal direct, default: 0)" << std::endl;
    std::cout << "  -d <devtype>     Device type (default: 0x01)" << std::endl;
    std::cout << "  -a <action>      Action type (0=none, 1=zoom, default: 1)" << std::endl;
    std::cout << "  -s <seq>         Sequence number for display (default: 0x01)" << std::endl;
    std::cout << "  --build-only     Only build and display the packet, do not send" << std::endl;
    std::cout << "  -v               Verbose output" << std::endl;
    std::cout << "  -h, --help       Show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << prog_name << " -H 192.168.1.100 -p 8080 -P 0.5 -T 0.3" << std::endl;
    std::cout << "      Send track command with diffPan=0.5, diffTilt=0.3" << std::endl;
    std::cout << "  " << prog_name << " -H 192.168.1.100 -p 8080 -P 1.0 -T -0.5 -Z 50.0 -a 1" << std::endl;
    std::cout << "      Send track command with zoom=50, action=SET_ZOOM" << std::endl;
    std::cout << "  " << prog_name << " -P 2.0 -T 1.5 --build-only" << std::endl;
    std::cout << "      Only build and display the tracking packet without sending" << std::endl;
}

/**
 * @brief 将字节数组格式化为十六进制字符串
 * @param data 字节数据
 * @param len 数据长度
 * @return std::string 格式化后的十六进制字符串
 */
static std::string format_hex(const uint8_t* data, int len) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (int i = 0; i < len; ++i) {
        if (i > 0 && i % 16 == 0) {
            oss << "\n    ";
        } else if (i > 0) {
            oss << " ";
        }
        oss << std::setw(2) << (int)data[i];
    }
    return oss.str();
}

/**
 * @brief 构建舵机跟踪数据包并显示各部分字段含义
 * @param diffPan 方位角差值（度）
 * @param diffTilt 俯仰角差值（度）
 * @param seq 序列号
 * @param action 动作类型
 * @param device_type 设备类型
 * @param packet_type 数据包类型
 * @return std::vector<uint8_t> 序列化后的 72 字节向量
 */
static std::vector<uint8_t> build_and_display_tracking_packet(
    float diffPan, float diffTilt, uint8_t seq,
    int action, uint8_t device_type, uint8_t packet_type)
{
    // 使用协议库构建跟踪数据包
    std::vector<uint8_t> packet = cammon::build_servo_packet_tracking(
        diffPan, diffTilt, seq, static_cast<uint8_t>(action),
        device_type, packet_type);

    std::cout << "\n===== 舵机跟踪数据包 (72 字节) =====" << std::endl;

    // 尝试解析并显示各字段
    auto parsed = cammon::ServoPacket::deserialize_servo(packet);
    if (parsed) {
        cammon::ServoPacket& s = *parsed;
        std::cout << "  帧头:         0x" << std::hex << (int)s.header << std::dec << std::endl;
        std::cout << "  帧长:         0x" << std::hex << (int)s.frame_len << " (" << (int)s.frame_len << ")" << std::dec << std::endl;
        std::cout << "  序列号:       0x" << std::hex << (int)s.seq << std::dec << std::endl;
        std::cout << "  设备类型:     0x" << std::hex << (int)s.device_type << std::dec << std::endl;
        std::cout << "  包类型:       0x" << std::hex << (int)s.packet_type << std::dec;
        switch (s.packet_type) {
            case 0x01: std::cout << " (广播)"; break;
            case 0x02: std::cout << " (定点)"; break;
            case 0x03: std::cout << " (秒同步)"; break;
            case 0x04: std::cout << " (链路测试)"; break;
            default: break;
        }
        std::cout << std::endl;
        std::cout << "  设备IP:       0x" << std::hex << (int)s.device_ip << std::dec << std::endl;
        std::cout << "  主控连接:     0x" << std::hex << (int)s.main_conn << std::dec << std::endl;
        std::cout << "  控制字节:     0x" << std::hex << (int)s.control << std::dec;
        if ((s.control & 0x0F) == 0x0B) {
            std::cout << " (跟踪启动)";
        } else if ((s.control & 0x0F) == 0x0A) {
            std::cout << " (跟踪停止)";
        } else if ((s.control & 0x0F) == 0x09) {
            std::cout << " (位置模式)";
        }
        std::cout << std::endl;
        std::cout << "  雨刷/加热:    0x" << std::hex << (int)s.wiper_heater_ctrl << std::dec << std::endl;
        std::cout << "  方位角:       " << s.azimuth << "°" << std::endl;
        std::cout << "  俯仰角:       " << s.elevation << "°" << std::endl;
        std::cout << "  方位速度:     " << s.az_speed << "°/s" << std::endl;
        std::cout << "  俯仰速度:     " << s.el_speed << "°/s" << std::endl;
        std::cout << "  目标距离:     " << s.target_distance << " mm" << std::endl;
        std::cout << "  俯仰偏差:     " << s.target_el_aberration << std::endl;
        std::cout << "  跟踪方位偏差: " << s.track_az_aberration << "°" << std::endl;
        std::cout << "  跟踪俯仰偏差: " << s.track_el_aberration << "°" << std::endl;
        std::cout << "  视场角度:     " << s.fov_angle << "°" << std::endl;
        std::cout << "  红外上电:     0x" << std::hex << (int)s.ir_power_ctrl << std::dec << std::endl;
        std::cout << "  焦距单位:     0x" << std::hex << (int)s.vis_focal_unit << std::dec;
        if (s.vis_focal_unit == 0x00) std::cout << " (mm)";
        std::cout << std::endl;
        std::cout << "  白光焦距:     " << s.vis_focal_value << " mm" << std::endl;
        std::cout << "  红外焦距:     " << s.ir_focal_value << " mm" << std::endl;
        std::cout << "  时间戳:       " << s.timestamp << std::endl;
        std::cout << "  跟踪状态:     0x" << std::hex << (int)s.track_status << std::dec << std::endl;
        std::cout << "  备份(动作):   " << s.backup;
        if (s.backup > 0) std::cout << " (跟踪动作=" << s.backup << ")";
        std::cout << std::endl;
        std::cout << "  校验和:       0x" << std::hex << (int)s.checksum << std::dec << std::endl;
    } else {
        std::cout << "  (无法解析)" << std::endl;
    }

    std::cout << "\n原始报文 (十六进制):" << std::endl;
    std::cout << "  " << format_hex(packet.data(), (int)packet.size()) << std::endl;
    std::cout << "  总长度: " << packet.size() << " 字节" << std::endl;

    // 构建并显示外层标准帧封装
    cammon::Packet outer;
    outer.addr = cammon::ADDR_SERVO;
    outer.func = 0x00;
    outer.ctrl = 0x00;
    outer.data = packet;
    auto outer_bytes = outer.serialize();

    std::cout << "\n外层标准帧封装 (" << outer_bytes.size() << " 字节):" << std::endl;
    std::cout << "  " << format_hex(outer_bytes.data(), (int)outer_bytes.size()) << std::endl;
    std::cout << "=========================================" << std::endl;

    return packet;
}

/**
 * @brief 测试各种参数组合
 * @param host 目标主机地址
 * @param port 目标端口
 * @param build_only 仅构建不发送
 */
static void run_battery_tests(const std::string& host, int port, bool build_only) {
    struct TestCase {
        const char* desc;
        float diffPan;
        float diffTilt;
        float zoom;
        uint8_t device_type;
        int action;
        uint8_t seq;
    };

    TestCase cases[] = {
        {"跟踪启动 - 正向小偏差",          0.5f,   0.3f,   0.0f,   0x01, 1, 0x01},
        {"跟踪启动 - 负向偏差",            -1.0f,  -0.5f,   0.0f,   0x01, 1, 0x01},
        {"跟踪启动 - 大偏差",              5.0f,   3.0f,    50.0f,  0x01, 1, 0x02},
        {"跟踪停止 (action=0)",            0.0f,   0.0f,    0.0f,   0x01, 0, 0x03},
        {"可见光设备 (devtype=0)",         1.0f,   0.5f,    0.0f,   0x00, 1, 0x04},
        {"红外设备 (devtype=0x30)",        2.0f,   1.0f,    100.0f, 0x30, 1, 0x05},
        {"纯跟踪无变焦",                   0.8f,   0.2f,    -1.0f,  0x01, 0, 0x06},
    };

    int case_num = 0;
    for (const auto& tc : cases) {
        case_num++;
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║  测试用例 #" << case_num << ": " << tc.desc;
        // Pad with spaces to align
        int pad_len = 44 - static_cast<int>(std::strlen(tc.desc));
        if (pad_len < 0) pad_len = 0;
        for (int i = 0; i < pad_len; ++i) std::cout << " ";
        std::cout << "║" << std::endl;
        std::cout << "╚════════════════════════════════════════════════════╝" << std::endl;

        // 构建并显示数据包
        build_and_display_tracking_packet(
            tc.diffPan, tc.diffTilt, tc.seq,
            tc.action, tc.device_type, cammon::SERVO_PACKET_TYPE_POINT);

        if (!build_only) {
            // 创建控制器并发送
            CamController* c = cam_controller_create();
            if (!c) {
                std::cerr << "  创建控制器失败！" << std::endl;
                continue;
            }

            // 启动监听（使用本地端口避免冲突）
            int listen_port = 14002 + case_num;
            int r = cam_controller_start(c, listen_port);
            if (r != 0) {
                std::cout << "  监听启动: 端口 " << listen_port << " (r=" << r << ")" << std::endl;
            } else {
                std::cout << "  监听已启动: 端口 " << listen_port << std::endl;
            }

            // 发送跟踪命令
            std::cout << "  发送跟踪命令:" << std::endl;
            std::cout << "    目标: " << host << ":" << port << std::endl;
            std::cout << "    diffPan=" << tc.diffPan << "° diffTilt=" << tc.diffTilt << "°" << std::endl;
            std::cout << "    zoom=" << tc.zoom << " device=0x" << std::hex << (int)tc.device_type 
                      << " action=" << std::dec << tc.action << std::endl;

            int result = cam_controller_set_ptz_track(
                c, host.c_str(), port,
                tc.diffPan, tc.diffTilt, tc.zoom,
                tc.device_type, tc.action);

            std::cout << "  返回结果: " << result;
            if (result > 0) {
                std::cout << " (发送成功, 收到 " << result << " 字节)";
            } else if (result == 0) {
                std::cout << " (发送成功但无响应)";
            } else {
                std::cout << " (发送失败)";
            }
            std::cout << std::endl;

            // 清理
            cam_controller_destroy(c);
        }
    }
}

/**
 * @brief 主函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return int 退出码
 */
int main(int argc, char* argv[]) {
    // 默认参数
    std::string host = "127.0.0.1";
    int port = 8080;
    int listen_port = 4002;
    float diffPan = 0.5f;
    float diffTilt = 0.3f;
    float zoom = 0.0f;
    uint8_t device_type = 0x01;
    int action = 1;  // 默认 ACTION_SET_ZOOM
    uint8_t seq = 0x01;
    bool build_only = false;
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
            }
        } else if (arg == "-p") {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
                if (port <= 0) {
                    std::cerr << "Error: invalid port number" << std::endl;
                    return 1;
                }
            }
        } else if (arg == "-sp") {
            if (i + 1 < argc) {
                listen_port = std::atoi(argv[++i]);
                if (listen_port <= 0) {
                    std::cerr << "Error: invalid status port number" << std::endl;
                    return 1;
                }
            }
        } else if (arg == "-P") {
            if (i + 1 < argc) {
                diffPan = std::atof(argv[++i]);
            }
        } else if (arg == "-T") {
            if (i + 1 < argc) {
                diffTilt = std::atof(argv[++i]);
            }
        } else if (arg == "-Z") {
            if (i + 1 < argc) {
                zoom = std::atof(argv[++i]);
            }
        } else if (arg == "-d") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                device_type = (uint8_t)std::strtoul(val.c_str(), nullptr, 0);
            }
        } else if (arg == "-a") {
            if (i + 1 < argc) {
                action = std::atoi(argv[++i]);
            }
        } else if (arg == "-s") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                seq = (uint8_t)std::strtoul(val.c_str(), nullptr, 0);
            }
        } else if (arg == "--build-only") {
            build_only = true;
        } else if (arg == "-v") {
            verbose = true;
        } else {
            std::cerr << "未知选项: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // 显示配置
    std::cout << "=========================================" << std::endl;
    std::cout << "PTZ Track Test - 舵机跟踪命令测试" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "配置文件:" << std::endl;
    std::cout << "  目标主机: " << host << ":" << port << std::endl;
    std::cout << "  监听端口: " << listen_port << std::endl;
    std::cout << "  P(方位差): " << diffPan << "°" << std::endl;
    std::cout << "  T(俯仰差): " << diffTilt << "°" << std::endl;
    std::cout << "  Z(变焦):   " << zoom << std::endl;
    std::cout << "  设备类型:  0x" << std::hex << (int)device_type << std::dec << std::endl;
    std::cout << "  动作类型:  " << action;
    if (action == 0) std::cout << " (无动作)";
    else if (action == 1) std::cout << " (设置变焦)";
    std::cout << std::endl;
    std::cout << "  序列号:    0x" << std::hex << (int)seq << std::dec << std::endl;
    std::cout << "  模式:      " << (build_only ? "仅构建报文" : "构建并发送") << std::endl;
    std::cout << "=========================================" << std::endl;

    if (build_only) {
        // 仅构建模式：运行所有测试用例展示报文结构
        std::cout << "\n运行全部测试用例（仅构建报文）..." << std::endl;
        run_battery_tests(host, port, true);
        std::cout << "\n所有测试用例完成。使用 --build-only 模式，未发送任何网络数据包。" << std::endl;
        return 0;
    }

    // 单命令模式：构建并显示数据包，然后发送
    std::cout << "\n构建舵机跟踪数据包..." << std::endl;

    // 使用 build_servo_packet_tracking 构建原始舵机包
    std::vector<uint8_t> raw_packet = build_and_display_tracking_packet(
        diffPan, diffTilt, seq, action, device_type,
        cammon::SERVO_PACKET_TYPE_POINT);

    // 创建控制器
    CamController* c = cam_controller_create();
    if (!c) {
        std::cerr << "错误: 创建控制器失败！" << std::endl;
        return 1;
    }

    // 启动监听
    int r = cam_controller_start(c, listen_port);
    if (r != 0) {
        std::cout << "监听启动返回: " << r << " (非致命，继续发送)" << std::endl;
    } else {
        std::cout << "监听器已启动在端口 " << listen_port << std::endl;
    }

    // 让监听线程有时间初始化
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 调用 cam_controller_set_ptz_track
    std::cout << "\n>> 调用 cam_controller_set_ptz_track ..." << std::endl;

    int result = cam_controller_set_ptz_track(
        c, host.c_str(), port,
        diffPan, diffTilt, zoom,
        device_type, action);

    std::cout << "返回代码: " << result << std::endl;
    if (result > 0) {
        std::cout << "  -> 跟踪命令发送成功，收到 " << result << " 字节响应" << std::endl;
    } else if (result == 0) {
        std::cout << "  -> 跟踪命令发送完成（无响应数据）" << std::endl;
    } else {
        std::cout << "  -> 跟踪命令发送失败 (错误码: " << result << ")" << std::endl;
    }

    // 清理
    std::cout << "\n清理资源..." << std::endl;
    cam_controller_destroy(c);

    std::cout << "\n测试完成。" << std::endl;
    return (result >= 0) ? 0 : 1;
}