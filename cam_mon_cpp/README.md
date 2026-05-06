# 海面光电监控设备 C++ 接口库 (cammon)

## 项目简介

本项目实现了海面光电监控设备的 UDP 通信协议，提供了 C++ 接口用于发送相机控制命令和舵机控制命令。该库可通过 JNI（Java Native Interface）被 Java 应用程序调用，作为客户端与监控设备通信。

## 协议说明

本项目基于海面光电监控设备的 UDP 通信协议文档实现，主要支持以下两类通信：

1. **相机控制** - 通过可见光相机命令数据包与相机通信
2. **舵机控制** - 通过舵机控制数据包与云台舵机通信

## 项目结构

```
cam_mon_cpp/
├── CMakeLists.txt          # CMake 构建配置
├── README.md               # 项目说明文档
├── build_andcompile.bat    # Windows 批量编译脚本
├── build/                  # 构建输出目录（生成 .dll/.so 库）
│   └── cammon.dll / cammon.so  # 共享库文件
└── src/
    ├── protocol.h          # 协议定义、数据包结构体、函数声明
    ├── protocol.cpp        # 数据包序列化/反序列化实现
    ├── cammon_api.cpp      # 高层设备控制接口（相机/舵机命令）
    └── cammon_jni.cpp      # JNI 接口实现（供 Java 调用）
```

## 核心模块说明

### protocol.h / protocol.cpp - 协议层

定义数据包结构和序列化/反序列化方法：

- **Packet** - 标准相机数据包结构（帧头 0x0F 0xF0）
- **ServoPacket** - 舵机控制数据包结构（帧头 0x7E）
- **make_camera_command()** - 构建相机控制命令
- **build_servo_packet()** - 构建舵机控制数据包
- **deserialize_camera_response()** - 解析相机响应
- **deserialize_servo_response()** - 解析舵机响应

### cammon_api.cpp - API 层

提供高层设备控制接口：

- **cammon_send_udp_and_recv()** - 底层 UDP 通信（跨平台）
- **cammon_send_camera_command()** - 发送相机控制命令
- **cammon_send_servo_command()** - 发送舵机控制命令

### cammon_jni.cpp - JNI 接口层

提供 Java 可调用的 JNI 函数：

- **JNI_OnLoad()** - 库加载时初始化
- **JNI_OnUnload()** - 库卸载时清理
- **Java_com_marble_cammon_CamMonNative_*()** - 映射到 Java 本地方法

## 构建说明

### 环境要求

- CMake >= 3.15
- C++17 兼容的编译器
  - Windows: MSVC 或 MinGW
  - Linux: GCC 5+ 或 Clang 3.4+
- Java Development Kit (JDK) 8+（需要设置 JAVA_HOME 环境变量）

### Windows 构建

```batch
cd cam_mon_cpp
.\build_andcompile.bat
```

或手动构建：

```batch
cd cam_mon_cpp
mkdir build && cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

### Linux 构建

```bash
cd cam_mon_cpp
mkdir build && cd build
cmake ..
make
```

### 构建输出

构建完成后，共享库文件将位于：

- **Windows**: `cam_mon_cpp/build/cammon.dll`
- **Linux**: `cam_mon_cpp/build/libcammon.so`

## 导出的函数

### C 风格 API 函数（可直接从 C/C++ 调用）

```cpp
// 发送 UDP 数据包并接收响应
int cammon_send_udp_and_recv(const char* host, int port,
    const uint8_t* outbuf, int outlen,
    uint8_t* inbuf, int inbuf_len,
    int timeout_ms);

// 发送相机控制命令
int cammon_send_camera_command(const char* host, int port,
    uint8_t func, uint8_t ctrl,
    const uint8_t* payload, int payload_len,
    uint8_t* resp_buf, int resp_buf_len,
    int timeout_ms);

// 发送舵机控制命令
int cammon_send_servo_command(const char* host, int port,
    float az, float el, float azs, float els,
    uint16_t target_distance, uint8_t seq,
    uint8_t control, uint8_t device_type,
    uint8_t packet_type,
    uint8_t* resp_buf, int resp_buf_len,
    int timeout_ms);
```

### JNI 函数（从 Java 调用）

JNI 函数通过 Java 的 `native` 方法间接调用，详见 Java 部分说明。

## 使用示例（C++）

```cpp
#include "cammon_api.cpp"
#include <cstdint>
#include <cstdio>

int main() {
    // 发送相机状态查询命令
    uint8_t resp[2048];
    int n = cammon_send_camera_command(
        "192.168.1.100",   // 设备 IP
        5000,               // 设备端口
        0x01,               // 功能码：查询状态
        0x01,               // 控制码：启用
        nullptr,            // 无额外数据
        0,
        resp,               // 响应缓冲区
        sizeof(resp),
        1000                // 超时 1000ms
    );
    
    if (n > 0) {
        printf("收到响应: %d 字节\n", n);
        // 处理响应数据...
    } else {
        fprintf(stderr, "发送失败，错误码: %d\n", n);
    }
    return 0;
}
```

## 错误码

| 错误码 | 说明 |
|--------|------|
| >= 0 | 成功接收的字节数 |
| -1 | 创建 socket 失败 |
| -2 | IP 地址转换失败 |
| -3 | 发送数据失败 |
| <= -10 | Windows WSAGetLastError() 或 Linux errno |

## 注意事项

1. **跨平台兼容**: 库自动检测 Windows/Linux 平台并使用相应的 socket API
2. **Winsock 初始化**: Windows 平台上首次调用时自动初始化 Winsock 2.2
3. **线程安全**: 库函数可以在多线程中安全调用
4. **调试输出**: 默认向 stderr 输出调试信息，可通过重定向 stderr 关闭

## 依赖

- **Windows**: WS2_32.lib (Winsock 2)
- **Linux**: pthread, libc

## 许可证

本项目仅供内部使用。