# cam_mon_java

`cam_mon_java` 是当前项目的 Java 层，使用 **JNA（Java Native Access）** 调用 `cam_mon_cpp` 生成的 `cammon.dll` / `libcammon.so` 动态库。

如果 Java 和 C++ 的报文定义存在冲突，以 C++ 的实现为准。

## JNA 迁移说明

本项目已从 JNI 迁移到 JNA（Java Native Access），主要变化：

### JNI vs JNA 对比

| 特性 | JNI | JNA |
|------|-----|-----|
| 桥接代码 | 需要编写 `cammon_jni.cpp` | 无需桥接代码 |
| 类型映射 | 需要手动映射 JNI 类型 | 自动映射 Java/C 类型 |
| 编译依赖 | 需要 JNI 头文件和 JNI 配置 | 只需加载 DLL/SO |
| 维护成本 | 高（C++ + Java 两侧代码） | 低（纯 Java 接口定义） |
| 性能 | 略高（直接调用） | 略低（中间层） |

### 架构变化

**之前（JNI）：**
```
Java → JNI 桥接代码 (cammon_jni.cpp) → cammon.dll
```

**现在（JNA）：**
```
Java → JNA 接口 (CamMonLibrary.java) → cammon.dll
```

## 当前结构

- `CamMonLibrary`：JNA 接口定义类，映射 C++ 库中的所有公开函数
- `CamMonNative`：JNA 包装类，提供面向对象的 Java API
- `Client`：演示 `setPTZ(...)` 的命令行客户端
- `ServerSimulator`：Java UDP 模拟器，用于回包测试
- `PTZStatus`：`getPTZ()` 返回对象
- `Protocol`：Java 侧协议辅助类，与 C++ 保持一致
- `DebugClient`：状态监听和 PTZ 控制的调试入口

## 当前 JNA 接口

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

### 1. 构建 C++ 库

```bash
cd cam_mon_cpp
cmake -S . -B build
cmake --build build --config Release
```

输出目录：`cam_mon_cpp/build/jna/`

### 2. 构建 Java 项目

```bash
cd cam_mon_java
mvn clean package -Pwindows
```

## 常用运行方式

### 1. 运行 PTZ 客户端

```bash
cd cam_mon_java
java -cp target/cam_mon_java-1.0-SNAPSHOT.jar com.marble.cammon.Client 127.0.0.1 4001 servo
```

### 2. 运行状态监听演示

```bash
cd cam_mon_java
java -cp target/cam_mon_java-1.0-SNAPSHOT.jar com.marble.cammon.DebugClient 5001
```

### 3. 运行 Java 模拟器

```bash
cd cam_mon_java
java -cp target/cam_mon_java-1.0-SNAPSHOT.jar com.marble.cammon.ServerSimulator 4001
```

## 回归测试

仓库内当前会跑这两个集成测试：

- `ProtocolIntegrationTest`
- `PTZIntegrationTest`

它们依赖 `ServerSimulator` 和 `cammon.dll`，用于验证 Java ↔ C++ ↔ UDP 的端到端链路。

## 关键文件

- `src/main/java/com/marble/cammon/CamMonLibrary.java` - JNA 接口定义
- `src/main/java/com/marble/cammon/CamMonNative.java` - JNA 包装类
- `src/main/java/com/marble/cammon/Client.java`
- `src/main/java/com/marble/cammon/Protocol.java`
- `src/main/java/com/marble/cammon/PTZStatus.java`
- `src/main/java/com/marble/cammon/ServerSimulator.java`

## 依赖

- JNA 5.13.0 (`net.java.dev.jna:jna:5.13.0`)
- Java 8+
- C++ 编译器（用于构建 cam_mon_cpp）

## 相关项目

- [cam_mon_cpp](../cam_mon_cpp/) - 调用的底层 C++ 库