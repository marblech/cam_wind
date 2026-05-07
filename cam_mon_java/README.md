# 海面光电监控设备 Java 客户端 (cam_mon_java)

## 项目简介

本项目提供了 Java 接口，通过 JNI（Java Native Interface）调用 C++ 共享库（cammon），实现作为客户端与海面光电监控设备通信的功能。Java 应用程序可以发送相机控制命令和舵机控制命令，并接收设备的响应数据。

## 系统架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        Java 应用程序                              │
│   (Client / CppClientTest / DebugClient)                         │
├─────────────────────────────────────────────────────────────────┤
│                    Java 层                                        │
│  ┌──────────────────┐                                            │
│  │  CamMonNative    │  native 方法声明类                         │
│  │  (JNI 接口定义)   │                                            │
│  ├──────────────────┤                                            │
│  │     Client       │  高级客户端封装类                           │
│  ├──────────────────┤                                            │
│  │ CppClientTest    │  测试类 - 通过 Client 调用                    │
│  ├──────────────────┤                                            │
│  │  DebugClient     │  调试客户端 - 直接调用 native 方法            │
│  └──────────────────┘                                            │
├─────────────────────────────────────────────────────────────────┤
│                      JNI 桥接层                                    │
│         Java ↔ Native 方法映射 + 数据类型转换                       │
├─────────────────────────────────────────────────────────────────┤
│                    C++ 共享库 (cammon)                            │
│  ┌──────────────────┐  ┌──────────────────┐                     │
│  │  cammon_jni.cpp  │  │  cammon_api.cpp  │                     │
│  │  (JNI 接口实现)   │  │  (控制命令 API)   │                     │
│  ├──────────────────┤  ├──────────────────┤                     │
│  │  protocol.cpp   │  │  protocol.h      │                     │
│  │  (协议实现)      │  │  (协议定义)       │                     │
│  └──────────────────┘  └──────────────────┘                     │
├─────────────────────────────────────────────────────────────────┤
│                   操作系统网络层                                   │
│              UDP Socket (Windows: Winsock)                       │
│              UDP Socket (Linux: POSIX socket)                    │
├─────────────────────────────────────────────────────────────────┤
│                    监控设备                                        │
│              (相机 + 舵机云台)                                      │
└─────────────────────────────────────────────────────────────────┘
```

## 项目结构

```
cam_mon_java/
├── pom.xml                          # Maven 构建配置
├── README.md                        # 本文档
├── src/
│   └── main/
│       └── java/
│           └── com/
│               └── marble/
│                   └── cammon/
│                       ├── CamMonNative.java    # JNI native 方法声明
│                       ├── Client.java          # 高级客户端封装
│                       ├── CppClientTest.java   # 集成测试类
│                       └── DebugClient.java     # 调试客户端
├── target/
│   └── cam_mon_java-1.0-SNAPSHOT.jar  # 编译后的 JAR 包
└── native/                            # JNI 库文件目录（需提前构建 C++ 库）
    ├── cammon.dll                     # Windows
    └── libcammon.so                   # Linux
```

## 工作原理

### 调用流程

1. **Java 应用程序** 调用 `Client` 类的高级方法（如 `sendCameraCommand()`）
2. **Client 类** 将参数传递给 `CamMonNative` 的 native 方法
3. **JNI 桥接层** 将 Java 参数转换为 C++ 参数类型
4. **cammon_jni.cpp** 调用 C++ API 函数
5. **cammon_api.cpp** 构建数据包并通过 UDP 发送
6. **监控设备** 接收命令并返回响应
7. **响应数据** 沿原路径返回到 Java 应用程序

### 数据流向

```
Java: client.sendCameraCommand("192.168.1.100", 5000, 0x01, 0x01, ...)
    ↓
Java: CamMonNative.sendCameraCommand(hostPtr, port, func, ctrl, ...)
    ↓
JNI:  Java_com_marble_cammon_CamMonNative_sendCameraCommand()
    ↓
C++: cammon_send_camera_command(host, port, func, ctrl, ...)
    ↓
C++: make_camera_command() → serialize() → UDP 发送
    ↓
设备: 接收 UDP 数据包
    ↓
设备: 处理命令并返回响应
    ↓
C++: 接收 UDP 响应
    ↓
JNI: 将响应数据复制回 Java byte[]
    ↓
Java: 返回 byte[] 给调用者
```

## 前置要求

### 构建 C++ 共享库

在构建 Java 项目之前，必须先构建 C++ 共享库：

#### Windows

```batch
cd cam_mon_cpp
.\build_andcompile.bat
```

构建完成后，将 `cam_mon_cpp/build/cammon.dll` 复制到 `cam_mon_java/native/cammon.dll`

#### Linux

```bash
cd cam_mon_cpp
mkdir build && cd build
cmake ..
make
```

构建完成后，将 `cam_mon_cpp/build/libcammon.so` 复制到 `cam_mon_java/native/libcammon.so`

### 环境要求

- JDK 8 或更高版本
- Apache Maven 3.6+
- 已构建的 C++ 共享库（cammon.dll 或 libcammon.so）
- 设置 JAVA_HOME 环境变量

## 构建说明

### 使用 Maven 构建

```bash
cd cam_mon_java
mvn package
```

### 跳过测试快速构建

```bash
mvn -DskipTests package
```

### 构建输出

构建成功后，JAR 文件位于：
```
cam_mon_java/target/cam_mon_java-1.0-SNAPSHOT.jar
```

## 使用方法

### 方法一：使用 Client 高级封装类

`Client` 类提供了简化的 API，封装了 native 调用的细节：

```java
import com.marble.cammon.Client;

public class MyApp {
    public static void main(String[] args) {
        // 创建客户端实例
        Client client = new Client("192.168.1.100", 5000);
        
        // 发送相机状态查询
        byte[] response = client.sendCameraCommand(0x01, 0x01, new byte[0]);
        if (response != null) {
            System.out.println("收到响应: " + response.length + " 字节");
        }
        
        // 发送拍照命令
        response = client.sendCameraCommand(0x04, 0x01, new byte[0]);
        if (response != null) {
            System.out.println("拍照命令已发送");
        }
        
        // 发送舵机控制命令
        boolean success = client.sendServoCommand(
            45.0f,   // 方位角
            30.0f,   // 俯仰角
            10.0f,   // 方位角速度
            10.0f,   // 俯仰角速度
            1000,    // 目标距离 (mm)
            1,       // 序列号
            0x11     // 控制字节
        );
        
        if (success) {
            System.out.println("舵机命令发送成功");
        }
    }
}
```

### 方法二：直接使用 CamMonNative

`CamMonNative` 类直接声明了 native 方法，适合需要更多控制的场景：

```java
import com.marble.cammon.CamMonNative;

public class DirectNativeAccess {
    static {
        // 加载 native 库
        System.loadLibrary("cammon");
    }
    
    public static void main(String[] args) {
        // 分配直接缓冲区用于接收响应
        int respBufferSize = 4096;
        java.nio.ByteBuffer respBuffer = java.nio.ByteBuffer.allocateDirect(respBufferSize);
        
        // 发送相机命令
        int result = CamMonNative.sendCameraCommand(
            "192.168.1.100",   // host
            5000,              // port
            0x01,              // func
            0x01,              // ctrl
            null,              // payload
            0,                 // payload_len
            respBuffer,        // response buffer
            respBufferSize,    // buffer_size
            1000               // timeout_ms
        );
        
        if (result > 0) {
            System.out.println("收到 " + result + " 字节响应");
        }
    }
}
```

## 新增 JNI 功能（监听器与 PTZ）

库已新增 native 监听器与 PTZ 读写接口，便于在 Java 层异步监听设备状态并读取语义化的 PTZ 数据：

- `CamMonNative.startStatusListener(int port)` — 在 native 层启动后台 UDP 监听器，监听端口并缓存最后一帧
- `CamMonNative.stopStatusListener()` — 停止监听器
- `CamMonNative.getLastStatus()` — 返回最近收到的原始状态帧（`byte[]`）
- `CamMonNative.getPTZ()` — 返回 `com.marble.cammon.PTZStatus` 对象（字段：`az, el, irFocus, visFocus`）
- `CamMonNative.setPTZ(String host, int port, float az, float el, float azs, float els, int distance, byte seq, byte control)` — 发送舵机控制命令并返回响应字节数（int）

示例（读取 PTZ 并发送控制）：

```java
// 加载 native 库
System.loadLibrary("cammon");

// 启动监听（native 层后台线程）
CamMonNative.startStatusListener(5001);

// 获取语义化 PTZ 对象
PTZStatus p = CamMonNative.getPTZ();
System.out.println("PTZ: az=" + p.az + " el=" + p.el);

// 发送 PTZ 指令到设备
int result = CamMonNative.setPTZ("127.0.0.1", 4001, 10.0f, 5.0f, 1.0f, 1.0f, 1000, (byte)1, (byte)0x11);
System.out.println("setPTZ result: " + result);

CamMonNative.stopStatusListener();
```

### 运行测试

```bash
# 运行 CppClientTest
java -Djava.library.path=native -cp target/cam_mon_java-1.0-SNAPSHOT.jar \
    com.marble.cammon.CppClientTest 192.168.1.100 5000

# 运行 DebugClient
java -Djava.library.path=native -cp target/cam_mon_java-1.0-SNAPSHOT.jar \
    com.marble.cammon.DebugClient 192.168.1.100 5000
```

集成测试（建议）

1. 先在 `cam_mon_cpp` 下构建 native 库：

```bash
cd cam_mon_cpp && mkdir -p build && cd build
cmake .. && make -j$(nproc)
```

2. 回到 Java 项目并运行 maven 测试（确保 `java.library.path` 指向 native 构建目录）：

```bash
cd ../../cam_mon_java
mvn test -Djava.library.path=../cam_mon_cpp/build
```

（项目包含 `PTZIntegrationTest`，它会启动 `ServerSimulator` 并验证 `getPTZ()`/`setPTZ()` 的端到端行为。）

## API 参考

### CamMonNative (JNI native 方法)

| 方法 | 说明 |
|------|------|
| `sendCameraCommand(host, port, func, ctrl, payload, payloadLen, respBuf, respBufLen, timeoutMs)` | 发送相机控制命令 |
| `sendServoCommand(host, port, az, el, azs, els, targetDistance, seq, control, deviceType, packetType, respBuf, respBufLen, timeoutMs)` | 发送舵机控制命令 |
| `setDebugMode(enabled)` | 启用/禁用调试模式 |

### Client (高级封装)

| 方法 | 参数 | 返回值 | 说明 |
|------|------|--------|------|
| `sendCameraCommand(func, ctrl, payload)` | func: 功能码<br>ctrl: 控制码<br>payload: 数据载荷 | byte[] 响应数据或 null | 发送相机命令 |
| `sendServoCommand(az, el, azs, els, targetDistance, seq, control)` | az: 方位角<br>el: 俯仰角<br>azs: 方位角速度<br>els: 俯仰角速度<br>targetDistance: 目标距离<br>seq: 序列号<br>control: 控制字节 | boolean 是否成功 | 发送舵机命令 |
| `getHost()` | 无 | String | 获取设备 IP |
| `getPort()` | 无 | int | 获取设备端口 |

## 命令行参数

### CppClientTest

```bash
java -Djava.library.path=native -cp target/cam_mon_java-1.0-SNAPSHOT.jar \
    com.marble.cammon.CppClientTest [host] [port]
```

| 参数 | 说明 | 默认值 |
|------|------|--------|
| host | 设备 IP 地址 | 192.168.1.100 |
| port | 设备端口号 | 5000 |

### DebugClient

```bash
java -Djava.library.path=native -cp target/cam_mon_java-1.0-SNAPSHOT.jar \
    com.marble.cammon.DebugClient [host] [port]
```

DebugClient 执行两个测试：
1. **测试一**：直接调用 native 方法发送相机命令
2. **测试二**：通过 Client 类发送相机命令

## 数据类型映射

### Java → C++ 类型映射

| Java 类型 | C++ 类型 | 说明 |
|-----------|----------|------|
| String | const char* | 通过 JNIEnv->GetStringUTFChars() |
| int | int | 直接传递 |
| float | float | 直接传递 |
| byte[] | uint8_t* | 通过 JNIEnv->GetDirectBufferAddress() |
| boolean | bool | 直接传递 |

### C++ → Java 类型映射

| C++ 类型 | Java 类型 | 说明 |
|----------|-----------|------|
| int | int | 直接返回 |
| 失败 (-1 等) | -1 等 | 错误码 |

## 错误处理

### Java 层错误处理

```java
try {
    byte[] response = client.sendCameraCommand(0x01, 0x01, new byte[0]);
    if (response == null) {
        System.err.println("命令发送失败或无响应");
    } else {
        // 处理响应
    }
} catch (Exception e) {
    System.err.println("发生异常: " + e.getMessage());
}
```

### 常见错误

| 错误 | 原因 | 解决方法 |
|------|------|----------|
| UnsatisfiedLinkError | 未加载 native 库 | 确保调用 native 方法前执行 `System.loadLibrary("cammon")` |
| FileNotFoundException | DLL/SO 文件不存在 | 确保将 cammon.dll/libcammon.so 放在 native/ 目录或通过 -Djava.library.path 指定 |
| NullPointerException | 空参数传递 | 确保参数不为 null（respBuf 除外） |
| 超时 | 设备未响应 | 检查 IP、端口、网络连接 |

## 调试技巧

### 启用调试输出

1. **C++ 层调试输出**：默认输出到 stderr，可通过重定向 stderr 捕获
2. **Java 层调试**：Client 类内置日志输出

### 使用 DebugClient

DebugClient 提供了完整的测试流程，包括直接 native 调用和 Client 封装调用，适合验证 JNI 桥接是否正常工作：

```bash
java -Djava.library.path=native -cp target/cam_mon_java-1.0-SNAPSHOT.jar \
    com.marble.cammon.DebugClient 192.168.1.100 5000
```

## 注意事项

1. **Native 库加载**：必须在调用 native 方法前加载库，推荐使用静态初始化块
2. **缓冲区管理**：响应缓冲区使用直接缓冲区（ByteBuffer.allocateDirect）以获得更好的性能
3. **字节序**：所有多字节字段使用小端序（Little-Endian）
4. **超时设置**：建议超时时间不低于 500ms，网络较差时可增加
5. **线程安全**：Client 实例不是线程安全的，多线程环境下请创建多个实例
6. **资源释放**：应用退出时应调用 Client.close() 释放资源

## 常见问题

### Q: 运行时出现 "Could not find DLL" 错误

A: 确保将 cammon.dll 放在以下位置之一：
- 项目的 native/ 目录
- 系统 PATH 环境变量包含的目录
- 使用 -Djava.library.path 指定路径

### Q: 如何关闭调试输出？

A: C++ 层的调试输出固定输出到 stderr，无法通过参数关闭。可以通过重定向 stderr 来隐藏：
```bash
java -Djava.library.path=native -cp target/cam_mon_java-1.0-SNAPSHOT.jar \
    com.marble.cammon.CppClientTest 192.168.1.100 5000 2>nul
```

### Q: 如何修改默认 IP 和端口？

A: 修改 CppClientTest.java 和 DebugClient.java 中的 DEFAULT_HOST 和 DEFAULT_PORT 常量。

## 相关项目

- **cam_mon_cpp**: C++ 共享库源码，必须先构建此项目