#ifndef CAM_WIND_SRC_PROTOCOL_H
#define CAM_WIND_SRC_PROTOCOL_H

/**
 * @file src/protocol.h
 * @brief 摄像头监控系统通信协议定义
 * @author marblech
 * @date 2026-06-02
 * @copyright SPDX: MIT OR as-project-specifies
 *
 * 本头文件定义了摄像头监控系统所使用的通信协议格式，
 * 包括标准数据包结构、舵机专用数据包结构、设备地址枚举
 * 以及相关的序列化/反序列化函数。
 *
 * 协议支持两种帧格式：
 * - 标准帧：以 0x0F 0xF0 开头，0xF0 0x0F 结尾，最小帧长 23 字节
 * - 舵机帧：以 0x7E 开头，固定帧长 72 字节
 */

#include <cstdint>
#include <vector>
#include <optional>

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

// ---------------------------------------------------------------------------
// 上报帧头/帧尾常量 (协议 3.4 节)
// ---------------------------------------------------------------------------

/**
 * @brief 上报帧头字节1
 * 
 * 用于设备回送给综合显控软件的上报帧起始标志。
 * 协议文档 3.4 节定义：帧头为 0x1F 0xF1
 */
static constexpr uint8_t REPORT_HDR1 = 0x1F;

/**
 * @brief 上报帧头字节2
 * 
 * 与 REPORT_HDR1 一起构成上报帧头 0x1F 0xF1
 */
static constexpr uint8_t REPORT_HDR2 = 0xF1;

/**
 * @brief 上报帧尾字节1
 * 
 * 上报帧的尾部标志，与 REPORT_TAIL2 一起构成 0xF1 0x1F
 */
static constexpr uint8_t REPORT_TAIL1 = 0xF1;

/**
 * @brief 上报帧尾字节2
 * 
 * 与 REPORT_TAIL1 一起构成上报帧尾 0xF1 0x1F
 */
static constexpr uint8_t REPORT_TAIL2 = 0x1F;

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
    ADDR_SERVO      = 0x05, ///< 舵机控制器地址
    ADDR_SCAN       = 0x06, ///< 扫描生成器地址
    ADDR_OSD        = 0x07  ///< OSD实时信息地址
};

// ---------------------------------------------------------------------------
// 上报帧地址位定义 (协议 3.4.1)
// ---------------------------------------------------------------------------

/**
 * @brief 上报帧地址位：识别信息
 * 
 * 协议 3.4.2 节定义：地址位为 0x02，表示识别信息上报
 */
static constexpr uint8_t REPORT_ADDR_DETECT_INFO = 0x02;

/**
 * @brief 上报帧地址位：跟踪信息
 * 
 * 协议 3.4.3 节定义：地址位为 0x03，表示跟踪信息上报
 */
static constexpr uint8_t REPORT_ADDR_TRACK_INFO = 0x03;

/**
 * @brief 上报帧地址位：负载状态（白光通道）
 * 
 * 协议 3.4.1 节定义：地址位 0x00 表示白光通道负载状态上报
 */
static constexpr uint8_t REPORT_ADDR_LOAD_STATUS_VIS = 0x00;

/**
 * @brief 上报帧地址位：负载状态（红外通道）
 * 
 * 协议 3.4.1 节定义：地址位 0x01 表示红外通道负载状态上报
 */
static constexpr uint8_t REPORT_ADDR_LOAD_STATUS_IR = 0x01;
/**
 * @brief 可见光相机功能位（功能码）
 *
 * 对应协议 v0.07 中相机功能定义（表 3-5 / 3-6）。
 * 每个功能码指出上位机要相机执行的高层操作，
 * 具体的数据字段布局见协议文档相应条目。
 */
enum VisibleCameraFunction : uint8_t {
    VC_FUNC_CONTINUOUS_ZOOM      = 0x01, ///< 连续变焦（data[0] = 速度，范围 0-10）
    VC_FUNC_STEP_ZOOM            = 0x02, ///< 步进变焦（data[0] = 步距，范围 0-10）
    VC_FUNC_CONTINUOUS_FOCUS     = 0x03, ///< 连续调焦（data[0] = 速度，范围 0-10）
    VC_FUNC_SINGLE_STEP_FOCUS    = 0x04, ///< 单步调焦（data[0] = 步距，范围 0-10）
    VC_FUNC_AUTO_FOCUS           = 0x05, ///< 自动聚焦（触发式或状态式，见 control 含义）
    VC_FUNC_FOCAL_DIRECT         = 0x06, ///< 焦距直达（data 中为目标焦距，float 小端）
    VC_FUNC_FOCUS_DIRECT         = 0x07, ///< 聚焦直达（data 中为目标聚焦值，float 或指定格式）
    VC_FUNC_DEFOG                = 0x08, ///< 光学透雾 / 去雾（data[0]：0 关、1 开）
    VC_FUNC_BRIGHTNESS           = 0x09, ///< 亮度调整（data[0] 表示增减或直达值，详见协议）
    VC_FUNC_CONTRAST             = 0x0A, ///< 对比度调整
    VC_FUNC_IMAGE_ENHANCE        = 0x0B  ///< 图像增强
};

/**
 * @brief 相机控制位（Control 值的常用含义）
 *
 * 协议中 control 字节的含义依功能而异；此处列出常见值的语义，
 * 示例：对变焦/调焦类功能，0x01 常用于“+”（增加），0x00 常用于“停/减”。
 */
enum CameraControl : uint8_t {
    CAM_CTRL_STOP         = 0x00, ///< 停止 / 关闭 / 或作为“减/停止”的通用编码
    CAM_CTRL_CONT_PLUS    = 0x01, ///< 连续/步进操作的“增加 / 向正”控制
    CAM_CTRL_CONT_MINUS   = 0x02, ///< 连续/步进操作的“减少 / 向负”控制
    CAM_CTRL_ZOOM         = 0x05, ///< 变焦操作的控制码示例（协议示例中用到）
    CAM_CTRL_FOCAL        = 0x06  ///< 焦距直达控制码示例（协议示例中用到）
};

// 通用常量
static constexpr uint8_t DEFAULT_SEQ = 0x01;
static constexpr uint8_t SERVO_CTRL_POSITION = 0x11; ///< 伺服位置控制字节（常用）
static constexpr uint8_t SERVO_DEVICE_TYPE = 0x01;    ///< 舵机设备类型
static constexpr uint8_t SERVO_PACKET_TYPE_POINT = 0x02; ///< 定点报文类型
static constexpr uint8_t DEFAULT_MOVE_AMOUNT = 0x0A; ///< 默认移动量（十字线等）


// ============================================================================
// 负载状态上报数据结构 (协议 3.4.1)
// ============================================================================

/**
 * @brief 负载状态上报数据长度
 * 
 * 协议 3.4.1 节定义：数据位包含 41 字节（字节号 0~40，对应参数帧头0-1到当前检测/识别阈值41）
 */
static constexpr size_t REPORT_LOAD_STATUS_DATA_LEN = 41;

/**
 * @brief 负载状态上报总帧长
 * 
 * 2(帧头) + 1(地址) + 2(帧序号) + 41(数据) + 1(校验) + 2(帧尾) = 49 字节
 */
static constexpr size_t REPORT_LOAD_STATUS_FRAME_LEN = 49;

/**
 * @brief 负载状态上报数据结构
 * 
 * 对应协议 3.4.1 节 "负载状态上报" 定义。
 * 帧格式: [1F F1][地址][帧序号][数据41字节][校验][F1 1F]
 * 
 * 数据位各字段定义（字节号从0开始）：
 * - [0-1]: 帧头 0x1F 0xF1
 * - [2]: 地址位 (0x00白光通道/0x01红外通道)
 * - [3-4]: 帧序号
 * - [5]: 保留
 * - [6-9]: 红外焦距 (float)
 * - [10-13]: 白光焦距 (float)
 * - [14-17]: 方位脱靶量 (float)
 * - [18-21]: 俯仰脱靶量 (float)
 * - [22-25]: 环控设备状态 (4字节，01**02**格式)
 * - [26-29]: 伺服方位角 (float)
 * - [30-33]: 伺服俯仰角 (float)
 * - [34]: 自检状态 (Bit0=0分系统, Bit1=相机, Bit2=跟踪处理器, Bit3=自检结束标志)
 * - [35]: 可见光摄像机图像状态 (00=有图像, 01=无图像)
 * - [36]: 可见光摄像机通信状态 (00=有通信, 01=无通信)
 * - [37]: 跟踪处理器温度 (摄氏度)
 * - [38]: 可见光摄像机温度 (摄氏度)
 * - [39]: 电子放大状态 (00=无变倍, 01=2X, 02=4X)
 * - [40]: 电子透雾状态 (00=无效, 01=有效)
 * - [41]: 当前曝光时间 (0.1毫秒)
 * - [42]: 当前检测/识别阈值 (0-100)
 * - [43]: 当前跟踪阈值 (0-100)
 * - [44-45]: 当前记忆跟踪时间 (毫秒)
 */
struct LoadStatusReport {
    uint8_t hdr1 = REPORT_HDR1;              ///< 帧头字节1 (固定为 0x1F)
    uint8_t hdr2 = REPORT_HDR2;              ///< 帧头字节2 (固定为 0xF1)
    uint8_t addr = 0;                        ///< 地址位 (0x00=白光通道, 0x01=红外通道)
    uint16_t frame_seq = 0;                  ///< 帧序号 (0x0000-0xFFFF)
    
    // 数据区字段
    uint8_t reserved1 = 0;                   ///< [5] 保留
    float ir_focal_length = 0.0f;            ///< [6-9] 红外焦距 (float)
    float vis_focal_length = 0.0f;           ///< [10-13] 白光焦距 (float)
    float az_aberration = 0.0f;              ///< [14-17] 方位脱靶量 (float)
    float el_aberration = 0.0f;              ///< [18-21] 俯仰脱靶量 (float)
    
    // 环控设备状态 (4字节)
    uint8_t env_ctrl_byte1 = 0;              ///< [22] 环控设备1状态字节
    uint8_t env_ctrl_byte2 = 0;              ///< [23] 环控设备2状态字节
    uint8_t env_ctrl_byte3 = 0;              ///< [24] 环控设备3状态字节
    uint8_t env_ctrl_byte4 = 0;              ///< [25] 环控设备4状态字节
    
    float servo_azimuth = 0.0f;              ///< [26-29] 伺服方位角 (float)
    float servo_elevation = 0.0f;            ///< [30-33] 伺服俯仰角 (float)
    
    // 自检状态
    uint8_t self_check_status = 0;           ///< [34] 自检状态
    //   Bit0: 0=分系统正常, 1=异常
    //   Bit1: 0=相机正常, 1=异常
    //   Bit2: 0=跟踪处理器正常, 1=异常
    //   Bit3: 0=自检结束, 1=自检中
    //   Bit4-7: 备用
    
    uint8_t vis_cam_image_status = 0;        ///< [35] 可见光摄像机图像状态 (00=有图像, 01=无图像)
    uint8_t vis_cam_comm_status = 0;         ///< [36] 可见光摄像机通信状态 (00=有通信, 01=无通信)
    uint8_t tracker_temp = 0;                ///< [37] 跟踪处理器温度 (摄氏度)
    uint8_t vis_cam_temp = 0;                ///< [38] 可见光摄像机温度 (摄氏度)
    uint8_t electronic_zoom_status = 0;      ///< [39] 电子放大状态 (00=无变倍, 01=2X, 02=4X)
    uint8_t defog_status = 0;                ///< [40] 电子透雾状态 (00=无效, 01=有效)
    uint8_t current_exposure = 0;            ///< [41] 当前曝光时间 (0.1毫秒)
    uint8_t detect_recog_threshold = 0;      ///< [42] 当前检测/识别阈值 (0-100)
    uint8_t track_threshold = 0;             ///< [43] 当前跟踪阈值 (0-100)
    uint16_t memory_track_time = 0;          ///< [44-45] 当前记忆跟踪时间 (毫秒)
    
    uint8_t checksum = 0;                    ///< 校验和 (地址位到数据位和校验取低位1字节)
    uint8_t tail1 = REPORT_TAIL1;            ///< 帧尾字节1 (固定为 0xF1)
    uint8_t tail2 = REPORT_TAIL2;            ///< 帧尾字节2 (固定为 0x1F)
    
    /**
     * @brief 将负载状态上报数据序列化为字节流
     * 
     * 序列化格式: [1F F1][地址][帧序号][数据41字节][校验][F1 1F]
     * 总长度: 49 字节
     * 
     * @return std::vector<uint8_t> 序列化后的字节向量
     */
    std::vector<uint8_t> serialize();
    
    /**
     * @brief 从字节流反序列化负载状态上报数据
     * 
     * @param buf 输入字节缓冲区 (至少 49 字节)
     * @return std::optional<LoadStatusReport> 成功时返回解析好的对象，失败返回 std::nullopt
     */
    static std::optional<LoadStatusReport> deserialize(const std::vector<uint8_t>& buf);
};

/**
 * @brief 从字节流反序列化负载状态上报数据
 * 
 * 便捷函数，封装 LoadStatusReport::deserialize 并自动验证帧头帧尾。
 * 
 * @param buf 输入字节缓冲区
 * @return std::optional<LoadStatusReport> 成功时返回解析好的负载状态上报数据，失败返回 std::nullopt
 */
std::optional<LoadStatusReport> parse_load_status_report(const std::vector<uint8_t>& buf);

/**
 * @brief 兼容的负载状态解析器
 *
 * 一些测试发送端（例如 Python 工具）可能会把负载状态封装在标准帧
 * 或者直接以上报帧格式发送。本函数会尝试：
 *  - 若缓冲区以上报帧头 `REPORT_HDR1, REPORT_HDR2` 开头，则调用
 *    `LoadStatusReport::deserialize` 进行解析；
 *  - 否则若以标准帧头 `FRAME_HDR1, FRAME_HDR2` 开头，则尝试解析为
 *    `Packet`，并从其中抽取负载数据区去填充 `LoadStatusReport`。
 *
 * 这样可以在不改变现有代码的前提下兼容 Python 本地点对点发送器
 * 以及其他用不同封装方式发送的负载状态报文。
 */
std::optional<LoadStatusReport> parse_payload_status(const std::vector<uint8_t>& buf);

// ============================================================================
// 标准数据包结构
// ============================================================================

// ---------------------------------------------------------------------------
// 协议功能码枚举补充（跟随协议 v0.07 文档）
// ---------------------------------------------------------------------------

/**
 * @brief 跟踪处理器 / 可见光 / 红外 功能码（高层命令分类）
 *
 * 常用值参考协议 3.3.3 / 3.3.4 中的定义。
 */
enum TrackerFunction : uint8_t {
    TRACKER_FUNC_DISPLAY_SWITCH        = 0x00, ///< 显示切换（可见/红外等）
    TRACKER_FUNC_CROSSHAIR_SHOW        = 0x01, ///< 十字线显示/消隐（control 指定显示或消隐）
    TRACKER_FUNC_CROSSHAIR_UP          = 0x02, ///< 十字线上移（data[0]=移动像素数量）
    TRACKER_FUNC_CROSSHAIR_DOWN        = 0x03, ///< 十字线下移
    TRACKER_FUNC_CROSSHAIR_LEFT        = 0x04, ///< 十字线左移
    TRACKER_FUNC_CROSSHAIR_RIGHT       = 0x05, ///< 十字线右移
    TRACKER_FUNC_SET_CROSSHAIR_POS     = 0x06, ///< 设置十字线位置（data[0-1]=X, [2-3]=Y）
    TRACKER_FUNC_SAVE_CROSSHAIR        = 0x07, ///< 保存十字线位置与显隐状态
    TRACKER_FUNC_OSD_DISPLAY           = 0x08, ///< OSD 显示控制（开启/文字/刻度等）
    TRACKER_FUNC_TIME_SYNC             = 0x03  ///< 时统设置（在某些实现中使用，见文档）
};

/**
 * @brief 跟踪器控制位（用于指定具体动作，如上/下/左/右）
 */
enum TrackerControl : uint8_t {
    TRACKER_CTRL_SHOW      = 0x01, ///< 十字线显示/消隐
    TRACKER_CTRL_UP        = 0x02, ///< 十字线上移
    TRACKER_CTRL_DOWN      = 0x03, ///< 十字线下移
    TRACKER_CTRL_LEFT      = 0x04, ///< 十字线左移
    TRACKER_CTRL_RIGHT     = 0x05  ///< 十字线右移
};

/**
 * @brief 跟踪相关的检测/识别/跟踪指令枚举（部分）
 *
 * 这些功能码用于控制检测、识别与跟踪流程，详见协议 3.3.3 / 3.3.4。
 */
enum DetectionFunction : uint8_t {
    DETECT_FUNC_POINT_TARGET        = 0x04, ///< 检测（点目标）
    DETECT_FUNC_SENSITIVITY_SET     = 0x02, ///< 检测灵敏度设置
    DETECT_FUNC_DETECT_TO_TRACK_ON  = 0x03, ///< 检测转跟踪 开
    DETECT_FUNC_DETECT_TO_TRACK_OFF = 0x04  ///< 检测转跟踪 关
};

/**
 * @brief 识别（类别）相关函数码与控制
 *
 * 识别功能包括：开启/关闭识别、设定识别区域、识别灵敏度等。
 */
enum RecognitionFunction : uint8_t {
    RECOG_FUNC_ENABLE               = 0x00, ///< 识别开关（0: 关, 1: 开）
    RECOG_FUNC_AREA_KIND_TIME_SET   = 0x01, ///< 识别区域、种类与时间设置
    RECOG_FUNC_SENSITIVITY_SET      = 0x02, ///< 识别灵敏度设置
    RECOG_FUNC_RECOG_TO_TRACK_ON    = 0x03, ///< 识别转跟踪 开
    RECOG_FUNC_RECOG_TO_TRACK_OFF   = 0x04  ///< 识别转跟踪 关
};

/**
 * @brief 识别目标类别位掩码（data 字节按位表示各类是否开启/选择）
 *
 * 对应协议说明：Bit0=人, Bit1=车, Bit2=船, Bit3=飞机, Bit4=无人机, Bit5-7 预留
 */
enum IdentificationCategory : uint8_t {
    ID_CAT_PERSON   = 1u << 0, ///< 人
    ID_CAT_VEHICLE  = 1u << 1, ///< 车
    ID_CAT_SHIP     = 1u << 2, ///< 船
    ID_CAT_AIRCRAFT = 1u << 3, ///< 飞机
    ID_CAT_DRONE    = 1u << 4  ///< 无人机
};

/**
 * @brief 环境控制（环控）功能位
 *
 * 参见协议 3.3.5（雨刷、加热等）
 */
enum EnvControlFunction : uint8_t {
    ENV_FUNC_WIPER      = 0x01, ///< 雨刷控制（0 关闭，FF 打开）
    ENV_FUNC_HEATER     = 0x02  ///< 加热（0 关闭，FF 打开）
};

/**
 * @brief 跟踪/控制命令（通用跟踪类功能码示例）
 *
 * 例如点选跟踪、识别辅助点选跟踪、设置波门、手动框选等。
 */

enum TrackCommand : uint8_t {
    TRACK_CMD_POINT_SELECT_TRACK = 0x00, ///< 点选跟踪
    TRACK_CMD_RECOG_AID_SELECT   = 0x01, ///< 识别辅助点选跟踪
    TRACK_CMD_SET_LOST_THRESHOLD = 0x02, ///< 跟踪丢失阈值
    TRACK_CMD_RECAPTURE_THRESHOLD= 0x03, ///< 重捕获阈值
    TRACK_CMD_CANCEL_TRACK       = 0x04, ///< 取消跟踪
    TRACK_CMD_ADAPTIVE_GATE_ON   = 0x05, ///< 自适应波门开
    TRACK_CMD_SET_DEFAULT_GATE   = 0x06, ///< 设置默认波门尺寸
    TRACK_CMD_MEMORY_TRACK_TIME  = 0x07  ///< 记忆跟踪时间设置
};

/**
 * @brief 扫描生成器功能（扫描方式）
 *
 * 参见协议 3.3.6：预置点巡航、线扫、帧扫描、苹果皮扫描等。
 */
enum ScanFunction : uint8_t {
    SCAN_FUNC_SET_MODE       = 0x01, ///< 设定扫描方式
    SCAN_FUNC_PRESET_WRITE   = 0x02, ///< 预置点写入 / 操作
    SCAN_FUNC_PRESET_READ    = 0x02  ///< 预置点读取（同编码，区分 control/子域）
};

/**
 * @brief 伺服控制（Servo）中的控制位 / 模式位掩码说明
 *
 * 协议对伺服 control 字节的位有约定：低四位与高四位可能表示不同含义（测试/位置/速度等），
 * 此处定义常用常量供调用端形成 control 字节。
 */
enum ServoControlFlags : uint8_t {
    SERVO_CTRL_STOP           = 0x00, ///< 停止
    SERVO_CTRL_TEST_MODE_MASK = 0x80, ///< 测试模式掩码示例（高位）
    SERVO_CTRL_POSITION_MASK  = 0x01, ///< 位置模式（低位示例）
    SERVO_CTRL_SPEED_MASK     = 0x02  ///< 速度模式（低位示例）
};

// ---------------------------------------------------------------------------
// 结束 协议功能码枚举补充
// ---------------------------------------------------------------------------


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

#endif /* CAM_WIND_SRC_PROTOCOL_H */