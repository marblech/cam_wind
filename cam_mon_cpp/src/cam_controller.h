#ifndef CAM_WIND_SRC_CAM_CONTROLLER_H
#define CAM_WIND_SRC_CAM_CONTROLLER_H

/**
 * @file src/cam_controller.h
 * @brief 摄像头控制器封装头文件（C API 封装）
 * @author marblech
 * @date 2026-06-02
 * @copyright SPDX: MIT OR as-project-specifies
 */

#include <cstdint>
#include "cammon_api.h"

extern "C" {
/**
 * @brief 不透明控制器句柄
 *
 * 在实现中定义具体结构体，API 对外暴露为不透明指针以隐藏实现细节。
 */
typedef struct CamController CamController;

CAMMON_API enum action_type {
	ACTION_NONE = 0,
	ACTION_SET_ZOOM = 1	
};

/**
 * @brief 创建控制器实例
 * @return CamController* 成功返回指针，失败返回 nullptr
 */
CAMMON_API CamController* cam_controller_create();

/**
 * @brief 销毁控制器实例并释放资源
 * @param h [in] 控制器句柄
 */
CAMMON_API void cam_controller_destroy(CamController* h);

/**
 * @brief 启动控制器监听线程
 *
 * 如果 `mcast_group` 非空，则尝试加入该组播组；否则以点对点 UDP 模式运行。
 *
 * @param h [in] 控制器句柄
 * @param port [in] 监听端口
 * @param mcast_group [in] 组播组地址（NULL 或空字符串表示不加入组播）
 * @return int 0 表示成功，负值表示错误
 */
CAMMON_API int cam_controller_start_ex(CamController* h, int port, const char* mcast_group);

/**
 * @brief 启动控制器监听（不指定组播组）
 * @param h [in] 控制器句柄
 * @param port [in] 监听端口
 * @return int 0 表示成功，负值表示错误
 */
static inline int cam_controller_start(CamController* h, int port) { return cam_controller_start_ex(h, port, nullptr); }

/**
 * @brief 使用外部配置文件启动控制器监听
 *
 * 从传入的配置文件中读取点对点监听端口（键名为 `p2p.listen_port`，INI 格式），
 * 并在 `mcast_group` 为空时以点对点模式监听。返回值与 `cam_controller_start_ex` 相同。
 *
 * @param h [in] 控制器句柄
 * @param config_file [in] 配置文件路径（INI 格式）
 * @param mcast_group [in] 组播组地址（NULL 或空字符串表示不加入组播）
 */
CAMMON_API int cam_controller_start_with_config(CamController* h, const char* config_file, const char* mcast_group);

/**
 * @brief 便捷接口：只传入配置文件（不加入组播）
 */
static inline int cam_controller_start_with_config_simple(CamController* h, const char* config_file) { return cam_controller_start_with_config(h, config_file, nullptr); }

/**
 * @brief 停止控制器监听（阻塞直到线程退出）
 * @param h [in] 控制器句柄
 */
CAMMON_API void cam_controller_stop(CamController* h);

/**
 * @brief 读取最后一条接收的数据包
 *
 * 将最后接收的数据拷贝到 `buf` 中，最多拷贝 `buflen` 字节。
 *
 * @param h [in] 控制器句柄
 * @param buf [out] 输出缓冲区
 * @param buflen [in] 缓冲区大小
 * @return int 返回拷贝的字节数，若无数据则返回 0，出错返回负值
 */
CAMMON_API int cam_controller_get_last(CamController* h, uint8_t* buf, int buflen);

/**
 * @brief 从指定 IP 地址的最后一条状态包中获取解析后的 PTZ 值
 *
 * @param h [in] 控制器句柄
 * @param ip [in] 来源 IP 地址
 * @param out_az [out] 方位角
 * @param out_el [out] 俯仰角
 * @param out_ir_focus [out] 红外焦距
 * @param out_vis_focus [out] 可见光焦距
 * @return int 若有有效值返回 1，否则返回 0
 */
CAMMON_API int cam_controller_get_ptz(CamController* h, const char* ip, float* out_az, float* out_el, float* out_ir_focus, float* out_vis_focus);

/**
 * @brief 发送 PTZ（舵机）命令
 *
 * 封装到 `cammon_send_servo_command`，并返回相同的返回码。
 *
 * @param h [in] 控制器句柄
 * @param host [in] 目标主机地址
 * @param port [in] 目标端口
 * @param az [in] 方位角
 * @param el [in] 俯仰角
 * @param azs [in] 方位角速度
 * @param els [in] 俯仰角速度
 * @param target_distance [in] 目标距离（毫米）
 * @param seq [in] 序列号
 * @param control [in] 控制字节
 * @param device_type [in] 设备类型
 * @param packet_type [in] 数据包类型
 * @param resp_buf [out] 接收缓冲区
 * @param resp_buf_len [in] 接收缓冲区长度
 * @param timeout_ms [in] 超时时间（毫秒）
 * @return int 成功时接收到的字节数 (>0)，失败返回负 error code
 */
/**
 * @brief 发送 PTZ（舵机）命令（简化版）
 *
 * 该接口被简化为仅需要目标主机地址、要设置的 PTZ 值以及设备类型。
 * 实现会使用合理的默认值（速度、包类型、序列号等）来构建舵机包并发送，
 * 并在必要时向相机发送焦距（focus）直达命令以实现 zoom 的下发。
 *
 * 如果 `host` 为 NULL 或空字符串，或 `device_type` 未指定（例如为 0），
 * 则立即返回错误。
 *
 * @param h 控制器句柄
 * @param host 目标主机 IP 地址（不可为空）
 * @param port 目标主机端口
 * @param az 方位角（度）
 * @param el 俯仰角（度）
 * @param zoom 变焦/焦距（以 mm 为单位，若不需要可传负数）
 * @param device_type 设备类型（可见光 / 热成像，使用协议中定义的编码）
 * @return 成功时返回 cammon_send_servo_command 的返回值（>0 表示收到字节数），失败返回负错误码
 */
CAMMON_API int cam_controller_set_ptz(CamController* h, const char* host, const int port,
									  float az, float el, float zoom,
									  uint8_t device_type, const int action);
}

#endif /* CAM_WIND_SRC_CAM_CONTROLLER_H */
