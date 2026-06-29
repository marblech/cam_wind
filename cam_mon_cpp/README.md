# cammon

`cammon` 是当前项目的 C++ 协议实现，负责把 Java 侧请求转换成设备可识别的 UDP 报文，并把接收的状态帧解析成 PTZ 数据。

如果 Java 与 C++ 的报文定义存在冲突时，以本仓库的 C++ 实现为准。

## 功能

- **标准相机报文**：`0x0F 0xF0` 帧头，`0xF0 0x0F` 帧尾，默认数据区长度 15 字节
- **舵机报文**：`0x7E` 开头，固定 72 字节，按小端序写入浮点和整型字段
- **状态监听**：`CamController` 在后台线程监听 UDP 状态帧并缓存最后一帧
- **JNA 暴露**：通过 C 风格 API 供 `cam_mon_java` 的 JNA 调用

## JNA 迁移说明

本项目已从 JNI 迁移到 JNA（Java Native Access）：

| 特性 | 之前（JNI） | 现在（JNA） |
|------|-------------|-------------|
| 桥接代码 | 需要 `cammon_jni.cpp` | 无需桥接代码 |
| CMake 配置 | 需要 JNI 头文件和 `JNI_FOUND` 检查 | 只需导出 C API |
| Java 侧 | 需要 `System.loadLibrary()` | JNA 自动加载 |

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

### CamController 封装层

- `cam_controller_create()` - 创建控制器实例
- `cam_controller_destroy(CamController*)` - 销毁控制器实例
- `cam_controller_start_ex(CamController*, int port, const char* mcast_group)` - 启动 UDP 监听
- `cam_controller_stop(CamController*)` - 停止监听
- `cam_controller_get_last(CamController*, uint8_t*, int)` - 获取最后接收的状态包
- `cam_controller_get_ptz(CamController*, float*, float*, float*, float*)` - 解析 PTZ 数据
- `cam_controller_set_ptz(...)` - 发送舵机控制命令

## 协议约定

- 标准帧：`[0x0F 0xF0][addr][func][ctrl][data...][checksum][0xF0 0x0F]`
- 校验和：对 `addr + func + ctrl + data...` 做累加和，结果取低 8 位
- 舵机帧：固定 72 字节，字段按 `protocol.h` 中定义的小端序布局写入
- 状态帧：`0x1F 0xF1` 开头，PTZ 相关字段由 `CamController` 从固定偏移解析

## 构建

### Linux

```bash
cd cam_mon_cpp
cmake -S . -B build
cmake --build build --config Release
```

构建产物位于 `cam_mon_cpp/build/jna/`（Linux: `libcammon.so`，Windows: `cammon.dll`）。

### Windows (Visual Studio)

```bat
cd cam_mon_cpp
cmake -S . -B build -G "Visual Studio 16 2019"
cmake --build build --config Release
```

## 与 Java 联动

Java 项目通过 JNA 调用 `cammon.dll` / `libcammon.so`：

```bash
cd cam_mon_java
mvn clean package -Pwindows
```

JNA 会自动从 `java.library.path` 或类路径中查找 `cammon` 库。

## 关键文件

- `src/protocol.h`
- `src/protocol.cpp`
- `src/cam_controller.cpp`
- `src/cammon_api.cpp`
- `src/cam_ajf_lib.h`

## 参考

- [cam_mon_java](../cam_mon_java/) - 使用 JNA 调用本库的 Java 项目