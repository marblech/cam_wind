/**
 * @file cammon_jni.cpp
 * @brief Camera Monitor JNI 桥接层实现
 * 
 * 本文件实现了 Java 与 C++ 之间的 JNI（Java Native Interface）桥接功能。
 * 提供了两个主要的 JNI 接口函数：
 * - sendCameraCommand: 发送相机控制命令
 * - sendServoCommand: 发送舵机控制命令
 * 
 * 这些函数将 Java 层的调用转换为对 C++ 底层 API 的调用，
 * 实现了跨平台设备控制的统一接口。
 */

#include <jni.h>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include "protocol.h"

extern "C" {
/**
 * @brief 前向声明：C API 函数
 * 
 * 发送相机控制命令到指定设备
 * @param host 目标主机 IP 地址
 * @param port 目标端口号
 * @param func 功能码
 * @param ctrl 控制码
 * @param payload 数据载荷
 * @param payload_len 数据载荷长度
 * @param resp_buf 接收缓冲区
 * @param resp_buf_len 接收缓冲区最大长度
 * @param timeout_ms 超时时间（毫秒）
 * @return 成功返回接收到的字节数，失败返回负 error code
 */
int cammon_send_camera_command(const char* host, int port, uint8_t func, uint8_t ctrl, const uint8_t* payload, int payload_len, uint8_t* resp_buf, int resp_buf_len, int timeout_ms);

/**
 * @brief 前向声明：C API 函数
 * 
 * 发送舵机控制命令到指定设备
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
 * @return 成功返回接收到的字节数，失败返回负 error code
 */
int cammon_send_servo_command(const char* host, int port, float az, float el, float azs, float els, uint16_t target_distance, uint8_t seq, uint8_t control, uint8_t device_type, uint8_t packet_type, uint8_t* resp_buf, int resp_buf_len, int timeout_ms);
}

/**
 * @brief 辅助函数：将 Java jbyteArray 转换为 C++ std::vector<uint8_t>
 * 
 * 用于 JNI 桥接层中数据类型转换，将 Java 字节数组转换为 C++ 向量。
 * 
 * @param env JNI 环境变量指针
 * @param arr Java 字节数组
 * @return 转换后的 uint8_t 向量
 */
static std::vector<uint8_t> to_vector_uint8(JNIEnv* env, jbyteArray arr) {
    std::vector<uint8_t> v;
    if (!arr) return v;
    jsize len = env->GetArrayLength(arr);
    v.resize(len);
    env->GetByteArrayRegion(arr, 0, len, reinterpret_cast<jbyte*>(v.data()));
    return v;
}

extern "C" {

/**
 * @brief JNI 接口：发送相机控制命令
 * 
 * 对应 Java 方法 CamMonNative.sendCameraCommand()
 * 接收 Java 层的参数，调用 C++ 底层 API 发送相机控制命令，
 * 并将响应数据返回给 Java 层。
 * 
 * @param env JNI 环境变量指针
 * @param clazz 调用此方法的 Java 类
 * @param jhost 目标主机 IP 地址（Java String）
 * @param port 目标端口号
 * @param func 功能码
 * @param ctrl 控制码
 * @param jpayload 数据载荷（Java byte[]）
 * @param timeoutMs 超时时间（毫秒）
 * @return 响应数据字节数组，失败返回 NULL
 */
JNIEXPORT jbyteArray JNICALL Java_com_marble_cammon_CamMonNative_sendCameraCommand(JNIEnv* env, jclass, jstring jhost, jint port, jbyte func, jbyte ctrl, jbyteArray jpayload, jint timeoutMs) {
    // 获取 Java 字符串的 UTF-8 编码
    const char* host = env->GetStringUTFChars(jhost, NULL);
    // 将 Java 字节数组转换为 C++ 向量
    auto payload = to_vector_uint8(env, jpayload);
    
    // 定义最大响应缓冲区大小
    const int RESP_MAX = 2048;
    // 使用智能指针管理响应缓冲区内存
    std::unique_ptr<uint8_t[]> resp(new uint8_t[RESP_MAX]);
    
    // 调用 C++ 底层 API 发送相机命令
    int r = cammon_send_camera_command(host, port, (uint8_t)func, (uint8_t)ctrl, 
                                        payload.empty() ? nullptr : payload.data(), 
                                        (int)payload.size(), resp.get(), RESP_MAX, timeoutMs);
    
    // 释放 Java 字符串
    env->ReleaseStringUTFChars(jhost, host);
    
    // 检查返回结果
    if (r <= 0) return NULL;
    
    // 创建 Java 字节数组并复制响应数据
    jbyteArray out = env->NewByteArray(r);
    env->SetByteArrayRegion(out, 0, r, reinterpret_cast<jbyte*>(resp.get()));
    return out;
}

/**
 * @brief JNI 接口：发送舵机控制命令
 * 
 * 对应 Java 方法 CamMonNative.sendServoCommand()
 * 接收 Java 层的参数，调用 C++ 底层 API 发送舵机控制命令，
 * 并将响应数据返回给 Java 层。
 * 
 * @param env JNI 环境变量指针
 * @param clazz 调用此方法的 Java 类
 * @param jhost 目标主机 IP 地址（Java String）
 * @param port 目标端口号
 * @param az 方位角（度）
 * @param el 俯仰角（度）
 * @param azs 方位角速度（度/秒）
 * @param els 俯仰角速度（度/秒）
 * @param targetDistance 目标距离（毫米）
 * @param seq 序列号
 * @param control 控制字节
 * @param timeoutMs 超时时间（毫秒）
 * @return 响应数据字节数组，失败返回 NULL
 */
JNIEXPORT jbyteArray JNICALL Java_com_marble_cammon_CamMonNative_sendServoCommand(JNIEnv* env, jclass, jstring jhost, jint port, jfloat az, jfloat el, jfloat azs, jfloat els, jint targetDistance, jint seq, jint control, jint timeoutMs) {
    // 获取 Java 字符串的 UTF-8 编码
    const char* host = env->GetStringUTFChars(jhost, NULL);
    
    // 定义最大响应缓冲区大小
    const int RESP_MAX = 2048;
    // 使用智能指针管理响应缓冲区内存
    std::unique_ptr<uint8_t[]> resp(new uint8_t[RESP_MAX]);
    
    // 调用 C++ 底层 API 发送舵机命令
    // 默认 device_type = 0x01（舵机控制器），packet_type = 0x02（定点报告）
    int r = cammon_send_servo_command(host, port, az, el, azs, els, 
                                      (uint16_t)targetDistance, (uint8_t)seq, 
                                      (uint8_t)control, /*device_type*/0x01, 
                                      /*packet_type*/0x02, resp.get(), RESP_MAX, timeoutMs);
    
    // 释放 Java 字符串
    env->ReleaseStringUTFChars(jhost, host);
    
    // 检查返回结果
    if (r <= 0) return NULL;
    
    // 创建 Java 字节数组并复制响应数据
    jbyteArray out = env->NewByteArray(r);
    env->SetByteArrayRegion(out, 0, r, reinterpret_cast<jbyte*>(resp.get()));
    return out;
}

} // extern "C"