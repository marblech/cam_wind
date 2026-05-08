package com.marble.cammon;

/**
 * CamMonNative - 摄像头监控原生接口包装类
 * 
 * 此类提供了 Java 与底层 C++ 库之间的桥接接口。
 * 通过 JNI（Java Native Interface）加载原生共享库，
 * 并提供相机控制和舵机控制的原生方法调用。
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
                // 尝试从类路径中加载
                String path = CamMonNative.class.getResource("/cammon.dll").getPath();
                System.load(path);
                System.out.println("Successfully loaded native library: " + path);
            } catch (Exception ex) {
                System.out.println("Failed to load from class path: " + ex.getMessage());
                throw new RuntimeException("Failed to load native library cammon. Please build cam_mon_cpp first.");
            }
        }
    }

    /**
     * @brief 发送相机控制命令（原生接口）
     * 
     * 调用底层 C++ 库发送相机控制命令到指定设备。
     * 适用于可见光相机（地址 0x02）的控制操作。
     * 
     * @param host 目标主机 IP 地址（如 "192.168.1.100"）
     * @param port 目标端口号
     * @param func 功能码（如 0x01 查询状态、0x04 拍照等）
     * @param ctrl 控制码（如 0x01 启用、0x00 禁用等）
     * @param payload 数据载荷，可为 null（此时视为空载荷）
     * @param timeoutMs 超时时间（毫秒）
     * @return 响应数据字节数组，失败返回 null
     */
    public static native byte[] sendCameraCommand(String host, int port, byte func, byte ctrl, byte[] payload, int timeoutMs);

    /**
     * Send a generic protocol packet (specify address, func, ctrl).
     * Useful for sending commands to tracker (addr=0x00) or other devices.
     */
    public static native byte[] sendPacket(String host, int port, byte addr, byte func, byte ctrl, byte[] payload, int timeoutMs);

    /**
     * @brief 发送舵机控制命令（原生接口）
     * 
     * 调用底层 C++ 库发送舵机控制命令到指定设备。
     * 用于控制舵器运动，包括方位角、俯仰角及其速度控制。
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
     * @param packetType 数据包类型（默认 0x02 表示定点报告）
     * @param timeoutMs 超时时间（毫秒）
     * @return 响应数据字节数组，失败返回 null
     */
    public static native byte[] sendServoCommand(String host, int port, float az, float el, float azSpeed, float elSpeed, int targetDistance, int seq, int control, int deviceType, int packetType, int timeoutMs);

    /**
     * Start a background UDP listener for status reports on the given port.
     * @param port UDP port to bind
     * @return true on success
     */
    public static native boolean startStatusListener(int port);

    /**
     * Stop the background status listener.
     */
    public static native void stopStatusListener();

    /**
     * Get the last received status packet as a byte array, or null if none.
     */
    public static native byte[] getLastStatus();

    /**
     * Get last parsed PTZ values as a `PTZStatus` object (az, el, irFocus, visFocus).
     * Returns null if no status frame available.
     */
    public static native PTZStatus getPTZ();

    /**
     * Send PTZ command to camera (servo control). Returns number of response bytes (>0) or negative error code.
     */
    public static native int setPTZ(String host, int port, float az, float el, float azSpeed, float elSpeed, int targetDistance, int seq, int control, int deviceType, int packetType, int timeoutMs);
}