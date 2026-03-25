# 聚音 (BlueDrop) - 技术架构文档

> **版本：** 1.2
> **日期：** 2026-03-25
> **阶段：** v0.1.2

---

## 1. 技术栈总览

| 层面 | 技术选型 |
|------|----------|
| **语言** | C++20 |
| **UI 框架** | Qt 6 + QML |
| **音频引擎** | WASAPI (Windows Audio Session API) |
| **蓝牙** | Windows.Devices.Bluetooth (WinRT) + AudioPlaybackConnector 方案 |
| **构建系统** | CMake 3.20+ / MSVC 2022 (x64 only) |
| **包管理** | vcpkg |
| **打包** | windeployqt + PowerShell Compress-Archive → .zip |
| **最低系统** | Windows 10 2004 (Build 19041) x64 |
| **Qt 模块** | Core, Gui, Qml, Quick, **Widgets**（托盘需要） |
| **状态持久化** | QSettings (注册表 HKCU\Software\BlueDrop\BlueDrop) |

---

## 2. 模块架构

```
┌─────────────────────────────────────────────────────────────┐
│                     QML UI Layer                             │
│  (主页 / 广播 / 帮助 / 设置 / 导航栏 / 关闭对话框)           │
└──────────────┬────────────────────────────────┬─────────────┘
               │ Q_PROPERTY / signal-slot        │
┌──────────────▼────────────────────────────────▼─────────────┐
│                  Qt C++ Bridge Layer                          │
│  (ViewModel: BluetoothVM, DeviceVM, MixerVM, SettingsVM)     │
│  (App: TrayManager)                                          │
└──┬──────────┬──────────────┬──────────────┬──────────────────┘
   │          │              │              │
┌──▼───┐ ┌───▼────┐ ┌──────▼──────┐ ┌─────▼──────┐
│  BT  │ │ Device │ │   Audio     │ │  System    │
│Module│ │ Enum   │ │   Engine    │ │  Check     │
│      │ │ Module │ │  + Session  │ │  Module    │
└──┬───┘ └───┬────┘ │  VolumeCtrl│ └────────────┘
   │         │      └──────┬──────┘
   │    WASAPI API    WASAPI API + IPolicyConfig
   │         │             │
   ▼         ▼             ▼
 WinRT    Windows       Windows
 BT API   Audio Stack   Audio Stack + 系统默认端点切换
```

---

## 3. 模块详细设计

### 3.1 蓝牙模块 (BluetoothManager)

**职责：** 管理 A2DP Sink 连接生命周期，接收蓝牙音频原始 PCM 数据。

**核心类：**

```cpp
// src/bluetooth/BluetoothManager.h
class BluetoothManager : public QObject {
    Q_OBJECT
public:
    enum class ConnectionState {
        Disconnected,   // 未连接
        WaitingPair,    // 等待配对
        Connected,      // 已连接
        Reconnecting    // 断线重连中
    };
    Q_ENUM(ConnectionState)

    void startListening();       // 开始暴露 A2DP Sink
    void stopListening();        // 停止暴露
    void disconnect();           // 主动断开

    ConnectionState state() const;
    QString connectedDeviceName() const;
    AudioFormat receivedFormat() const;  // 采样率/声道/编码

signals:
    void stateChanged(ConnectionState newState);
    void deviceNameChanged(const QString& name);
    // 注意：不提供 PCM 数据回调。BT 音频由 Windows 自动播放在系统端点上，
    // AudioEngine 通过 WASAPI Loopback Capture 从该端点捕获。
};
```

**实现要点：**
- 基于 `AudioPlaybackConnection` WinRT API（Win10 2004+ 原生支持）
- 使用 `DeviceWatcher` + `GetDeviceSelector()` 发现 BT 设备
- `TryCreateFromId` → `StartAsync` → `OpenAsync` 建立连接
- **不直接接收 PCM 数据** — Windows 自动将 BT 音频路由到系统默认播放端点
- AudioEngine 通过 WASAPI Loopback Capture 从该端点间接捕获 BT 音频
- 蓝牙断开后自动进入 `Reconnecting` 状态，持续监听重连

### 3.2 设备枚举模块 (DeviceEnumerator)

**职责：** 枚举系统音频设备，监听热插拔事件。

```cpp
// src/audio/DeviceEnumerator.h
class DeviceEnumerator : public QObject {
    Q_OBJECT
public:
    struct AudioDevice {
        QString id;          // WASAPI 设备 ID
        QString displayName; // 显示名称
        bool isDefault;      // 是否系统默认
        bool isVirtual;      // 是否虚拟设备 (VB-Cable 等)
        DeviceType type;     // Input / Output
    };

    QList<AudioDevice> inputDevices() const;   // 录音设备列表
    QList<AudioDevice> outputDevices() const;  // 播放设备列表

    bool isVBCableInstalled() const;  // 检测 VB-Cable 是否存在

signals:
    void devicesChanged();  // 设备列表变更（热插拔）
};
```

**实现要点：**
- 使用 `IMMDeviceEnumerator` (WASAPI COM 接口) 枚举设备
- 注册 `IMMNotificationClient` 监听设备增删事件
- 通过设备名称/ID 匹配识别 VB-Cable 虚拟设备

### 3.3 音频引擎 (AudioEngine)

**职责：** 核心音频处理——采集、混音、路由输出。

```cpp
// src/audio/AudioEngine.h
class AudioEngine : public QObject {
    Q_OBJECT
public:
    // 设备绑定
    void setMicrophoneDevice(const QString& deviceId);
    void setMonitorOutputDevice(const QString& deviceId);  // 路由 A 输出
    void setVirtualCableDevice(const QString& deviceId);    // 路由 B 输出

    // 音频流控制
    void start();
    void stop();
    bool isRunning() const;

    // 设备设置
    void setLoopbackDevice(const QString& deviceId);  // BT 音频播放端点（Loopback 捕获源）

    // 音量控制
    void setPhoneVolume(float gain);    // 0.0 ~ 2.0
    void setMicVolume(float gain);      // 0.0 ~ 2.0
    void setPhoneMixRatio(float ratio); // 0.0 ~ 1.0 (路由 B 中手机占比)
    void setPhoneMuted(bool muted);
    void setMicMuted(bool muted);

private:
    // 内部音频处理线程
    std::unique_ptr<AudioCaptureStream> m_micCapture;    // 麦克风采集
    std::unique_ptr<AudioRenderStream>  m_monitorOutput; // 路由 A 输出
    std::unique_ptr<AudioRenderStream>  m_virtualOutput;  // 路由 B 输出
    RingBuffer m_bluetoothBuffer;  // 蓝牙音频环形缓冲区
};
```

**内部数据流（修正版 — Loopback Capture 模型）：**

```
iPhone --BT A2DP--> AudioPlaybackConnection --> Windows 自动播放在监听端点
                                                          |
        AudioEngine: WASAPI Loopback Capture on 监听端点 --+
                                                           |
                              +--> 路由 A: 自动完成（BT 音频已在耳机播放）
                              |
                              ├──→ 增益 × 混入比例 ──┐
                              │                       ├──→ 混音 ──→ 路由 B (VB-Cable)
麦克风: WASAPI Capture ──→ 增益调节 ────────────────┘
```

**实现要点：**
- 所有音频处理在专用高优先级线程中执行
- 内部统一工作格式：**48kHz / 32-bit float / Stereo**
- 蓝牙输入若为 44.1kHz 则通过线性插值重采样至 48kHz
- RingBuffer 容量设计：约 200ms（48000 × 2ch × 4bytes × 0.2s ≈ 76KB）
- 混音算法：逐样本加权求和 + 软限幅（soft clipping）防爆音
- WASAPI 使用共享模式（shared mode），缓冲区周期 10ms

### 3.4 系统校验模块 (SystemChecker)

**职责：** 启动时检测运行环境是否满足要求。

```cpp
// src/system/SystemChecker.h
class SystemChecker {
public:
    struct CheckResult {
        bool osVersionOk;        // Win10 2004+
        bool bluetoothAvailable; // 蓝牙适配器就绪
        bool vbCableInstalled;   // VB-Cable 已安装
        QString osVersion;       // 实际 OS 版本字符串
        QString failureMessage;  // 首个失败项的提示信息
    };

    static CheckResult runAllChecks();
};
```

**实现要点：**
- OS 版本检测：`RtlGetVersion`，build ≥ 22000 显示 "Windows 11"，否则显示 "Windows 10"
- 蓝牙检测：`BluetoothFindFirstRadio` 或 WinRT `BluetoothAdapter::GetDefaultAsync`
- VB-Cable 检测：通过 `IMMDeviceEnumerator` 遍历设备名称匹配

### 3.5 系统托盘模块 (TrayManager)

**职责：** 管理系统托盘图标和右键菜单，支持最小化到托盘。

```cpp
// src/app/TrayManager.h
class TrayManager : public QObject {
    Q_OBJECT
public:
    void show();
    Q_INVOKABLE void updateBoostState(bool enabled);  // 同步菜单勾选状态
signals:
    void showWindowRequested();    // 用户点击"显示窗口"或单击托盘图标
    void boostToggleRequested();   // 用户点击"增益模式"
    void exitRequested();          // 用户点击"退出"
};
```

**实现要点：**
- 需要 `Qt6::Widgets` + `QApplication`（`QSystemTrayIcon` 依赖 Widgets 模块）
- 图标通过 `QPainter` 在 32×32 QPixmap 上绘制蓝色圆形（无需外部图标文件）
- 右键菜单：`QMenu` + `QAction`，增益模式项为 checkable
- 单击/双击托盘图标均触发 `showWindowRequested()`

### 3.6 增益模式音频路由

**增益模式（Route-A Boost）数据流：**

```
iPhone --BT A2DP-->  AudioPlaybackConnection
                           │
                     Windows 将 BT 音频路由到系统默认端点
                           │
               ┌───────────▼──────────────────┐
               │  IPolicyConfig::SetDefaultEndpoint  │
               │  系统默认端点 → VB-Cable Input  │
               └───────────┬──────────────────┘
                           │ BT 音频播放至 VB-Cable
                           │
               WASAPI Loopback Capture on VB-Cable
                           │
                     软件增益 × N (0~10.0×)
                     softClip 防削波
                           │
               WASAPI Render → 真实耳机端点
                           │
                      用户听到放大音频
```

**端点恢复保证：**
- `MixerVM` 在 `main.cpp` 栈上分配，`app.exec()` 返回（无论正常退出还是关闭对话框退出）后必然析构
- `MixerVM::~MixerVM()` 若 `m_boostEnabled`，调用 `m_engine->stopBoost()` + `setDefaultPlaybackEndpoint(m_savedDefaultEndpoint)`

---

## 4. 目录结构

```
BlueDrop/
├── CMakeLists.txt                 # 顶层 CMake
├── vcpkg.json                     # vcpkg 依赖清单
├── docs/
│   ├── PRD.md                     # 产品需求文档
│   └── Architecture.md            # 本文档
├── src/
│   ├── main.cpp                   # 入口：日志、QApplication、VM 创建、QML 加载
│   ├── app/
│   │   ├── Application.h/cpp      # 应用生命周期管理（初始化检查）
│   │   └── TrayManager.h/cpp      # 系统托盘图标 + 右键菜单
│   ├── bluetooth/
│   │   ├── BluetoothManager.h/cpp # A2DP Sink 管理
│   │   └── BluetoothTypes.h       # 蓝牙相关类型定义
│   ├── audio/
│   │   ├── AudioEngine.h/cpp          # 混音 + 路由核心 + 增益模式
│   │   ├── SessionVolumeController.h/cpp  # WASAPI 会话音量 + IPolicyConfig 端点切换
│   │   ├── AudioCaptureStream.h/cpp   # WASAPI 采集封装
│   │   ├── AudioRenderStream.h/cpp    # WASAPI 输出封装
│   │   ├── DeviceEnumerator.h/cpp     # 设备枚举 + 热插拔
│   │   ├── RingBuffer.h/cpp           # 无锁环形缓冲区
│   │   └── Resampler.h/cpp            # 采样率转换
│   ├── system/
│   │   ├── SystemChecker.h/cpp    # 环境校验（OS/BT/VBCable）
│   │   └── Logger.h/cpp           # 文件日志（--log 参数启用）
│   ├── viewmodel/
│   │   ├── BluetoothVM.h/cpp      # 蓝牙状态 ViewModel
│   │   ├── DeviceVM.h/cpp         # 设备选择 ViewModel（含"跟随系统默认"）
│   │   ├── MixerVM.h/cpp          # 混音台 ViewModel（含增益/持久化/析构恢复）
│   │   └── SettingsVM.h/cpp       # 设置页 ViewModel（含 closeAction 持久化）
│   └── qml/
│       ├── main.qml               # 主窗口 + 导航框架 + 关闭对话框 + 托盘连接
│       ├── pages/
│       │   ├── HomePage.qml       # 主页（监听状态 + 增益模式 + VB-Cable 提示）
│       │   ├── BroadcastPage.qml  # 广播页（麦克风 + 虚拟声卡选择）
│       │   ├── SettingsPage.qml   # 设置页（系统信息 + 关闭行为）
│       │   └── HelpPage.qml       # 帮助页（操作步骤 + 功能说明）
│       ├── components/
│       │   ├── VolumeSlider.qml   # 音量推子（支持静音、displayValue 自定义）
│       │   ├── StyledComboBox.qml # 自定义下拉框
│       │   └── NavSidebar.qml     # 左侧导航栏（4 项）
│       └── theme/
│           └── Theme.qml          # 统一主题（颜色/圆角/字体/间距）
├── docs/
│   ├── PRD.md                     # 产品需求文档
│   ├── Architecture.md            # 本文档
│   └── DevLog.md                  # 开发日志
├── tests/                         # GTest 单元测试（37 个）
├── resources/
│   └── fonts/                     # 内嵌 Noto Sans SC（Regular + Medium）
├── build.bat                      # 快速 Debug 构建脚本
├── run.bat                        # 设置 Qt PATH 后运行
└── package.bat                    # Release 构建 + windeployqt + ZIP 打包
```

---

## 5. 构建配置

### 5.1 CMake 核心配置

```cmake
cmake_minimum_required(VERSION 3.20)
project(BlueDrop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 仅 x64
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "BlueDrop only supports 64-bit builds")
endif()

# 最低 Windows 版本: Win10 2004
set(CMAKE_SYSTEM_VERSION 10.0.19041.0)
add_compile_definitions(WINVER=0x0A00 _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000007 NOMINMAX)

# Qt 6 — Widgets 必须包含（QSystemTrayIcon 依赖）
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Widgets)
```

### 5.2 打包流程（package.bat）

1. `cmake --preset release` 配置 Release 构建
2. `cmake --build` 编译
3. 拷贝 `BlueDrop.exe` 到 `release/BlueDrop-vX.X.X/`
4. `windeployqt --release --qmldir src/qml --no-translations` 部署 Qt 运行时
5. `Compress-Archive` 打包成 zip

---

## 6. 线程模型

| 线程 | 职责 | 优先级 |
|------|------|--------|
| **UI 线程** | QML 渲染、用户交互 | 普通 |
| **蓝牙 MTA 线程** | WinRT AudioPlaybackConnection 生命周期（MTA COM apartment，需保活） | 高 |
| **音频处理线程** | 麦克风采集 + 混音 + 双路由输出 | 最高 (MMCSS "Pro Audio") |
| **增益模式线程** | VB-Cable Loopback Capture → 软件增益 → 耳机 Render | 最高 (MMCSS) |
| **设备监听线程** | IMMNotificationClient 回调 | 普通 |

**线程间通信：**
- 蓝牙线程 → 音频线程：通过无锁 RingBuffer（SPSC，单生产者单消费者）
- 音频线程 → UI 线程：通过 Qt 信号槽（QueuedConnection）上报电平/状态
- UI 线程 → 音频线程：通过原子变量（std::atomic）传递音量/静音控制

**MTA 线程保活：**
蓝牙 `AudioPlaybackConnection` 必须在 MTA COM apartment 上保持，否则约 2.5s 后 RPC_E_SERVERFAULT 崩溃。实现：`std::thread` + `std::condition_variable`，连接线程阻塞等待断开信号，保持 COM 引用。

---

## 7. 关键技术决策记录

| 编号 | 决策 | 理由 |
|------|------|------|
| **D1** | WASAPI 共享模式而非独占模式 | 独占模式会抢占设备导致其他应用无法使用，共享模式延迟已可满足 ≤50ms 要求 |
| **D2** | 内部工作格式统一 48kHz/f32/Stereo | 避免混音时反复转换格式，48kHz 是 WASAPI 默认共享格式 |
| **D3** | 无锁 RingBuffer 而非互斥锁队列 | 音频实时线程不能阻塞，无锁 SPSC 队列保证确定性延迟 |
| **D4** | Qt QML 而非 Widgets | QML 更适合实现参考图中的现代卡片式 UI，且动画性能更好 |
| **D5** | MMCSS "Pro Audio" 线程优先级 | Windows 多媒体类调度服务，确保音频线程获得足够 CPU 时间片 |
| **D6** | 蓝牙模块与 AudioPlaybackConnector 相同方案 | 经过验证的 Win10 2004+ A2DP Sink 实现路径，降低技术风险 |
| **D7** | IPolicyConfig 切换系统默认端点 | 非公开 COM 接口，但被 EarTrumpet/SoundSwitch 等广泛使用；是实现增益模式音频重定向的唯一无驱动方案 |
| **D8** | MixerVM 析构恢复端点而非 atexit | 栈分配保证析构顺序可预期，atexit 无法访问对象状态 |
| **D9** | 移除输出设备选择器 | 本地音频路由由 Windows 自动决定（BT 音频播放在系统默认端点），应用层无法在不切换系统默认端点的前提下重定向；选择器误导用户 |
| **D10** | QSettings 持久化而非自定义配置文件 | Qt 原生支持，Windows 下写入注册表，无需文件 I/O 错误处理 |

---

## 8. 外部接口与依赖

| 接口 | API | 用途 |
|------|-----|------|
| **WASAPI** | `IAudioClient`, `IAudioCaptureClient`, `IAudioRenderClient` | 音频采集与输出 |
| **MMDevice** | `IMMDeviceEnumerator`, `IMMNotificationClient` | 设备枚举与热插拔监听 |
| **WinRT Bluetooth** | `Windows::Devices::Bluetooth` | A2DP Sink 连接管理 |
| **WinRT Media** | `Windows::Media::Audio` | 蓝牙音频流接收 |
| **MMCSS** | `AvSetMmThreadCharacteristicsW` | 音频线程提权 |
| **RtlGetVersion** | `ntdll.dll` | 精确获取 OS Build 号（用于 Win11 检测，build ≥ 22000） |
| **IPolicyConfig** | undocumented COM | 切换 Windows 系统默认播放端点（增益模式音频重定向） |
| **QSystemTrayIcon** | `Qt6::Widgets` | 系统托盘图标 + 右键菜单 |
| **QSettings** | `Qt6::Core` | 用户设置持久化（注册表 HKCU\Software\BlueDrop\BlueDrop） |
