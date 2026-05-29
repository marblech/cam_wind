# cam_mon_java

`cam_mon_java` 是当前项目的 Java 层，使用 JNI 调用 `cam_mon_cpp` 生成的 `cammon` 共享库。

如果 Java 和 C++ 的报文定义存在冲突，以 C++ 的实现为准。

## 当前结构

- `CamMonNative`：JNI 声明类，负责加载 `libcammon.so`
- `Client`：演示 `setPTZ(...)` 的命令行客户端
- `ServerSimulator`：Java UDP 模拟器，用于回包测试
- `PTZStatus`：`getPTZ()` 返回对象
- `Protocol`：Java 侧协议辅助类，与 C++ 保持一致
- `DebugClient`：状态监听和 PTZ 控制的调试入口

## 当前 JNI 接口

`CamMonNative` 提供的 native 方法如下：

- `startStatusListener(int port)`
- `stopStatusListener()`
- `getLastStatus()`
- `getPTZ()`
- `setPTZ(String host, int port, float az, float el, float azSpeed, float elSpeed, int targetDistance, int seq, int control, int deviceType, int packetType, int timeoutMs)`

说明：

- `setPTZ(...)` 是直接发 UDP 舵机命令并等待响应的接口，不依赖 `startStatusListener()`
- `getPTZ()` 读取的是 `startStatusListener()` 缓存的最后一帧状态

## 协议说明

Java 端 `Protocol` 仅作为辅助实现，具体报文规则与 C++ 保持一致：

- 标准帧：`0x0F 0xF0 ... 0xF0 0x0F`
- 舵机帧：`0x7E` 开头，固定 72 字节，小端序编码
- 状态帧：`0x1F 0xF1 ... 0xF1 0x1F`

## 构建前提

先确保 C++ 侧已经生成 native 库：

```bash
cd cam_mon_cpp
cmake -S . -B build
cmake --build build --target cammon -j"$(nproc)"
```

然后在 Java 项目中运行：

```bash
cd cam_mon_java
mvn test -Djava.library.path=../cam_mon_cpp/build
```

当前仓库中这条测试命令已验证通过。

## 常用运行方式

### 1) 运行 PTZ 客户端

```bash
cd cam_mon_java
java -Djava.library.path=../cam_mon_cpp/build -cp target/classes com.marble.cammon.Client 127.0.0.1 4001
```

### 2) 运行状态监听演示

```bash
cd cam_mon_java
java -Djava.library.path=../cam_mon_cpp/build -cp target/classes com.marble.cammon.Client listen 5001
```

### 3) 运行 Java 模拟器

```bash
cd cam_mon_java
java -cp target/classes com.marble.cammon.ServerSimulator 4001
```

## 回归测试

仓库内当前会跑这两个集成测试：

- `ProtocolIntegrationTest`
- `PTZIntegrationTest`

它们依赖 `ServerSimulator` 和 `libcammon.so`，用于验证 Java ↔ C++ ↔ UDP 的端到端链路。

## 关键文件

- `src/main/java/com/marble/cammon/CamMonNative.java`
- `src/main/java/com/marble/cammon/Client.java`
- `src/main/java/com/marble/cammon/Protocol.java`
- `src/main/java/com/marble/cammon/PTZStatus.java`
- `src/main/java/com/marble/cammon/ServerSimulator.java`


## 相关项目
cam_mon_cpp 此为调用的底层库