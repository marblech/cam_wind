package com.marble.cammon;

/**
 * CamMonNative - 摄像头监控原生接口包装类
 * 
 * 使用 JNA（Java Native Access）桥接 Java 与底层 C++ 库。
 * 所有接口均通过 CamController 封装层调用，不直接访问底层 cammon_api。
 * 
 * JNA 优势（相比 JNI）:
 * - 无需编写桥接 C++ 代码（cammon_jni.cpp）
 * - 无需 JNI 头文件和复杂的类型映射
 * - 纯 Java 实现，易于维护和调试
 * 
 * CamController 提供的接口:
 * - cam_controller_create/destroy: 生命周期管理
 * - cam_controller_start/stop: UDP 监听启停
 * - cam_controller_get_last: 获取最后接收的状态包
 * - cam_controller_get_ptz: 解析获取 PTZ 数据
 * - cam_controller_set_ptz: 发送 PTZ/舵机控制命令
 * 
 * @author marble
 * @version 2.0 (JNA migration)
 */
public class CamMonNative {
    
    /**
     * 控制器句柄，使用 JNA Pointer 类型
     * 由 cam_controller_create() 创建，由 cam_controller_destroy() 释放
     */
    private static com.sun.jna.Pointer g_controller = null;
    
    /**
     * 静态初始化块
     * JNA 会在首次调用 CamMonLibrary.INSTANCE 时自动加载 cammon.dll
     * 无需手动调用 System.loadLibrary() 或 System.load()
     */
    static {
        System.out.println("JNA: CamMonNative class loaded");
        System.out.println("JNA library loading is handled by CamMonLibrary.INSTANCE");
    }
    
    // =========================================================================
    // 控制器生命周期管理
    // =========================================================================
    
    /**
     * 启动后台 UDP 状态监听器
     * 
     * 实现流程:
     * 1. 调用 cam_controller_create() 创建控制器实例
     * 2. 调用 cam_controller_start_ex() 启动 UDP 监听
     * 
     * @param port UDP 监听端口
     * @param mcastGroup 组播组地址（null 或空字符串表示单播模式）
     * @return true 如果启动成功，false 失败
     */
    public static boolean startStatusListener(int port, String mcastGroup) {
        if (g_controller != null) {
            System.out.println("JNA: Controller already started");
            return false;
        }
        
        // 创建控制器实例
        g_controller = CamMonLibrary.INSTANCE.cam_controller_create();
        if (g_controller == null) {
            System.err.println("JNA: Failed to create controller");
            return false;
        }
        
        // 启动监听
        int result = CamMonLibrary.INSTANCE.cam_controller_start_ex(
            g_controller, port, (mcastGroup != null && !mcastGroup.isEmpty()) ? mcastGroup : null);
        
        if (result != 0) {
            System.err.println("JNA: Failed to start listener, error code: " + result);
            CamMonLibrary.INSTANCE.cam_controller_destroy(g_controller);
            g_controller = null;
            return false;
        }
        
        System.out.println("JNA: Status listener started on port " + port + 
            (mcastGroup != null && !mcastGroup.isEmpty() ? " (mcast: " + mcastGroup + ")" : " (unicast)"));
        return true;
    }
    
    /**
     * 重载方法，默认不加入组播组
     */
    public static boolean startStatusListener(int port) {
        return startStatusListener(port, null);
    }
    
    /**
     * 停止后台状态监听器
     * 
     * 实现流程:
     * 1. 调用 cam_controller_stop() 停止监听线程
     * 2. 调用 cam_controller_destroy() 释放控制器资源
     */
    public static void stopStatusListener() {
        if (g_controller == null) {
            System.out.println("JNA: Controller is null, nothing to stop");
            return;
        }
        
        System.out.println("JNA: Stopping status listener...");
        CamMonLibrary.INSTANCE.cam_controller_stop(g_controller);
        CamMonLibrary.INSTANCE.cam_controller_destroy(g_controller);
        g_controller = null;
        
        System.out.println("JNA: Status listener stopped");
    }
    
    // =========================================================================
    // 数据获取接口
    // =========================================================================
    
    /**
     * 获取最后接收的状态包原始数据
     * 
     * 对应 C++ 接口：cam_controller_get_last()
     * 
     * @return 状态包字节数组，如果无数据或控制器未启动则返回 null
     */
    public static byte[] getLastStatus() {
        if (g_controller == null) {
            return null;
        }
        
        byte[] buf = new byte[2048];
        int n = CamMonLibrary.INSTANCE.cam_controller_get_last(g_controller, buf, buf.length);
        
        if (n <= 0) {
            return null;
        }
        
        byte[] result = new byte[n];
        System.arraycopy(buf, 0, result, 0, n);
        return result;
    }
    
    /**
     * 获取最后解析的 PTZ 数据
     * 
     * 对应 C++ 接口：cam_controller_get_ptz()
     * 
     * @return PTZStatus 对象（包含 az, el, irFocus, visFocus），无数据时返回 null
     */
    public static PTZStatus getPTZ() {
        if (g_controller == null) {
            return null;
        }
        
        float[] az = new float[1];
        float[] el = new float[1];
        float[] irFocus = new float[1];
        float[] visFocus = new float[1];
        
        int ok = CamMonLibrary.INSTANCE.cam_controller_get_ptz(
            g_controller, az, el, irFocus, visFocus);
        
        if (ok != 1) {
            return null;
        }
        
        return new PTZStatus(az[0], el[0], irFocus[0], visFocus[0]);
    }
    
    // =========================================================================
    // 控制命令接口
    // =========================================================================
    
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
    public static int setPTZ(String host, int port, float az, float el, 
                              float azSpeed, float elSpeed, int targetDistance, 
                              int seq, int control, int deviceType, 
                              int packetType, int timeoutMs) {
        if (g_controller == null) {
            return -1;
        }
        
        byte[] resp = new byte[2048];
        int result = CamMonLibrary.INSTANCE.cam_controller_set_ptz(
            g_controller,
            host, port,
            az, el, azSpeed, elSpeed,
            (short) targetDistance,
            (byte) seq, (byte) control,
            (byte) deviceType, (byte) packetType,
            resp, resp.length, timeoutMs);
        
        return result;
    }
    
    // =========================================================================
    // 便捷重载方法
    // =========================================================================
    
    /**
     * 发送 PTZ 控制命令（使用默认参数）
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
     * @param timeoutMs 超时时间（毫秒）
     * @return 成功返回接收到的字节数（>0），失败返回负 error code
     */
    public static int setPTZ(String host, int port, float az, float el, 
                              float azSpeed, float elSpeed, int targetDistance, 
                              int seq, int control, int timeoutMs) {
        return setPTZ(host, port, az, el, azSpeed, elSpeed, targetDistance, 
                      seq, control, 0x01, 0x20, timeoutMs);
    }
    
    /**
     * 发送 PTZ 控制命令（简化版，使用默认超时）
     * 
     * @param host 目标主机 IP 地址
     * @param port 目标端口号
     * @param az 方位角（度）
     * @param el 俯仰角（度）
     * @param azSpeed 方位角速度（度/秒）
     * @param elSpeed 俯仰角速度（度/秒）
     * @return 成功返回接收到的字节数（>0），失败返回负 error code
     */
    public static int setPTZ(String host, int port, float az, float el, 
                              float azSpeed, float elSpeed) {
        return setPTZ(host, port, az, el, azSpeed, elSpeed, 0, 0, 0, 1000);
    }
}