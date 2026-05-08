#include "protocol.h"
#include <cstring>

namespace cammon {

// ============================================================================
// XOR 校验和计算
// ============================================================================

/**
 * @brief 计算 XOR 校验和
 * 
 * 对输入字节向量中的所有字节进行逐位异或运算，生成 1 字节校验和。
 * 该校验和用于验证数据包完整性，计算范围为地址字节到数据字节末尾。
 * 
 * @param bytes 输入字节向量
 * @return uint8_t 1 字节 XOR 校验和
 */
uint8_t compute_xor_checksum(const std::vector<uint8_t>& bytes) {
    // NOTE: manufacturer examples use additive checksum (mod 256) over address..data.
    // Keep function name for compatibility but implement as sum.
    uint8_t cs = 0;
    for (auto b : bytes) cs = static_cast<uint8_t>(cs + b);
    return cs;
}

// ============================================================================
// 标准数据包序列化/反序列化
// ============================================================================

/**
 * @brief 将标准数据包序列化为 UDP 字节流
 * 
 * 序列化格式: [HDR1][HDR2][地址][功能][控制][数据(补齐至DATA_LEN_DEFAULT)][校验和][TAIL1][TAIL2]
 * - 数据区不足 DATA_LEN_DEFAULT 时自动填充 0x00
 * - 数据区超过 DATA_LEN_DEFAULT 时保留原长度（用于舵机等大帧）
 * - 校验和 = 地址 XOR 功能 XOR 控制 XOR 数据区所有字节
 * 
 * @return std::vector<uint8_t> 序列化后的字节向量（通常 23 字节）
 */
std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> out;
    out.push_back(hdr1);
    out.push_back(hdr2);
    out.push_back(addr);
    out.push_back(func);
    out.push_back(ctrl);
    
    // 数据区处理：补齐或保留原长
    size_t data_len = std::max<size_t>(data.size(), DATA_LEN_DEFAULT);
    std::vector<uint8_t> d = data;
    if (d.size() < data_len) d.resize(data_len, 0x00);
    out.insert(out.end(), d.begin(), d.end());
    
    // 计算校验和：从地址到数据区
    std::vector<uint8_t> csrange;
    csrange.push_back(addr);
    csrange.push_back(func);
    csrange.push_back(ctrl);
    csrange.insert(csrange.end(), d.begin(), d.end());
    uint8_t cs = compute_xor_checksum(csrange);
    out.push_back(cs);
    out.push_back(tail1);
    out.push_back(tail2);
    return out;
}

/**
 * @brief 从字节流反序列化标准数据包
 * 
 * 验证流程:
 * 1. 检查缓冲区最小长度（至少 23 字节）
 * 2. 验证帧头 (0x0F 0xF0)
 * 3. 查找帧尾 (0xF0 0x0F)
 * 4. 验证 XOR 校验和
 * 
 * @param buf 输入字节缓冲区
 * @return std::optional<Packet> 成功时返回 Packet 对象，失败返回 std::nullopt
 */
std::optional<Packet> Packet::deserialize(const std::vector<uint8_t>& buf) {
    // 最小长度检查: 2头 + 3控制 + 15数据 + 1校验 + 2尾 = 23
    if (buf.size() < 23) return std::nullopt;

    // 支持流式缓冲区：在缓冲区内查找首个合法帧（header .. tail），并只解析该帧。
    size_t n = buf.size();
    for (size_t start = 0; start + 1 < n; ++start) {
        if (buf[start] != FRAME_HDR1 || buf[start+1] != FRAME_HDR2) continue;
        // 找到可能的帧头，从 start + minimal_length - 1 寻找帧尾位置
        size_t min_tail_idx = start + 23 - 1; // 最小帧尾索引 (inclusive)
        if (min_tail_idx + 1 >= n) {
            // 不足以包含最小帧，继续搜索下一个可能的起始位置
            continue;
        }
        // 查找帧尾 (FRAME_TAIL1, FRAME_TAIL2)
        for (size_t tail_idx = min_tail_idx; tail_idx + 1 < n; ++tail_idx) {
            if (buf[tail_idx] == FRAME_TAIL1 && buf[tail_idx+1] == FRAME_TAIL2) {
                // 拷贝该帧的字节范围 [start, tail_idx+1]
                std::vector<uint8_t> frame(buf.begin() + start, buf.begin() + tail_idx + 2);
                // 基本验证
                if (frame.size() < 23) break; // should not happen
                // 解析字段
                Packet p;
                size_t idx = 2; // after hdr1,hdr2
                p.addr = frame[idx++];
                p.func = frame[idx++];
                p.ctrl = frame[idx++];

                size_t expected_cs_idx = frame.size() - 3; // checksum position relative to frame
                if (expected_cs_idx <= idx) break;
                p.data.assign(frame.begin() + idx, frame.begin() + expected_cs_idx);
                p.checksum = frame[expected_cs_idx];

                // 验证校验和（地址..数据）
                std::vector<uint8_t> csrange;
                csrange.push_back(p.addr);
                csrange.push_back(p.func);
                csrange.push_back(p.ctrl);
                csrange.insert(csrange.end(), p.data.begin(), p.data.end());
                if (compute_xor_checksum(csrange) != p.checksum) {
                    // 校验失败，继续在当前 start 下寻找下一个可能的尾
                    continue;
                }

                return p;
            }
        }
        // 未找到合适的尾部，继续搜索下一个起始位置
    }

    return std::nullopt;
}

// ============================================================================
// 相机命令构建
// ============================================================================

/**
 * @brief 创建可见光相机命令数据包
 * 
 * 使用默认相机地址 (ADDR_CAMERA_VIS = 0x02) 构建标准相机控制帧。
 * 
 * @param func 功能码（如 0x01 查询状态、0x04 拍照等）
 * @param ctrl 控制码（如 0x01 启用、0x00 禁用等）
 * @param payload 数据载荷（会自动补齐至 15 字节）
 * @return Packet 构造好的 Packet 对象
 */
Packet make_camera_command(uint8_t func, uint8_t ctrl, const std::vector<uint8_t>& payload) {
    Packet p;
    p.addr = ADDR_CAMERA_VIS;
    p.func = func;
    p.ctrl = ctrl;
    p.data = payload;
    return p;
}

// ============================================================================
// 舵机数据包构建辅助函数
// ============================================================================

/**
 * @brief 写入 16 位小端整数到输出向量（内部辅助函数）
 * 
 * @param out 输出向量
 * @param v 要写入的 16 位无符号整数
 */
static void write_le_uint16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

/**
 * @brief 写入 32 位小端整数到输出向量（内部辅助函数）
 * 
 * @param out 输出向量
 * @param v 要写入的 32 位无符号整数
 */
static void write_le_uint32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

/**
 * @brief 写入浮点数到输出向量（按小端字节序）（内部辅助函数）
 * 
 * @param out 输出向量
 * @param f 要写入的浮点数
 */
static void write_le_float(std::vector<uint8_t>& out, float f) {
    uint32_t v;
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    memcpy(&v, &f, 4);
    write_le_uint32(out, v);
}

// ============================================================================
// 舵机数据包序列化/反序列化
// ============================================================================

/**
 * @brief 将舵机数据包序列化为 72 字节 UDP 帧
 * 
 * 序列化格式:
 * [0x7E][0x48][序列][设备类型][包类型][IP][保留][控制][保留]
 * [方位角(f)][俯仰角(f)][方位速度(f)][俯仰速度(f)][距离(u16)]
 * [保留33字节][保留2(u16)][保留2(u16)][时间戳(u32)][保留状态][备份(u16)][校验和]
 * 
 * 校验和计算: 从索引 1 到 70（不含校验和位置）的所有字节 XOR
 * 
 * @return std::vector<uint8_t> 72 字节序列化向量
 */
std::vector<uint8_t> ServoPacket::serialize_servo() const {
    std::vector<uint8_t> out;
    out.push_back(header);
    out.push_back(frame_len);
    out.push_back(seq);
    out.push_back(device_type);
    out.push_back(packet_type);
    out.push_back(device_ip);
    out.push_back(reserved_conn);
    out.push_back(control);
    out.push_back(reserved_ctrl);
    
    // 写入浮点数值（小端字节序）
    write_le_float(out, azimuth);
    write_le_float(out, elevation);
    write_le_float(out, az_speed);
    write_le_float(out, el_speed);
    
    // 写入目标距离
    write_le_uint16(out, target_distance);
    
    // 写入 33 字节保留字段
    if (reserved.size() < 33) {
        std::vector<uint8_t> r = reserved;
        r.resize(33, 0x00);
        out.insert(out.end(), r.begin(), r.end());
    } else {
        out.insert(out.end(), reserved.begin(), reserved.begin() + 33);
    }
    
    // 写入剩余字段
    write_le_uint16(out, reserved_horz);
    write_le_uint16(out, reserved_vert);
    write_le_uint32(out, timestamp);
    out.push_back(reserved_state);
    write_le_uint16(out, backup);
    
    // 计算校验和: XOR 索引 1 到 70
    uint8_t cs = 0;
    for (size_t i = 1; i < 71; ++i) cs ^= out[i];
    out.push_back(cs);
    return out;
}

/**
 * @brief 从字节流反序列化舵机数据包
 * 
 * 验证流程:
 * 1. 检查缓冲区最小长度（至少 72 字节）
 * 2. 验证帧头 (0x7E)
 * 3. 解析所有字段
 * 4. 验证 XOR 校验和（索引 1 到 70）
 * 
 * @param buf 输入字节缓冲区（至少 72 字节）
 * @return std::optional<ServoPacket> 成功时返回 ServoPacket 对象，失败返回 std::nullopt
 */
std::optional<ServoPacket> ServoPacket::deserialize_servo(const std::vector<uint8_t>& buf) {
    if (buf.size() < 72) return std::nullopt;
    
    ServoPacket s;
    size_t idx = 0;
    s.header = buf[idx++];
    if (s.header != 0x7E) return std::nullopt;
    
    s.frame_len = buf[idx++];
    s.seq = buf[idx++];
    s.device_type = buf[idx++];
    s.packet_type = buf[idx++];
    s.device_ip = buf[idx++];
    s.reserved_conn = buf[idx++];
    s.control = buf[idx++];
    s.reserved_ctrl = buf[idx++];
    
    // 辅助 lambda: 读取小端整数/浮点
    auto read_le_uint32 = [&](uint32_t &out)->void{
        out = (uint32_t)buf[idx] | ((uint32_t)buf[idx+1]<<8) | ((uint32_t)buf[idx+2]<<16) | ((uint32_t)buf[idx+3]<<24);
        idx += 4;
    };
    auto read_le_uint16 = [&](uint16_t &out)->void{
        out = (uint16_t)buf[idx] | ((uint16_t)buf[idx+1]<<8);
        idx += 2;
    };
    auto read_le_float = [&](float &out)->void{
        uint32_t v;
        read_le_uint32(v);
        memcpy(&out, &v, 4);
    };
    
    // 读取浮点数值
    read_le_float(s.azimuth);
    read_le_float(s.elevation);
    read_le_float(s.az_speed);
    read_le_float(s.el_speed);
    
    // 读取剩余字段
    read_le_uint16(s.target_distance);
    s.reserved.assign(buf.begin() + idx, buf.begin() + idx + 33);
    idx += 33;
    read_le_uint16(s.reserved_horz);
    read_le_uint16(s.reserved_vert);
    read_le_uint32(s.timestamp);
    s.reserved_state = buf[idx++];
    read_le_uint16(s.backup);
    s.checksum = buf[idx++];
    
    // 验证校验和: XOR 索引 1 到 70
    uint8_t cs = 0;
    for (size_t i = 1; i < 71; ++i) cs ^= buf[i];
    if (cs != s.checksum) return std::nullopt;
    
    return s;
}

// ============================================================================
// 高层舵机数据包构建
// ============================================================================

/**
 * @brief 构建完整的舵机控制数据包
 * 
 * 这是高层接口函数，用于构建标准舵机控制指令并序列化。
 * 默认构造一个定点报告类型 (packet_type = 0x02) 的数据包。
 * 
 * @param azimuth 方位角（度），范围通常为 -180° 到 +180°
 * @param elevation 俯仰角（度），范围通常为 -90° 到 +90°
 * @param az_speed 方位角速度（度/秒）
 * @param el_speed 俯仰角速度（度/秒）
 * @param target_distance 目标距离（毫米）
 * @param seq 序列号（用于匹配请求和响应）
 * @param control 控制字节（0xFF 通常表示运动控制）
 * @param device_type 设备类型（0x01 表示舵机控制器）
 * @param packet_type 数据包类型（0x02 表示定点报告）
 * @return std::vector<uint8_t> 序列化后的 72 字节向量
 */
std::vector<uint8_t> build_servo_packet(float azimuth, float elevation, float az_speed, float el_speed, uint16_t target_distance, uint8_t seq, uint8_t control, uint8_t device_type, uint8_t packet_type) {
    ServoPacket s;
    s.header = 0x7E;
    s.frame_len = 0x48;
    s.seq = seq;
    s.device_type = device_type;
    s.packet_type = packet_type;
    s.device_ip = 0x00;
    s.reserved_conn = 0x00;
    s.control = control;
    s.reserved_ctrl = 0x00;
    s.azimuth = azimuth;
    s.elevation = elevation;
    s.az_speed = az_speed;
    s.el_speed = el_speed;
    s.target_distance = target_distance;
    s.reserved = std::vector<uint8_t>(33, 0);
    s.reserved_horz = 0;
    s.reserved_vert = 0;
    s.timestamp = 0;
    s.reserved_state = 0;
    s.backup = 0;
    return s.serialize_servo();
}

} // namespace cammon