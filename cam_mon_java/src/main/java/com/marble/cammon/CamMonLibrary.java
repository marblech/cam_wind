package com.marble.cammon;

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;

/**
 * CamMonLibrary - JNA 接口定义
 * 
 * 通过 JNA（Java Native Access）直接调用 cam_mon_cpp 编译生成的 cammon.dll 库。
 * 与 JNI 不同，JNA 不需要编写桥接代码，直接映射 C 函数签名。
 * 
 * 映射的 C 函数来自 cammon_api.h 和 cam_controller.h:
 * - 底层 UDP 通信: cammon_send_udp_and_recv, cammon_send_packet, etc.
 * - 控制器封装: cam_controller_create/destroy/start/stop/get_ptz/set_ptz
 * 
 * @author marble
 * @version 1.0 (JNA migration)
 */
public interface CamMonLibrary extends Library {
    
    /**
     * 加载名为 "cammon" 的原生库
     * JNA 会自动从 java.library.path 或类路径中查找并加载
     */
    CamMonLibrary INSTANCE = Native.load("cammon", CamMonLibrary.class);
    
    // =========================================================================
    // 底层 UDP 通信接口 (cammon_api.h)
    // =========================================================================
    
    /**
     * 发送 UDP 数据包并等待接收响应
     * 
     * C 签名: int cammon_send_udp_and_recv(const char* host, int port, 
     *                                        const uint8_t* outbuf, int outlen,
     *                                        uint8_t* inbuf, int inbuf_len, 
     *                                        int timeout_ms)
     * 
     * @param host 目标主机 IP 地址
     * @param port 目标端口号
     * @param outbuf 发送数据缓冲区
     * @param outlen 发送数据长度
     * @param inbuf 接收缓冲区（传入足够大小的数组）
     * @param inbuf_len 接收缓冲区最大长度
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功时返回接收到的字节数（>0），失败返回负 error code
     */
    int cammon_send_udp_and_recv(String host, int port, 
                                  byte[] outbuf, int outlen,
                                  byte[] inbuf, int inbuf_len, 
                                  int timeout_ms);
    
    /**
     * 发送相机控制命令
     * 
     * C 签名: int cammon_send_camera_command(const char* host, int port, 
     *                                        uint8_t func, uint8_t ctrl,
     *                                        const uint8_t* payload, int payload_len,
     *                                        uint8_t* resp_buf, int resp_buf_len, 
     *                                        int timeout_ms)
     * 
     * @param host 目标主机 IP 地址
     * @param port 目标端口号
     * @param func 功能码
     * @param ctrl 控制码
     * @param payload 数据载荷（可为 null 表示无载荷）
     * @param payload_len 数据载荷长度
     * @param resp_buf 接收缓冲区
     * @param resp_buf_len 接收缓冲区最大长度
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功时返回接收到的字节数（>0），失败返回负 error code
     */
    int cammon_send_camera_command(String host, int port, 
                                    byte func, byte ctrl,
                                    byte[] payload, int payload_len,
                                    byte[] resp_buf, int resp_buf_len, 
                                    int timeout_ms);
    
    /**
     * 发送通用协议数据包（可指定地址）
     * 
     * C 签名: int cammon_send_packet(const char* host, int port,
     *                                uint8_t addr, uint8_t func, uint8_t ctrl,
     *                                const uint8_t* payload, int payload_len,
     *                                uint8_t* resp_buf, int resp_buf_len,
     *                                int timeout_ms)
     * 
     * @param host 目标主机 IP 地址
     * @param port 目标端口号
     * @param addr 目标地址
     * @param func 功能码
     * @param ctrl 控制码
     * @param payload 数据载荷（可为 null）
     * @param payload_len 数据载荷长度
     * @param resp_buf 接收缓冲区
     * @param resp_buf_len 接收缓冲区最大长度
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功时返回接收到的字节数（>0），失败返回负 error code
     */
    int cammon_send_packet(String host, int port,
                            byte addr, byte func, byte ctrl,
                            byte[] payload, int payload_len,
                            byte[] resp_buf, int resp_buf_len,
                            int timeout_ms);
    
    /**
     * 发送舵机控制命令
     * 
     * C 签名: int cammon_send_servo_command(const char* host, int port,
     *                                       float az, float el, float azs, float els,
     *                                       uint16_t target_distance, uint8_t seq, 
     *                                       uint8_t control, uint8_t device_type, 
     *                                       uint8_t packet_type,
     *                                       uint8_t* resp_buf, int resp_buf_len, 
     *                                       int timeout_ms)
     * 
     * @param host 目标主机 IP 地址
     * @param port 目标端口号
     * @param az 方位角（度）
     * @param el 俯仰角（度）
     * @param azs 方位角速度（度/秒）
     * @param els 俯仰角速度（度/秒）
     * @param targetDistance 目标距离（毫米）
     * @param seq 序列号
     * @param control 控制字节
     * @param deviceType 设备类型
     * @param packetType 数据包类型
     * @param respBuf 接收缓冲区
     * @param respBufLen 接收缓冲区最大长度
     * @param timeoutMs 超时时间（毫秒）
     * @return 成功时返回接收到的字节数（>0），失败返回负 error code
     */
    int cammon_send_servo_command(String host, int port,
                                   float az, float el, float azs, float els,
                                   short targetDistance, byte seq, 
                                   byte control, byte deviceType, 
                                   byte packetType,
                                   byte[] respBuf, int respBufLen, 
                                   int timeoutMs);
    
    // =========================================================================
    // 控制器封装接口 (cam_controller.h)
    // =========================================================================
    
    /**
     * 创建控制器实例
     * 
     * C 签名: CamController* cam_controller_create()
     * 
     * @return 成功返回指针，失败返回 null
     */
    Pointer cam_controller_create();
    
    /**
     * 销毁控制器实例并释放资源
     * 
     * C 签名: void cam_controller_destroy(CamController* h)
     * 
     * @param h 控制器句柄
     */
    void cam_controller_destroy(Pointer h);
    
    /**
     * 启动控制器监听线程（可指定组播组）
     * 
     * C 签名: int cam_controller_start_ex(CamController* h, int port, const char* mcast_group)
     * 
     * @param h 控制器句柄
     * @param port 监听端口
     * @param mcastGroup 组播组地址（null 或空字符串表示不加入组播）
     * @return 0 表示成功，负值表示错误
     */
    int cam_controller_start_ex(Pointer h, int port, String mcastGroup);
    
    /**
     * 停止控制器监听（阻塞直到线程退出）
     * 
     * C 签名: void cam_controller_stop(CamController* h)
     * 
     * @param h 控制器句柄
     */
    void cam_controller_stop(Pointer h);
    
    /**
     * 读取最后一条接收的数据包
     * 
     * C 签名: int cam_controller_get_last(CamController* h, uint8_t* buf, int buflen)
     * 
     * @param h 控制器句柄
     * @param buf 输出缓冲区
     * @param buflen 缓冲区大小
     * @return 返回拷贝的字节数，若无数据则返回 0，出错返回负值
     */
    int cam_controller_get_last(Pointer h, byte[] buf, int buflen);
    
    /**
     * 从最后一条状态包中获取解析后的 PTZ 值
     * 
     * C 签名: int cam_controller_get_ptz(CamController* h, float* out_az, float* out_el, 
     *                                     float* out_ir_focus, float* out_vis_focus)
     * 
     * @param h 控制器句柄
     * @param outAz 输出口位角
     * @param outEl 输出俯仰角
     * @param outIrFocus 输出红外焦距
     * @param outVisFocus 输出可见光焦距
     * @return 若有有效值返回 1，否则返回 0
     */
    int cam_controller_get_ptz(Pointer h, float[] outAz, float[] outEl, 
                                float[] outIrFocus, float[] outVisFocus);
    
    /**
     * 发送 PTZ（舵机）命令
     * 
     * C 签名: int cam_controller_set_ptz(CamController* h, const char* host, int port,
     *                                     float az, float el, float azs, float els,
     *                                     uint16_t target_distance, uint8_t seq, uint8_t control,
     *                                     uint8_t device_type, uint8_t packet_type,
     *                                     uint8_t* resp_buf, int resp_buf_len, int timeout_ms)
     * 
     * @param h 控制器句柄
     * @param host 目标主机地址
     * @param port 目标端口
     * @param az 方位角
     * @param el 俯仰角
     * @param azs 方位角速度
     * @param els 俯仰角速度
     * @param targetDistance 目标距离（毫米）
     * @param seq 序列号
     * @param control 控制字节
     * @param deviceType 设备类型
     * @param packetType 数据包类型
     * @param respBuf 接收缓冲区
     * @param respBufLen 接收缓冲区长度
     * @param timeoutMs 超时时间（毫秒）
     * @return 成功时接收到的字节数 (>0)，失败返回负 error code
     */
    int cam_controller_set_ptz(Pointer h, String host, int port,
                                float az, float el, float azs, float els,
                                short targetDistance, byte seq, byte control,
                                byte deviceType, byte packetType,
                                byte[] respBuf, int respBufLen, int timeoutMs);
}