# cammon

`cammon` 是当前项目的 C++ 协议与 JNI 实现，负责把 Java 侧请求转换成设备可识别的 UDP 报文，并把接收的状态帧解析成 PTZ 数据。

当 Java 与 C++ 的报文定义存在冲突时，以本仓库的 C++ 实现为准。

## 功能

- **标准相机报文**：`0x0F 0xF0` 帧头，`0xF0 0x0F` 帧尾，默认数据区长度 15 字节
- **舵机报文**：`0x7E` 开头，固定 72 字节，按小端序写入浮点和整型字段
- **状态监听**：`CamController` 在后台线程监听 UDP 状态帧并缓存最后一帧
- **JNI 导出**：供 `cam_mon_java` 的 `CamMonNative` 调用

## 主要接口

### 协议层

- `cammon::Packet`：标准相机报文序列化/反序列化
- `cammon::ServoPacket`：舵机报文序列化/反序列化
- `cammon::build_servo_packet(...)`：构建舵机控制包
- `cammon::make_camera_command(...)`：构建标准相机控制包

### 传输层

- `cammon_send_udp_and_recv(...)`：发送 UDP 并等待响应
- `cammon_send_camera_command(...)`：发送标准相机命令
- `cammon_send_servo_command(...)`：发送舵机命令
- `cammon_send_packet(...)`：发送自定义标准报文

### JNI 层

- `CamMonNative.startStatusListener(int port)`
- `CamMonNative.stopStatusListener()`
- `CamMonNative.getLastStatus()`
- `CamMonNative.getPTZ()`
- `CamMonNative.setPTZ(...)`

## 协议约定

- 标准帧：`[0x0F 0xF0][addr][func][ctrl][data...][checksum][0xF0 0x0F]`
- 校验和：对 `addr + func + ctrl + data...` 做累加和，结果取低 8 位
- 舵机帧：固定 72 字节，字段按 `protocol.h` 中定义的小端序布局写入
- 状态帧：`0x1F 0xF1` 开头，PTZ 相关字段由 `CamController` 从固定偏移解析

## 构建

在 Linux 上使用 CMake：

```bash
cd cam_mon_cpp
cmake -S . -B build
cmake --build build --target cammon -j"$(nproc)"
```

构建产物位于 `cam_mon_cpp/build/libcammon.so`。

## 与 Java 联动

Java 项目运行时需要能找到 native 库：

```bash
cd cam_mon_java
mvn test
```

如果需要显式指定库路径，可以加上：

```bash
mvn test -Djava.library.path=../cam_mon_cpp/build
```

## 验证

当前仓库中已验证通过的回归：

- `cammon` 目标构建成功
- `cam_mon_java` 的 `mvn test` 通过

## 参考文件

- `src/protocol.h`
- `src/protocol.cpp`
- `src/cam_controller.cpp`
- `src/cammon_api.cpp`
- `src/cammon_jni.cpp`

````markdown
# cammon — C++ 相机/云台控制库（带 JNI 暴露）

该库封装了对海面光电监控设备的 UDP 协议实现，并提供两套对外接口：

- 直接的 C 风格 API（适合 C/C++ 调用）
- 通过 JNI 暴露给 Java 的 API（当前项目: `cam_mon_java` 使用）

本文件已更新为反映最新重构：新增 `CamController`（封装监听器与状态缓存）、PTZ 读写接口，以及相应的 Java `PTZStatus` 返回类型和集成测试说明。

## 主要改动摘要

- 新增 `CamController` C API（位于 `src/cam_controller.*`）：
  - `cam_controller_create()` / `cam_controller_destroy()`
  - `cam_controller_start_ex(listen_port, mcast_group)` / `cam_controller_start(listen_port)` / `cam_controller_stop()` — 在后台线程监听 UDP 状态帧并缓存最后一帧；
    `cam_controller_start_ex` 接受可选的 `mcast_group` 字符串，如果非空则尝试加入该组播组并在指定端口监听；传 `NULL` 或空字符串表示点对点（直接 UDP）模式。
  - `cam_controller_get_last(uint8_t* buf, int buf_len)` — 读取最后收到的原始帧
  - `cam_controller_get_ptz(float* az, float* el, float* ir, float* vis)` — 从缓存帧解析 PTZ 值（小端 float）
  - `cam_controller_set_ptz(host, port, az, el, azs, els, distance, seq, control)` — 发送舵机（servo）控制包

- JNI 层（`cammon_jni.cpp`）已新增/更新方法，供 Java 调用：
  - `startStatusListener(int port, String mcastGroup)` / `stopStatusListener()`
    - Java 侧提供了两种调用方式：`startStatusListener(port)`（点对点） 或 `startStatusListener(port, "239.255.43.21")`（组播）。
  - `getLastStatus()` → 返回 `byte[]`（最近一帧原始数据）
  - `getPTZ()` → 返回 Java 对象 `com.marble.cammon.PTZStatus`（字段：`az, el, irFocus, visFocus`）
  - `setPTZ(...)` → 发送舵机命令并返回发送/接收结果（int）

## 快速使用与构建（Linux）

1) 构建 native 库：

```bash
cd cam_mon_cpp
mkdir -p build && cd build
cmake ..
make -j$(nproc)
# 构建完成后共享库位于 cam_mon_cpp/build/libcammon.so
```

2) 在 Java 项目中运行集成测试（建议方式：先构建 native，再运行 Maven 测试）：

```bash
# 从 cam_mon_cpp 目录构建 libcammon.so
cd cam_mon_cpp/build
# 回到 java 项目并运行 maven 测试，确保 JVM 能找到 native 库
cd ../../cam_mon_java
# 使用 -Djava.library.path 指定 native 库位置（相对或绝对均可）
mvn test -Djava.library.path=../cam_mon_cpp/build
```

（注意：`pom.xml` 中已包含对 surefire 的 argLine 设置以便 forked JVM 能找到 `libcammon.so`，但手动运行时可显式传 `-Djava.library.path` 以保证正确加载。）

## JNI / Java 使用示例

假设 Java 端通过 `com.marble.cammon.CamMonNative` 调用：

```java
// 加载库（也可以通过 System.loadLibrary 在其他类中完成）
System.loadLibrary("cammon");

// 启动本地监听器（本地线程在 native 层运行并缓存最后一帧）
CamMonNative.startStatusListener(5001);

// 从 native 获取 PTZ 状态（返回 PTZStatus 对象）
PTZStatus s = CamMonNative.getPTZ();
System.out.println("az=" + s.az + " el=" + s.el + " ir=" + s.irFocus + " vis=" + s.visFocus);

// 发送舵机控制命令到设备
int resp = CamMonNative.setPTZ("192.168.1.100", 4001, 45.0f, 10.0f, 5.0f, 5.0f, 1000, (byte)1, (byte)0x11);
System.out.println("setPTZ result bytes: " + resp);

// 停止监听器
CamMonNative.stopStatusListener();
```

Java 端中 `PTZStatus` 为简单 POJO：

```java
package com.marble.cammon;
public final class PTZStatus {
    public final float az;
    public final float el;
    public final float irFocus;
    public final float visFocus;

    public PTZStatus(float az, float el, float irFocus, float visFocus) {
        this.az = az;
        this.el = el;
        this.irFocus = irFocus;
        this.visFocus = visFocus;
    }
}
```

## 调试与测试提示

- 在运行 Maven 测试前务必先构建 `libcammon.so` 并确保 `-Djava.library.path` 指向 `cam_mon_cpp/build`（或将库复制到 Java 项目 `native/` 目录）。
- `PTZIntegrationTest`（测试文件位于 `cam_mon_java`）会启动 Java `ServerSimulator`（作为 UDP 服务器）并使用本地库作为客户端进行端到端验证。
- 若出现 `UnsatisfiedLinkError`，请检查 JVM 的 `java.library.path` 与 `libcammon.so` 路径是否一致，并确认库名为 `libcammon.so`（Linux）或 `cammon.dll`（Windows）。

## 兼容与注意事项

- 所有多字节字段使用小端序（Little-Endian），PTZ 值按浮点（IEEE-754）编码在状态帧特定偏移处。
- `CamController` 在内部使用互斥保护对最后一帧缓存的访问，提供线程安全的读取接口。

---

如果需要，我可以继续把 `README` 中的示例代码提取到 `examples/` 目录，或把 Java 的测试启动脚本加入 Makefile / maven profile 以便一键运行集成测试。
````