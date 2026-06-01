package com.marble.cammon;

/**
 * CamMonNative - 摄像头监控原生接口包装类
 * 
 * 此类提供了 Java 与底层 C++ 库之间的桥接接口。
 * 通过 JNI（Java Native Interface）加载原生共享库，
 * 所有接口均通过 CamController 封装层调用，不直接访问底层 cammon_api。
 * 
 * CamController 提供的接口：
 * - cam_controller_create/destroy: 生命周期管理
 * - cam_controller_start/stop: UDP 监听启停
 * - cam_controller_get_last: 获取最后接收的状态包
 * - cam_controller_get_ptz: 解析获取 PTZ 数据
 * - cam_controller_set_ptz: 发送 PTZ/舵机控制命令
 * 
 * 原生库加载顺序：
 * 1. 首先尝试从 java.library.path 加载
 * 2. 如果失败，尝试从类路径中加载 cammon.dll
 * 
 * @author marble
 * @version 1.0
 */
public class CamMonNative {
    
    static {
        // 加载从 C++ 构建的原生共享库
        try {
            // 首先尝试从 java.library.path 加载
            System.loadLibrary("cammon");
            System.out.println("Successfully loaded native library: cammon from library path");
        } catch (UnsatisfiedLinkError e) {
            System.out.println("Failed to load from library path: " + e.getMessage());
            try {
                // 尝试从类路径中加载：如果 cammon.dll 被打包到 resources/native/ 下，
                // 先将其解压到临时文件再调用 System.load
                NativeLoader.loadFromJar("/native/cammon.dll", "cammon");
                System.out.println("Successfully loaded native library from JAR resource: /native/cammon.dll");
            } catch (Exception ex) {
                System.out.println("Failed to load from class path: " + ex.getMessage());
                throw new RuntimeException("Failed to load native library cammon. Please build cam_mon_cpp first.");
            }
        }
    }

    /**
     * 启动后台 UDP 状态监听器
     * 
     * 对应 C++ 接口：cam_controller_create() + cam_controller_start()
     * 
     * @param port UDP 监听端口
     * @return true 如果启动成功，false 失败
     */
    // Start listener. If mcastGroup is non-null, join that multicast group;
    // otherwise operate in point-to-point UDP listening mode.
    public static native boolean startStatusListener(int port, String mcastGroup);

    // Convenience overload for callers that don't want to supply a multicast group.
    public static boolean startStatusListener(int port) {
        return startStatusListener(port, null);
    }

    /**
     * 停止后台状态监听器
     * 
     * 对应 C++ 接口：cam_controller_stop() + cam_controller_destroy()
     */
    public static native void stopStatusListener();

    /**
     * 获取最后接收的状态包原始数据
     * 
     * 对应 C++ 接口：cam_controller_get_last()
     * 
     * @return 状态包字节数组，如果无数据或控制器未启动则返回 null
     */
    public static native byte[] getLastStatus();

    /**
     * 获取最后解析的 PTZ 数据
     * 
     * 对应 C++ 接口：cam_controller_get_ptz()
     * 
     * @return PTZStatus 对象（包含 az, el, irFocus, visFocus），无数据时返回 null
     */
    public static native PTZStatus getPTZ();

    /**
     * 发送 PTZ/舵机控制命令
     * 
     * 对应 C++ 接口：cam_controller_set_ptz()
     * 
     * @param host 目标主机 IP 地址
     * @param port 目标端口号
     * @param az 方位角（度）
     * @param el 俯仰角（度）
     * @param azSpeed 方位角速度（度/秒）
     * @param elSpeed 俯仰角速度（度/秒）
     * @param targetDistance 目标距离（毫米）
     * @param seq 序列号
     * @param control 控制字节
     * @param deviceType 设备类型（默认 0x01 表示舵机控制器）
     * @param packetType 数据包类型（默认 0x20 表示定点报告）
     * @param timeoutMs 超时时间（毫秒）
     * @return 成功返回接收到的字节数（>0），失败返回负 error code
     */
    public static native int setPTZ(String host, int port, float az, float el, float azSpeed, float elSpeed, int targetDistance, int seq, int control, int deviceType, int packetType, int timeoutMs);
}