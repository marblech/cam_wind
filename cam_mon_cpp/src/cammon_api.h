#pragma once

#include <cstdint>

/**
 * @file cammon_api.h
 * @brief 摄像头监控底层 UDP 通信 API 声明
 * 
 * 提供两层接口：
 * - 底层: cammon_send_udp_and_recv() 直接发送 UDP 并接收响应
 * - 高层: cammon_send_camera_command() / cammon_send_servo_command() 
 *   自动构建协议包再发送
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 发送 UDP 数据包并等待接收响应
 * 
 * @param host 目标主机 IP 地址
 * @param port 目标端口号
 * @param outbuf 发送数据缓冲区
 * @param outlen 发送数据长度
 * @param inbuf 接收缓冲区
 * @param inbuf_len 接收缓冲区最大长度
 * @param timeout_ms 超时时间（毫秒），0 表示使用默认超时
 * @return 成功时返回接收到的字节数（>0），失败返回负 error code
 */
int cammon_send_udp_and_recv(const char* host, int port, 
                              const uint8_t* outbuf, int outlen,
                              uint8_t* inbuf, int inbuf_len, 
                              int timeout_ms);

/**
 * @brief 发送相机控制命令
 * 
 * @param host 目标主机 IP 地址
 * @param port 目标端口号
 * @param func 功能码
 * @param ctrl 控制码
 * @param payload 数据载荷（可为 nullptr）
 * @param payload_len 数据载荷长度
 * @param resp_buf 接收缓冲区
 * @param resp_buf_len 接收缓冲区最大长度
 * @param timeout_ms 超时时间（毫秒）
 * @return 成功时返回接收到的字节数（>0），失败返回负 error code
 */
int cammon_send_camera_command(const char* host, int port, 
                                uint8_t func, uint8_t ctrl,
                                const uint8_t* payload, int payload_len,
                                uint8_t* resp_buf, int resp_buf_len, 
                                int timeout_ms);

/**
 * @brief 发送通用协议数据包（可指定地址）
 * 
 * 构建一个标准帧（0x0F 0xF0 ... 0xF0 0x0F），并发送到目标主机。
 * 可用于向跟踪处理器（addr=0x00）或相机（addr=0x02）发送命令。
 */
int cammon_send_packet(const char* host, int port,
                       uint8_t addr, uint8_t func, uint8_t ctrl,
                       const uint8_t* payload, int payload_len,
                       uint8_t* resp_buf, int resp_buf_len,
                       int timeout_ms);


/**
 * @brief 发送舵机控制命令
 * 
 * @param host 目标主机 IP 地址
 * @param port 目标端口号
 * @param az 方位角（度）
 * @param el 俯仰角（度）
 * @param azs 方位角速度（度/秒）
 * @param els 俯仰角速度（度/秒）
 * @param target_distance 目标距离（毫米）
 * @param seq 序列号
 * @param control 控制字节
 * @param device_type 设备类型
 * @param packet_type 数据包类型
 * @param resp_buf 接收缓冲区
 * @param resp_buf_len 接收缓冲区最大长度
 * @param timeout_ms 超时时间（毫秒）
 * @return 成功时返回接收到的字节数（>0），失败返回负 error code
 */
int cammon_send_servo_command(const char* host, int port,
                               float az, float el, float azs, float els,
                               uint16_t target_distance, uint8_t seq, 
                               uint8_t control, uint8_t device_type, 
                               uint8_t packet_type,
                               uint8_t* resp_buf, int resp_buf_len, 
                               int timeout_ms);

#ifdef __cplusplus
}
#endif