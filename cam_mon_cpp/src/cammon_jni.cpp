/**
 * @file cammon_jni.cpp
 * @brief Camera Monitor JNI 桥接层实现
 * 
 * 本文件实现了 Java 与 C++ 之间的 JNI（Java Native Interface）桥接功能。
 * 所有接口均通过 CamController 封装层调用，不直接访问底层 cammon_api。
 * 
 * CamController 提供的接口：
 * - cam_controller_create/destroy: 生命周期管理
 * - cam_controller_start/stop: UDP 监听启停
 * - cam_controller_get_last: 获取最后接收的状态包
 * - cam_controller_get_ptz: 解析获取 PTZ 数据
 * - cam_controller_set_ptz: 发送 PTZ/舵机控制命令
 */

#include <jni.h>
#include <string>
#include <memory>
#include <cstring>
#include "cam_controller.h"

// singleton controller instance managed by JNI
static CamController* g_controller = nullptr;

extern "C" {

/**
 * @brief JNI 接口：启动后台状态监听
 * 
 * 对应 Java 方法 CamMonNative.startStatusListener(int port)
 * 创建 CamController 并启动 UDP 监听线程
 * 
 * @param env JNI 环境变量指针
 * @param clazz 调用此方法的 Java 类
 * @param port UDP 监听端口
 * @return true 如果启动成功，false 失败
 */
JNIEXPORT jboolean JNICALL Java_com_marble_cammon_CamMonNative_startStatusListener(JNIEnv* env, jclass, jint port) {
    if (g_controller) return JNI_FALSE;
    g_controller = cam_controller_create();
    if (!g_controller) return JNI_FALSE;
    int r = cam_controller_start(g_controller, (int)port);
    if (r != 0) {
        cam_controller_destroy(g_controller);
        g_controller = nullptr;
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

/**
 * @brief JNI 接口：停止后台状态监听
 * 
 * 对应 Java 方法 CamMonNative.stopStatusListener()
 * 停止并销毁 CamController
 */
JNIEXPORT void JNICALL Java_com_marble_cammon_CamMonNative_stopStatusListener(JNIEnv* env, jclass) {
    if (!g_controller) return;
    cam_controller_stop(g_controller);
    cam_controller_destroy(g_controller);
    g_controller = nullptr;
}

/**
 * @brief JNI 接口：获取最后接收的状态包原始数据
 * 
 * 对应 Java 方法 CamMonNative.getLastStatus()
 * 通过 cam_controller_get_last 获取最后接收的 UDP 数据包
 * 
 * @return 响应数据字节数组，失败返回 NULL
 */
JNIEXPORT jbyteArray JNICALL Java_com_marble_cammon_CamMonNative_getLastStatus(JNIEnv* env, jclass) {
    if (!g_controller) return NULL;
    const int RESP_MAX = 2048;
    uint8_t buf[RESP_MAX];
    int n = cam_controller_get_last(g_controller, buf, RESP_MAX);
    if (n <= 0) return NULL;
    jbyteArray out = env->NewByteArray(n);
    env->SetByteArrayRegion(out, 0, n, reinterpret_cast<jbyte*>(buf));
    return out;
}

/**
 * @brief JNI 接口：获取解析后的 PTZ 数据
 * 
 * 对应 Java 方法 CamMonNative.getPTZ()
 * 通过 cam_controller_get_ptz 获取方位角、俯仰角、红外焦点、可见光焦点
 * 
 * @return PTZStatus 对象，失败返回 NULL
 */
JNIEXPORT jobject JNICALL Java_com_marble_cammon_CamMonNative_getPTZ(JNIEnv* env, jclass) {
    if (!g_controller) return NULL;
    float az=0, el=0, ir=0, vis=0;
    int ok = cam_controller_get_ptz(g_controller, &az, &el, &ir, &vis);
    if (!ok) return NULL;
    // Find PTZStatus class and construct object
    jclass cls = env->FindClass("com/marble/cammon/PTZStatus");
    if (!cls) return NULL;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(FFFF)V");
    if (!ctor) return NULL;
    jobject obj = env->NewObject(cls, ctor, (jfloat)az, (jfloat)el, (jfloat)ir, (jfloat)vis);
    return obj;
}

/**
 * @brief JNI 接口：发送 PTZ/舵机控制命令
 * 
 * 对应 Java 方法 CamMonNative.setPTZ(...)
 * 通过 cam_controller_set_ptz 发送舵机控制命令
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
 * @param deviceType 设备类型
 * @param packetType 数据包类型
 * @param timeoutMs 超时时间（毫秒）
 * @return 成功返回接收到的字节数（>0），失败返回负 error code
 */
JNIEXPORT jint JNICALL Java_com_marble_cammon_CamMonNative_setPTZ(JNIEnv* env, jclass, jstring jhost, jint port, jfloat az, jfloat el, jfloat azs, jfloat els, jint targetDistance, jint seq, jint control, jint deviceType, jint packetType, jint timeoutMs) {
    const char* host = env->GetStringUTFChars(jhost, NULL);
    const int RESP_MAX = 2048;
    std::unique_ptr<uint8_t[]> resp(new uint8_t[RESP_MAX]);
    int r = cam_controller_set_ptz(g_controller, host, (int)port, (float)az, (float)el, (float)azs, (float)els, (uint16_t)targetDistance, (uint8_t)seq, (uint8_t)control, (uint8_t)deviceType, (uint8_t)packetType, resp.get(), RESP_MAX, (int)timeoutMs);
    env->ReleaseStringUTFChars(jhost, host);
    return r;
}

} // extern "C"