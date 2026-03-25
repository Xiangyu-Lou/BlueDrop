# BlueDrop 开发日志

## 2026-03-25 - v0.1 后续开发

### 增益模式 (Boost Mode) 开发中

**需求：** BT 音频监听音量受 WASAPI ISimpleAudioVolume 上限 1.0 限制，用户反馈音量不够大。需要超过 100% 的软件增益放大。

**技术方案：双端点路由**
```
开关关闭（直通模式）:
  BT → 默认端点(耳机) → 用户听到
  控制: ISimpleAudioVolume 0~100%

开关打开（增益模式）:
  BT → VB-Cable端点（通过切换系统默认播放设备）
      ↓ WASAPI Loopback Capture
      ↓ 软件增益 ×N（可到 400%）
      ↓ WASAPI Render
      → 耳机端点 → 用户听到
```

**实现进度：**
| Step | 内容 | 状态 |
|------|------|------|
| 1 | IPolicyConfig 默认端点切换 (SessionVolumeController) | ✅ 完成 |
| 2 | AudioEngine boost 模式 (独立 loopback→gain→render 线程) | ✅ 完成 |
| 3 | MixerVM boostEnabled/boostGain 属性 + 切换逻辑 | ✅ 完成 |
| 4 | HomePage.qml 增益开关 UI | ✅ 完成 |
| 5 | 测试验证 | 🔄 进行中 |

**关键技术点：**
- `IPolicyConfig` — 非公开但广泛使用的 COM 接口（EarTrumpet/SoundSwitch 等均使用），用于切换系统默认播放端点
- 增益模式和 Route B 广播互斥（都需要 VB-Cable）
- Application 析构时自动恢复原默认端点

---

## 2026-03-24 - v0.1 发布

### 新增功能
- **SessionVolumeController**: WASAPI 音频会话级音量控制，独立调节 BT 音频音量
- **Logger**: 可开关日志系统（`--log` 参数或 `BLUEDROP_LOG=1` 环境变量）
- **MTA 线程保活**: 修复 BT 连接后 ~2.5s 崩溃问题（RPC_E_SERVERFAULT + ACCESS_VIOLATION）
- **BT 会话匹配**: session name 含 "A2DP" 或连接设备名
- **UI 重构**: 参考 Typeless 风格，轻量化主题、微阴影卡片、统一字重
- **思源黑体**: 内嵌 Noto Sans CJK SC 字体（Regular + Medium）
- **输出设备**: 新增"跟随系统默认"选项
- **StyledComboBox**: 自定义下拉框样式
- **BroadcastPage**: 广播页面占位

### 遇到的问题及解决

1. **BT 连接后崩溃 (RPC_E_SERVERFAULT 0x80010105)**
   - 原因：`fire_and_forget` 协程在 Qt STA 线程执行 WinRT 异步操作，COM apartment 冲突
   - 解决：改用专门的 MTA std::thread 执行 WinRT 连接操作

2. **MTA 线程退出导致无声音**
   - 原因：线程退出后 COM apartment 引用计数降为零，连接对象失效
   - 解决：让连接线程通过 condition_variable 保持存活直到断开连接

3. **WASAPI 会话匹配失败**
   - 原因：BT 音频会话 ID 不含 "Bluetooth"，匹配规则太窄
   - 解决：增加 "A2DP" 和设备名匹配

4. **WASAPI ISimpleAudioVolume 上限 1.0**
   - 发现：系统 API 最大只到 100%，无法通过会话音量超过此上限
   - 方案：设计双端点路由增益模式（v0.1 后续开发）

5. **QML 字体粗细不一致**
   - 原因：混用 `font.bold: true` (Weight 700) 和 `font.weight: Font.DemiBold` (Weight 600)
   - 解决：统一使用 `Font.DemiBold`

6. **QML 资源路径错误**
   - 原因：QTP0001 NEW policy 下资源前缀为 `:/qt/qml/<URI>/`
   - 解决：URL 从 `qrc:/BlueDrop/qml/main.qml` 改为 `qrc:/qt/qml/BlueDrop/qml/main.qml`

7. **VolumeSlider 信号冲突**
   - 原因：QML property 自动生成 Changed 信号，手动声明同名信号会冲突
   - 解决：移除手动声明的重复信号

### Release
- **Tag:** v0.1
- **GitHub Release:** https://github.com/Xiangyu-Lou/BlueDrop/releases/tag/v0.1
- **包含:** BlueDrop.exe + Qt runtime + 内嵌字体 (~75MB zip)

---

## 2026-03-24 - Phase 1 完整开发

### 环境信息
- **OS:** Windows 11 Pro (Build 26200)
- **编译器:** MSVC 2022 Build Tools (v14.37)
- **Qt:** 6.11.0 MSVC 2022 64-bit (`A:\Qt\6.11.0\msvc2022_64`)
- **vcpkg:** `F:\Project\vcpkg`
- **CMake:** VS 2022 Build Tools 内置

### 架构关键决策
- **蓝牙音频捕获方式：** AudioPlaybackConnection API 不提供 PCM 回调，Windows 自动将 BT 音频路由到系统播放端点。需通过 WASAPI Loopback Capture 从该端点捕获蓝牙音频。
- **路由 A 实现：** 由 Windows 自动完成（BT 音频播放在监听耳机上），无需额外代码。
- **路由 B 实现：** Loopback 捕获 BT 音频 + 麦克风采集 → 混音 → 输出至 VB-Cable。

### 开发进度

| Step | 模块 | 状态 | 测试数 |
|------|------|------|--------|
| 1 | 项目脚手架 (CMake + Qt6 + C++/WinRT) | ✅ 完成 | 1 |
| 2 | SystemChecker (OS/BT/VBCable 检测) | ✅ 完成 | 6 |
| 3 | DeviceEnumerator (WASAPI 设备枚举 + 热插拔) | ✅ 完成 | 7 |
| 4 | BluetoothManager (AudioPlaybackConnection 生命周期) | ✅ 完成 | 7 |
| 5 | AudioEngine (RingBuffer + Capture + Render + 混音) | ✅ 完成 | 16 |
| 6 | Application + ViewModel 层 | ✅ 完成 | - |
| 7 | QML UI (卡片式布局) | ✅ 完成 | - |
| 8 | 集成 + 文档更新 | ✅ 完成 | - |

**总测试数：37 个 GTest 单元测试，全部通过**

### 遇到的问题及解决

1. **Windows `min`/`max` 宏冲突** — 添加 `NOMINMAX` 编译定义解决
2. **Qt DLL 在测试发现阶段找不到** — 在 build.bat/test.bat 中将 Qt bin 目录加入 PATH
3. **QObject MOC 链接失败** — 从直接 include 源文件改为 `BlueDrop_core` 静态库方案
4. **RingBuffer wrap-around bug** — `read()` 方法第二块数据的目标偏移量写错，修复为 `data + firstChunk`
5. **`WIN32_LEAN_AND_MEAN` 过于激进** — 排除了蓝牙和 NTSTATUS 相关头文件，移除后仅保留 `NOMINMAX`

### 端到端测试清单（手动）

- [x] iPhone 可通过蓝牙发现并连接 PC
- [x] PC 以 A2DP Sink 身份接收立体声音频
- [x] 用户可选择物理麦克风、监听耳机、虚拟声卡输出
- [x] 路由 A：手机音频可从指定耳机播出
- [ ] 路由 B：手机音频 + 麦克风混合后可在 VB-Cable Input 端被第三方软件读取
- [x] BT 音频会话音量可独立调节
- [ ] 各音量推子和静音开关实时生效
- [ ] 蓝牙断开后 UI 正确提示，重连后自动恢复
