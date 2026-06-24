/**
 * @file src/test_multicast.cpp
 * @brief 组播接收测试程序
 * @author marblech
 * @date 2026-06-24
 * @copyright SPDX: MIT OR as-project-specifies
 */

#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>

#include "protocol.h"
#include "multicast_receiver.h"
#include "plog_init.h"

using namespace cammon;

#ifdef _WIN32
#include <signal.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

// 全局标志用于控制程序退出
static std::atomic<bool> g_running(true);

#ifdef _WIN32
BOOL WINAPI controlHandler(DWORD ctrl) {
    if (ctrl == CTRL_C_EVENT) {
        PLOG_INFO << "\n[测试程序] 收到中断信号，正在退出...";
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#else
void controlHandler(int sig) {
    (void)sig;
    PLOG_INFO << "\n[测试程序] 收到中断信号，正在退出...";
    g_running = false;
}
#endif

// 统计信息
struct Stats {
    int total_packets = 0;
    int parsed_reports = 0;
    int parse_errors = 0;
};

static Stats g_stats;

// 上次显示统计的时间
static std::chrono::steady_clock::time_point g_last_stats_time;

void printStats() {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - g_last_stats_time).count();
    if (elapsed >= 5.0) { // 每5秒显示一次统计
        fprintf(stderr, "\n========== 组播接收统计 ==========\n");
        fprintf(stderr, "总接收报文数: %d\n", g_stats.total_packets);
        fprintf(stderr, "成功解析负载状态上报: %d\n", g_stats.parsed_reports);
        fprintf(stderr, "解析失败: %d\n", g_stats.parse_errors);
        if (g_stats.total_packets > 0) {
            double rate = (double)g_stats.parsed_reports / g_stats.total_packets * 100;
            fprintf(stderr, "解析成功率: %.1f%%\n", rate);
        }
        fprintf(stderr, "===================================\n\n");
        g_last_stats_time = now;
    }
}

/**
 * @brief 打印使用说明
 */
static void printUsage(const char* prog) {
    fprintf(stderr, "用法: %s [选项]\n", prog);
    fprintf(stderr, "选项:\n");
    fprintf(stderr, "  -b <addr>    本地绑定地址 (默认: 0.0.0.0)\n");
    fprintf(stderr, "  -p <port>    组播端口 (默认: %d)\n", MULTICAST_GROUP_PORT);
    fprintf(stderr, "  -m <addr>    组播组地址 (默认: %s)\n", MULTICAST_GROUP_ADDR);
    fprintf(stderr, "  -i <addr>    网络接口地址 (可选，默认自动检测)\n");
    fprintf(stderr, "  -h           显示此帮助\n");
    fprintf(stderr, "\n示例:\n");
    fprintf(stderr, "  %s                         # 全部使用默认值\n", prog);
    fprintf(stderr, "  %s -b 0.0.0.0              # 指定绑定地址\n", prog);
    fprintf(stderr, "  %s -p 23232                # 指定端口\n", prog);
    fprintf(stderr, "  %s -m 239.255.43.21        # 指定组播地址\n", prog);
    fprintf(stderr, "  %s -i 192.168.1.100        # 指定网卡接口\n", prog);
    fprintf(stderr, "  %s -b 0.0.0.0 -p 23232 -m 239.255.43.21 -i 192.168.1.100\n", prog);
}

int main(int argc, char* argv[]) {
    initPlog();

    // 设置信号处理
#ifdef _WIN32
    SetConsoleCtrlHandler(controlHandler, TRUE);
#else
    signal(SIGINT, controlHandler);
    signal(SIGTERM, controlHandler);
#endif

    // 命令行参数
    std::string bind_addr = "0.0.0.0";
    int port = MULTICAST_GROUP_PORT;
    std::string multicast_addr = MULTICAST_GROUP_ADDR;
    std::string iface_addr;

    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-b" && i + 1 < argc) {
            bind_addr = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "错误: 无效端口号 %d (有效范围: 1-65535)\n", port);
                return 1;
            }
        } else if (arg == "-m" && i + 1 < argc) {
            multicast_addr = argv[++i];
        } else if (arg == "-i" && i + 1 < argc) {
            iface_addr = argv[++i];
        } else {
            // 向后兼容: 第一个无标志参数作为接口地址
            if (i == 1 && arg[0] != '-') {
                iface_addr = arg;
            } else {
                fprintf(stderr, "错误: 未知选项或参数缺失: %s\n", arg.c_str());
                printUsage(argv[0]);
                return 1;
            }
        }
    }

    PLOG_INFO << "========================================";
    PLOG_INFO << "  组播接收测试程序";
    PLOG_INFO << "  绑定地址: " << bind_addr;
    PLOG_INFO << "  组播地址: " << multicast_addr;
    PLOG_INFO << "  组播端口: " << port;
    if (!iface_addr.empty()) {
        PLOG_INFO << "  接口地址: " << iface_addr;
    } else {
        PLOG_INFO << "  接口地址: (自动检测)";
    }
    PLOG_INFO << "  按 Ctrl+C 退出";
    PLOG_INFO << "========================================";

    // 创建组播接收器
    cammon::MulticastReceiver receiver;

    // 设置原始报文回调
    receiver.setOnRawPacketHandler([](const uint8_t* data, int len, const char* addr, int port) {
        g_stats.total_packets++;
        // 打印原始报文 hex（前20字节）
        std::ostringstream oss;
        oss << "[原始报文] src=" << addr << ":" << port << ", len=" << len << ", data=";
        oss << std::hex << std::uppercase << std::setfill('0');
        for (int i = 0; i < len && i < 20; i++) {
            oss << std::setw(2) << (int)data[i] << ' ';
        }
        PLOG_INFO << oss.str() << "...";
    });

    // 设置负载状态上报回调
    receiver.setOnLoadStatusReportHandler([](const cammon::LoadStatusReport& report) {
        g_stats.parsed_reports++;

        PLOG_INFO << "\n>>> [负载状态上报] <<<";
        PLOG_INFO << "  地址: 0x" << std::hex << (int)report.addr << std::dec;
        PLOG_INFO << "  帧序号: " << (unsigned)report.frame_seq;
        PLOG_INFO << "  红外焦距: " << report.vis_focal_length;
        PLOG_INFO << "  白光焦距: " << report.ir_focal_length;
        PLOG_INFO << "  方位脱靶量: " << report.az_aberration;
        PLOG_INFO << "  俯仰脱靶量: " << report.el_aberration;
        PLOG_INFO << "  伺服方位角: " << report.servo_azimuth;
        PLOG_INFO << "  伺服俯仰角: " << report.servo_elevation;
        PLOG_INFO << ">>> ======================== <<<";
    });

    // 初始化（传入配置的组播地址和端口）
    PLOG_INFO << "正在初始化组播接收器...";
    PLOG_INFO << "  绑定地址: " << bind_addr;
    PLOG_INFO << "  组播地址: " << multicast_addr;
    PLOG_INFO << "  组播端口: " << port;
    PLOG_INFO << "  接口地址: " << (iface_addr.empty() ? std::string("(自动检测)") : iface_addr);

    if (!receiver.init(bind_addr.c_str(),
                       iface_addr.empty() ? nullptr : iface_addr.c_str(),
                       multicast_addr.c_str(),
                       port)) {
        PLOG_ERROR << "错误: 组播接收器初始化失败";
        PLOG_ERROR << "请检查:";
        PLOG_ERROR << "  1. 网络接口是否正确";
        PLOG_ERROR << "  2. 是否有权限加入组播组";
        PLOG_ERROR << "  3. 防火墙是否阻止组播报文";
        return 1;
    }

    // 启动接收
    if (!receiver.start()) {
        PLOG_ERROR << "错误: 组播接收器启动失败";
        return 1;
    }

    PLOG_INFO << "组播接收器已启动，等待报文...";
    g_last_stats_time = std::chrono::steady_clock::now();

    // 主循环
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        printStats();
    }

    // 停止接收器
    receiver.stop();

    // 最终统计
    PLOG_INFO << "\n========== 最终统计 ==========";
    PLOG_INFO << "总接收报文数: " << g_stats.total_packets;
    PLOG_INFO << "成功解析负载状态上报: " << g_stats.parsed_reports;
    PLOG_INFO << "解析失败: " << g_stats.parse_errors;
    if (g_stats.total_packets > 0) {
        double rate = (double)g_stats.parsed_reports / g_stats.total_packets * 100;
        PLOG_INFO << "解析成功率: " << rate << "%";
    }
    PLOG_INFO << "==============================";

    return 0;
}