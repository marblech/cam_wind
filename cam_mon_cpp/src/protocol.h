#pragma once

#include <cstdint>
#include <vector>
#include <optional>

/**
 * @file protocol.h
 * @brief 摄像头监控系统通信协议定义
 * 
 * 本头文件定义了摄像头监控系统所使用的通信协议格式，
 * 包括标准数据包结构、舵机专用数据包结构、设备地址枚举
 * 以及相关的序列化/反序列化函数。
 * 
 * 协议支持两种帧格式：
 * - 标准帧：以 0x0F 0xF0 开头，0xF0 0x0F 结尾，最小帧长 23 字节
 * - 舵机帧：以 0x7E 开头，固定帧长 72 字节
 */

namespace cammon {

// ============================================================================
// 协议常量 (基于协议文档 V0.07)
// ============================================================================

/**
 * @brief 数据包首字节标志
 * 
 * 每个数据包的第一个字节，用于标识帧起始。
 */
static constexpr uint8_t FRAME_HDR1 = 0x0F;

/**
 * @brief 数据包辅助标志
 * 
 * 每个数据包的第二个字节，与 FRAME_HDR1 一起构成帧头。
 */
static constexpr uint8_t FRAME_HDR2 = 0xF0;

/**
 * @brief 数据包尾字节标志1
 * 
 * 帧尾的第一个字节。
 */
static constexpr uint8_t FRAME_TAIL1 = 0xF0;

/**
 * @brief 数据包尾字节标志2
 * 
 * 帧尾的第二个字节。
 */
static constexpr uint8_t FRAME_TAIL2 = 0x0F;

/**
 * @brief 大多数消息的默认数据长度
 * 
 * 除舵机控制器外，大多数消息使用 15 字节的数据区。
 */
static constexpr size_t DATA_LEN_DEFAULT = 15;

// ============================================================================
// 设备地址枚举
// ============================================================================

/**
 * @brief 设备地址定义
 * 
 * 定义了系统中所有可用设备的地址编码，
 * 用于在通信数据包中标识目标设备。
 */
enum DeviceAddress : uint8_t {
    ADDR_TRACK_VIS = 0x00,  ///< 可见光跟踪仪地址
    ADDR_TRACK_IR  = 0x01,  ///< 红外跟踪仪地址
    ADDR_CAMERA_VIS = 0x02, ///< 可见光相机地址
    ADDR_CAMERA_IR  = 0x03, ///< 红外相机地址
    ADDR_ENV_CTRL   = 0x04, ///< 环境控制器地址
    ADDR_SERVO      = 0x05  ///< 舵机控制器地址
};
enum VisibleCameraFunction : uint8_t {
    VC_FUNC_CONTINUOUS_ZOOM = 0x01,
    VC_FUNC_STEP_ZOOM = 0x02,
    VC_FUNC_CONTINUOUS_FOCUS = 0x03,
    VC_FUNC_SINGLE_STEP_FOCUS = 0x04,
    VC_FUNC_AUTO_FOCUS = 0x05,
    VC_FUNC_FOCAL_DIRECT = 0x06,
    VC_FUNC_FOCUS_DIRECT = 0x07,
    VC_FUNC_DEFOG = 0x08,
    VC_FUNC_BRIGHTNESS = 0x09,
    VC_FUNC_CONTRAST = 0x0A,
    VC_FUNC_IMAGE_ENHANCE = 0x0B
};

enum CameraControl : uint8_t {
    CAM_CTRL_STOP = 0x00,
    CAM_CTRL_CONT_PLUS = 0x01,
    CAM_CTRL_CONT_MINUS = 0x02
};


// ============================================================================
// 标准数据包结构
// ============================================================================

/**
 * @brief 标准通信数据包结构
 * 
 * 帧格式: [HDR1][HDR2][地址][功能][控制][数据...][校验和][TAIL1][TAIL2]
 * 最小帧长: 23 字节 (2头 + 3控制 + 15数据 + 1校验 + 2尾)
 * 
 * 此结构用于表示所有标准格式的数据包，
 * 包括命令、响应、状态查询等。
 * 
 * @example
 * @code
 * Packet p;
 * p.addr = ADDR_CAMERA_VIS;
 * p.func = 0x01;
 * p.ctrl = 0x01;
 * p.data = {0x01, 0x02, 0x03, ...};
 * auto bytes = p.serialize();
 * @endcode
 */
struct Packet {
    uint8_t hdr1 = FRAME_HDR1;           ///< 帧首标志1 (固定为 0x0F)
    uint8_t hdr2 = FRAME_HDR2;           ///< 帧首标志2 (固定为 0xF0)
    uint8_t addr = 0;                    ///< 目标设备地址 (使用 DeviceAddress 枚举值)
    uint8_t func = 0;                    ///< 功能码 (定义操作类型，如 0x01 查询、0x04 拍照等)
    uint8_t ctrl = 0;                    ///< 控制码 (定义操作行为，如 0x01 启用、0x00 禁用等)
    std::vector<uint8_t> data;           ///< 可变长度数据区 (大多数消息使用 15 字节)
    uint8_t checksum = 0;                ///< XOR 校验和 (从地址到数据的逐字节异或结果)
    uint8_t tail1 = FRAME_TAIL1;         ///< 帧尾标志1 (固定为 0xF0)
    uint8_t tail2 = FRAME_TAIL2;         ///< 帧尾标志2 (固定为 0x0F)

    /**
     * @brief 将数据包序列化为字节流
     * 
     * 根据当前 Packet 对象的各字段值，构造完整的帧字节序列。
     * 序列化过程：
     * 1. 计算校验和（从 addr 到 data 末尾的 XOR）
     * 2. 按顺序组装: HDR1 + HDR2 + addr + func + ctrl + data + checksum + TAIL1 + TAIL2
     * 
     * @return std::vector<uint8_t> 序列化后的字节向量 (通常 23 字节)
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief 从字节流反序列化数据包
     * 
     * 解析输入的字节缓冲区，验证帧头和帧尾，计算并校验校验和。
     * 
     * @param buf 输入字节缓冲区
     * @return std::optional<Packet> 成功时返回解析好的 Packet 对象，失败返回 std::nullopt
     * 
     * 验证步骤：
     * 1. 检查缓冲区长度是否 >= 23 字节
     * 2. 验证帧头是否为 0x0F 0xF0
     * 3. 验证帧尾是否为 0xF0 0x0F
     * 4. 计算并验证 XOR 校验和
     */
    static std::optional<Packet> deserialize(const std::vector<uint8_t>& buf);
};

/**
 * @brief 计算 XOR 校验和
 * 
 * 对输入字节向量从第 0 字节开始逐字节进行 XOR 运算，
 * 生成 1 字节校验和。此校验和用于验证数据包的完整性。
 * 
 * @param bytes 输入字节向量 (通常为 addr + func + ctrl + data)
 * @return uint8_t 1 字节 XOR 校验和
 */
uint8_t compute_xor_checksum(const std::vector<uint8_t>& bytes);

/**
 * @brief 创建相机命令数据包
 * 
 * 创建一个使用可见光相机地址 (ADDR_CAMERA_VIS = 0x02) 的标准数据包。
 * 
 * @param func 功能码 (定义操作类型)
 * @param ctrl 控制码 (定义操作行为)
 * @param payload 数据载荷 (将被复制到 data 字段)
 * @return Packet 构造好的 Packet 对象 (已设置正确的帧头、帧尾和校验和)
 */
Packet make_camera_command(uint8_t func, uint8_t ctrl, const std::vector<uint8_t>& payload);

/**
 * @brief 构建舵机数据包
 * 
 * 生成标准舵机控制指令（72 字节帧，以 0x7E 起始）。
 * 此函数用于构造完整的舵机运动控制数据包，
 * 包含方位角、俯仰角、速度控制和目标距离等信息。
 * 
 * @param azimuth 方位角 (单位: 度, 范围通常 -180° ~ +180°)
 * @param elevation 俯仰角 (单位: 度, 范围通常 -90° ~ +90°)
 * @param az_speed 方位角速度 (单位: 度/秒)
 * @param el_speed 俯仰角速度 (单位: 度/秒)
 * @param target_distance 目标距离 (单位: 毫米)
 * @param seq 序列号 (用于匹配请求和响应，默认 0x01)
 * @param control 控制字节 (控制标志位，默认 0x11)
 * @param device_type 设备类型 (默认 0x01 表示舵机控制器)
 * @param packet_type 数据包类型 (默认 0x02 表示定点报告)
 * @return std::vector<uint8_t> 序列化后的 72 字节向量
 */
std::vector<uint8_t> build_servo_packet(float azimuth, float elevation, float az_speed, float el_speed, uint16_t target_distance, uint8_t seq = 0x01, uint8_t control = 0x11, uint8_t device_type = 0x01, uint8_t packet_type = 0x02);

// ============================================================================
// 舵机专用数据包结构 (帧起始符 0x7E)
// ============================================================================

/**
 * @brief 舵机数据包结构
 * 
 * 帧格式: [0x7E][0x48][序列][设备类型][包类型][IP][保留][控制][保留][方位角][俯仰角][方位速度][俯仰速度][距离][保留33字节][保留2][保留2][时间戳][保留状态][备份][校验和]
 * 固定帧长: 72 字节
 * 
 * 此结构专门用于舵机控制器的通信，
 * 与标准数据包结构不同，使用 0x7E 作为帧起始符。
 * 
 * @note 舵机数据包使用小端字节序存储多字节字段
 */
struct ServoPacket {
    uint8_t header = 0x7E;              ///< 帧头固定值 (固定为 0x7E)
    uint8_t frame_len = 0x48;           ///< 固定长度 (固定为 0x48 = 72 字节)
    uint8_t seq = 0;                    ///< 序列号 (用于请求/响应匹配)
    uint8_t device_type = 0;            ///< 源/目标设备类型 (0x01 表示舵机控制器)
    uint8_t packet_type = 0;            ///< 数据包类型 (0x02 表示定点报告)
    uint8_t device_ip = 0;              ///< 设备 IP 标识
    uint8_t reserved_conn = 0;          ///< 保留字段 (连接相关)
    uint8_t control = 0;                ///< 控制字节
    uint8_t reserved_ctrl = 0;          ///< 保留字段 (控制相关)
    float azimuth = 0.0f;               ///< 方位角 (4 字节小端浮点数，单位: 度)
    float elevation = 0.0f;             ///< 俯仰角 (4 字节小端浮点数，单位: 度)
    float az_speed = 0.0f;              ///< 方位角速度 (4 字节小端浮点数，单位: 度/秒)
    float el_speed = 0.0f;              ///< 俯仰角速度 (4 字节小端浮点数，单位: 度/秒)
    uint16_t target_distance = 0;       ///< 目标距离 (2 字节小端无符号整数，单位: 毫米)
    std::vector<uint8_t> reserved;      ///< 保留字段 (33 字节)
    uint16_t reserved_horz = 0;         ///< 保留字段 (水平相关，2 字节)
    uint16_t reserved_vert = 0;         ///< 保留字段 (垂直相关，2 字节)
    uint32_t timestamp = 0;             ///< 时间戳 (4 字节)
    uint8_t reserved_state = 0;         ///< 保留字段 (状态相关)
    uint16_t backup = 0;                ///< 备份数据 (2 字节)
    uint8_t checksum = 0;               ///< 校验和 (位置 71 的 XOR 值)

    /**
     * @brief 将舵机数据包序列化为字节流
     * 
     * 根据当前 ServoPacket 对象的各字段值，构造 72 字节的帧字节序列。
     * 序列化过程：
     * 1. 按小端字节序组装多字节字段
     * 2. 计算校验和
     * 3. 组装完整帧
     * 
     * @return std::vector<uint8_t> 序列化后的 72 字节向量
     */
    std::vector<uint8_t> serialize_servo() const;

    /**
     * @brief 从字节流反序列化舵机数据包
     * 
     * 解析输入的字节缓冲区，验证帧头，提取各字段值。
     * 
     * @param buf 输入字节缓冲区 (至少 72 字节)
     * @return std::optional<ServoPacket> 成功时返回解析好的 ServoPacket 对象，失败返回 std::nullopt
     * 
     * 验证步骤：
     * 1. 检查缓冲区长度是否 >= 72 字节
     * 2. 验证帧头是否为 0x7E
     */
    static std::optional<ServoPacket> deserialize_servo(const std::vector<uint8_t>& buf);
};

} // namespace cammon