#include "protocol.h"
#include <cstring>
#include <cstdio>

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
uint8_t compute_checksum(const std::vector<uint8_t>& bytes) {
    // 协议 3.2 节定义：校验位 = 除帧头帧尾外所有字节的和校验 (mod 256)
    // 即: checksum = sum(addr, func, ctrl, data...) mod 256
    uint8_t cs = 0;
    for (auto b : bytes) cs = static_cast<uint8_t>(cs + b);
    return cs;
}

// 为兼容旧代码保留别名
uint8_t compute_xor_checksum(const std::vector<uint8_t>& bytes) {
    return compute_checksum(bytes);
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
    
    // 计算校验和：从地址到数据区（和校验 mod 256）
    std::vector<uint8_t> csrange;
    csrange.push_back(addr);
    csrange.push_back(func);
    csrange.push_back(ctrl);
    csrange.insert(csrange.end(), d.begin(), d.end());
    uint8_t cs = compute_checksum(csrange);
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

                // 验证校验和（地址..数据，和校验 mod 256）
                std::vector<uint8_t> csrange;
                csrange.push_back(p.addr);
                csrange.push_back(p.func);
                csrange.push_back(p.ctrl);
                csrange.insert(csrange.end(), p.data.begin(), p.data.end());
                if (compute_checksum(csrange) != p.checksum) {
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
 * 校验和计算: 从索引 0 到 70（包含帧头 0x7E，在校验字节之前的所有字节）
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
    
    // 计算校验和: XOR 索引 0 到 70（包含帧头 0x7E）
    uint8_t cs = 0;
    for (size_t i = 0; i < 71; ++i) cs ^= out[i];
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
 * 4. 验证 XOR 校验和（索引 0 到 70）
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
    
    // 验证校验和: XOR 索引 0 到 70（包含帧头 0x7E）
    uint8_t cs = 0;
    for (size_t i = 0; i < 71; ++i) cs ^= buf[i];
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

// ============================================================================
// 负载状态上报序列化/反序列化 (协议 3.4.1)
// ============================================================================

namespace cammon {

/**
 * @brief 将负载状态上报数据序列化为字节流
 * 
 * 序列化格式: [1F F1][地址][帧序号][数据41字节][校验][F1 1F]
 * 总长度: 49 字节
 * 
 * 数据区布局（字节号从0开始，相对于数据区起始）：
 * - [0]: 保留
 * - [1-4]: 红外焦距 (float)
 * - [5-8]: 白光焦距 (float)
 * - [9-12]: 方位脱靶量 (float)
 * - [13-16]: 俯仰脱靶量 (float)
 * - [17-20]: 环控设备状态 (4字节)
 * - [21-24]: 伺服方位角 (float)
 * - [25-28]: 伺服俯仰角 (float)
 * - [29]: 自检状态
 * - [30]: 可见光摄像机图像状态
 * - [31]: 可见光摄像机通信状态
 * - [32]: 跟踪处理器温度
 * - [33]: 可见光摄像机温度
 * - [34]: 电子放大状态
 * - [35]: 电子透雾状态
 * - [36]: 当前曝光时间
 * - [37]: 当前检测/识别阈值
 * - [38]: 当前跟踪阈值
 * - [39-40]: 当前记忆跟踪时间 (uint16_t)
 * 
 * 注意：协议文档中字节号从帧头开始计算，此处转换为相对于数据区的偏移。
 * 协议字节号0-1为帧头，2为地址，3-4为帧序号，5开始为数据区。
 * 
 * @return std::vector<uint8_t> 序列化后的49字节向量
 */
std::vector<uint8_t> LoadStatusReport::serialize() {
    std::vector<uint8_t> out;
    out.reserve(REPORT_LOAD_STATUS_FRAME_LEN);
    
    // 帧头
    out.push_back(hdr1);  // 0x1F
    out.push_back(hdr2);  // 0xF1
    
    // 地址位
    out.push_back(addr);
    
    // 帧序号 (小端)
    write_le_uint16(out, frame_seq);
    
    // 数据区 (从协议字节号5开始，对应数据区偏移0)
    // [5] 保留
    out.push_back(reserved1);
    
    // [6-9] 红外焦距 (float)
    write_le_float(out, ir_focal_length);
    
    // [10-13] 白光焦距 (float)
    write_le_float(out, vis_focal_length);
    
    // [14-17] 方位脱靶量 (float)
    write_le_float(out, az_aberration);
    
    // [18-21] 俯仰脱靶量 (float)
    write_le_float(out, el_aberration);
    
    // [22-25] 环控设备状态 (4字节)
    out.push_back(env_ctrl_byte1);
    out.push_back(env_ctrl_byte2);
    out.push_back(env_ctrl_byte3);
    out.push_back(env_ctrl_byte4);
    
    // [26-29] 伺服方位角 (float)
    write_le_float(out, servo_azimuth);
    
    // [30-33] 伺服俯仰角 (float)
    write_le_float(out, servo_elevation);
    
    // [34] 自检状态
    out.push_back(self_check_status);
    
    // [35] 可见光摄像机图像状态
    out.push_back(vis_cam_image_status);
    
    // [36] 可见光摄像机通信状态
    out.push_back(vis_cam_comm_status);
    
    // [37] 跟踪处理器温度
    out.push_back(tracker_temp);
    
    // [38] 可见光摄像机温度
    out.push_back(vis_cam_temp);
    
    // [39] 电子放大状态
    out.push_back(electronic_zoom_status);
    
    // [40] 电子透雾状态
    out.push_back(defog_status);
    
    // [41] 当前曝光时间
    out.push_back(current_exposure);
    
    // [42] 当前检测/识别阈值
    out.push_back(detect_recog_threshold);
    
    // [43] 当前跟踪阈值
    out.push_back(track_threshold);
    
    // [44-45] 当前记忆跟踪时间 (uint16_t 小端)
    write_le_uint16(out, memory_track_time);
    
    // 校验和：地址位到数据位和校验取低位1字节
    // 即: sum(addr, frame_seq[0], frame_seq[1], data...) mod 256
    // 注意：协议定义"地址位到数据位和校验"，地址位之后的所有字节
    std::vector<uint8_t> csrange;
    csrange.push_back(addr);
    // 帧序号已在上面以字节形式写入out，需要从out中提取
    // out当前长度 = 2(帧头) + 1(地址) + 2(帧序号) + 41(数据) = 46
    // 数据区从out[5]开始
    for (size_t i = 3; i < out.size(); ++i) {
        csrange.push_back(out[i]);
    }
    checksum = compute_checksum(csrange);
    out.push_back(checksum);
    
    // 帧尾
    out.push_back(tail1);  // 0xF1
    out.push_back(tail2);  // 0x1F
    
    return out;
}

/**
 * @brief 从字节流反序列化负载状态上报数据
 * 
 * @param buf 输入字节缓冲区 (至少 49 字节)
 * @return std::optional<LoadStatusReport> 成功时返回解析好的对象，失败返回 std::nullopt
 */
std::optional<LoadStatusReport> LoadStatusReport::deserialize(const std::vector<uint8_t>& buf) {
    if (buf.size() < REPORT_LOAD_STATUS_FRAME_LEN) return std::nullopt;
    
    LoadStatusReport r;
    
    // 验证帧头
    if (buf[0] != REPORT_HDR1 || buf[1] != REPORT_HDR2) return std::nullopt;
    
    size_t idx = 2;
    // 按协议布局：buf[0-1]=帧头, buf[2]=addr, buf[3-4]=frame_seq (小端), buf[5...] = data
    r.addr = buf[idx++];

    // 帧序号 (小端)
    r.frame_seq = (uint16_t)buf[idx] | ((uint16_t)buf[idx+1] << 8);
    idx += 2;
    
    // 数据区
    r.reserved1 = buf[idx++];                                    // [5]
    // [6-9] 红外焦距
    { uint32_t v; v = (uint32_t)buf[idx] | ((uint32_t)buf[idx+1]<<8) | ((uint32_t)buf[idx+2]<<16) | ((uint32_t)buf[idx+3]<<24); memcpy(&r.ir_focal_length, &v, 4); idx += 4; }
    // [10-13] 白光焦距
    { uint32_t v; v = (uint32_t)buf[idx] | ((uint32_t)buf[idx+1]<<8) | ((uint32_t)buf[idx+2]<<16) | ((uint32_t)buf[idx+3]<<24); memcpy(&r.vis_focal_length, &v, 4); idx += 4; }
    // [14-17] 方位脱靶量
    { uint32_t v; v = (uint32_t)buf[idx] | ((uint32_t)buf[idx+1]<<8) | ((uint32_t)buf[idx+2]<<16) | ((uint32_t)buf[idx+3]<<24); memcpy(&r.az_aberration, &v, 4); idx += 4; }
    // [18-21] 俯仰脱靶量
    { uint32_t v; v = (uint32_t)buf[idx] | ((uint32_t)buf[idx+1]<<8) | ((uint32_t)buf[idx+2]<<16) | ((uint32_t)buf[idx+3]<<24); memcpy(&r.el_aberration, &v, 4); idx += 4; }
    // [22-25] 环控设备
    r.env_ctrl_byte1 = buf[idx++];
    r.env_ctrl_byte2 = buf[idx++];
    r.env_ctrl_byte3 = buf[idx++];
    r.env_ctrl_byte4 = buf[idx++];
    // [26-29] 伺服方位角
    { uint32_t v; v = (uint32_t)buf[idx] | ((uint32_t)buf[idx+1]<<8) | ((uint32_t)buf[idx+2]<<16) | ((uint32_t)buf[idx+3]<<24); memcpy(&r.servo_azimuth, &v, 4); idx += 4; }
    // [30-33] 伺服俯仰角
    { uint32_t v; v = (uint32_t)buf[idx] | ((uint32_t)buf[idx+1]<<8) | ((uint32_t)buf[idx+2]<<16) | ((uint32_t)buf[idx+3]<<24); memcpy(&r.servo_elevation, &v, 4); idx += 4; }
    // [34] 自检状态
    r.self_check_status = buf[idx++];
    // [35] 可见光摄像机图像状态
    r.vis_cam_image_status = buf[idx++];
    // [36] 可见光摄像机通信状态
    r.vis_cam_comm_status = buf[idx++];
    // [37] 跟踪处理器温度
    r.tracker_temp = buf[idx++];
    // [38] 可见光摄像机温度
    r.vis_cam_temp = buf[idx++];
    // [39] 电子放大状态
    r.electronic_zoom_status = buf[idx++];
    // [40] 电子透雾状态
    r.defog_status = buf[idx++];
    // [41] 当前曝光时间
    r.current_exposure = buf[idx++];
    // [42] 当前检测/识别阈值
    r.detect_recog_threshold = buf[idx++];
    // [43] 当前跟踪阈值
    r.track_threshold = buf[idx++];
    // [44-45] 记忆跟踪时间
    r.memory_track_time = (uint16_t)buf[idx] | ((uint16_t)buf[idx+1] << 8);
    idx += 2;
    
    // 校验和
    r.checksum = buf[idx++];
    
    // 帧尾
    r.tail1 = buf[idx++];
    r.tail2 = buf[idx++];
    
    // 验证帧尾
    if (r.tail1 != REPORT_TAIL1 || r.tail2 != REPORT_TAIL2) {
        fprintf(stderr, "[LoadStatusReport] 帧尾不匹配: %02X %02X (期望 %02X %02X)\n", r.tail1, r.tail2, REPORT_TAIL1, REPORT_TAIL2);
        return std::nullopt;
    }

    // 验证校验和：计算地址位到数据位（包含帧序号低字节）之和的低位
    std::vector<uint8_t> csrange;
    csrange.push_back(r.addr);
    // 地址后的第一个字节是帧序号低字节，索引为 3，数据区一直到索引 45
    for (size_t i = 3; i < REPORT_LOAD_STATUS_FRAME_LEN - 3; ++i) {
        csrange.push_back(buf[i]);
    }
    uint8_t cs_calc = compute_checksum(csrange);
    if (cs_calc != r.checksum) {
        fprintf(stderr, "[LoadStatusReport] 校验和不匹配: 期望=%02X, 计算=%02X (addr=%02X, frame_seq=%u)\n",
                r.checksum, cs_calc, r.addr, r.frame_seq);
        // 同时打印前 20 字节的十六进制用于排查
        fprintf(stderr, "[LoadStatusReport] 前20字节: ");
        for (size_t i = 0; i < buf.size() && i < 20; ++i) fprintf(stderr, "%02X ", buf[i]);
        fprintf(stderr, "\n");
        return std::nullopt;
    }
    
    return r;
}

/**
 * @brief 从字节流反序列化负载状态上报数据（便捷函数）
 */
std::optional<LoadStatusReport> parse_load_status_report(const std::vector<uint8_t>& buf) {
    return LoadStatusReport::deserialize(buf);
}

} // namespace cammon
