#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <chrono>
#include <thread>
#include <atomic>

#include "protocol.h"
#include "multicast_receiver.h"

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
        fprintf(stderr, "\n[测试程序] 收到中断信号，正在退出...\n");
        g_running = false;
        return TRUE;
    }
    return FALSE;
}
#else
void controlHandler(int sig) {
    (void)sig;
    fprintf(stderr, "\n[测试程序] 收到中断信号，正在退出...\n");
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

int main(int argc, char* argv[]) {
    // 设置信号处理
#ifdef _WIN32
    SetConsoleCtrlHandler(controlHandler, TRUE);
#else
    signal(SIGINT, controlHandler);
    signal(SIGTERM, controlHandler);
#endif

    fprintf(stderr, "========================================\n");
    fprintf(stderr, "  组播接收测试程序\n");
    fprintf(stderr, "  组播地址: %s\n", cammon::MULTICAST_GROUP_ADDR);
    fprintf(stderr, "  组播端口: %d\n", cammon::MULTICAST_GROUP_PORT);
    fprintf(stderr, "  按 Ctrl+C 退出\n");
    fprintf(stderr, "========================================\n\n");

    // 获取接口地址（可选）
    std::string iface_addr;
    if (argc > 1) {
        iface_addr = argv[1];
    }

    // 创建组播接收器
    cammon::MulticastReceiver receiver;

    // 设置原始报文回调
    receiver.setOnRawPacketHandler([](const uint8_t* data, int len, const char* addr, int port) {
        g_stats.total_packets++;
        // 打印原始报文 hex（前20字节）
        fprintf(stderr, "[原始报文] src=%s:%d, len=%d, data=", addr, port, len);
        for (int i = 0; i < len && i < 20; i++) {
            fprintf(stderr, "%02X ", data[i]);
        }
        fprintf(stderr, "...\n");
    });

    // 设置负载状态上报回调
    receiver.setOnLoadStatusReportHandler([](const cammon::LoadStatusReport& report) {
        g_stats.parsed_reports++;

        fprintf(stderr, "\n>>> [负载状态上报] <<<\n");
        fprintf(stderr, "  地址: 0x%02X\n", static_cast<unsigned>(report.addr));
        fprintf(stderr, "  帧序号: %u\n", static_cast<unsigned>(report.frame_seq));
        fprintf(stderr, "  红外焦距: %.1f\n", report.ir_focal_length);
        fprintf(stderr, "  白光焦距: %.1f\n", report.vis_focal_length);
        fprintf(stderr, "  方位脱靶量: %.2f\n", report.az_aberration);
        fprintf(stderr, "  俯仰脱靶量: %.2f\n", report.el_aberration);
        fprintf(stderr, "  伺服方位角: %.2f\n", report.servo_azimuth);
        fprintf(stderr, "  伺服俯仰角: %.2f\n", report.servo_elevation);
        fprintf(stderr, ">>> ======================== <<<\n\n");
    });

    // 初始化
    fprintf(stderr, "正在初始化组播接收器 (接口地址: %s)...\n", 
            iface_addr.empty() ? "默认" : iface_addr.c_str());
    
    if (!receiver.init("0.0.0.0", iface_addr.empty() ? "" : iface_addr.c_str())) {
        fprintf(stderr, "错误: 组播接收器初始化失败\n");
        fprintf(stderr, "请检查:\n");
        fprintf(stderr, "  1. 网络接口是否正确\n");
        fprintf(stderr, "  2. 是否有权限加入组播组\n");
        fprintf(stderr, "  3. 防火墙是否阻止组播报文\n");
        return 1;
    }

    // 启动接收
    if (!receiver.start()) {
        fprintf(stderr, "错误: 组播接收器启动失败\n");
        return 1;
    }

    fprintf(stderr, "组播接收器已启动，等待报文...\n\n");
    g_last_stats_time = std::chrono::steady_clock::now();

    // 主循环
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        printStats();
    }

    // 停止接收器
    receiver.stop();

    // 最终统计
    fprintf(stderr, "\n========== 最终统计 ==========\n");
    fprintf(stderr, "总接收报文数: %d\n", g_stats.total_packets);
    fprintf(stderr, "成功解析负载状态上报: %d\n", g_stats.parsed_reports);
    fprintf(stderr, "解析失败: %d\n", g_stats.parse_errors);
    if (g_stats.total_packets > 0) {
        double rate = (double)g_stats.parsed_reports / g_stats.total_packets * 100;
        fprintf(stderr, "解析成功率: %.1f%%\n", rate);
    }
    fprintf(stderr, "==============================\n");

    return 0;
}