# BlueDrop 开发日志

## 2026-03-25 - v0.1.2 发布

### 新增功能

#### 最小化到托盘 + 关闭行为对话框
- 首次点击关闭按钮弹出对话框，让用户选择"最小化到托盘"或"退出程序"
- 可勾选"记住该选择"，下次直接执行，不再询问
- 选择通过 `QSettings` 保存至 `behavior/closeAction`
- 新增 `TrayManager` 类（`src/app/TrayManager.h/cpp`）：
  - 系统托盘图标（蓝色圆形，QPainter 绘制）
  - 右键菜单：显示窗口 / 增益模式开关（可勾选）/ 退出

#### 增益模式退出修复（重要 Bug Fix）
- **问题：** 增益模式开启时直接关闭应用，系统默认播放设备会停留在 VB-Cable 上，导致电脑完全没有声音
- **根因：** 应用退出前未恢复原始系统默认端点
- **修复：** `MixerVM::~MixerVM()` 析构时若增益已启用，自动停止 boost 并调用 `SessionVolumeController::setDefaultPlaybackEndpoint` 恢复原端点
- MixerVM 在 `main.cpp` 栈上分配，`app.exec()` 返回后必然被析构

#### 状态持久化
通过 `QSettings` 记忆以下状态，下次启动自动恢复：
| 设置项 | Key | 默认值 |
|--------|-----|--------|
| 增益模式开关 | `boost/enabled` | false |
| 增益倍数 | `boost/gain` | 1.0 |
| 增益静音 | `boost/muted` | false |
| 监听音量 | `monitor/volume` | 1.0 |
| 监听静音 | `monitor/muted` | false |
| 关闭行为 | `behavior/closeAction` | "" (每次询问) |

- 启动后 200ms 由 QML `Timer` 触发 `mixerVM.autoRestoreState()`，延迟加载确保 QML 加载完毕后再恢复状态
- 修复 `sessionFound` 处理逻辑：原来会读取会话当前音量覆盖用户设置，改为将已保存音量 *应用到* 会话

#### UI 改进
- **帮助页面**：新增 `HelpPage.qml`，包含 4 步操作引导 + 增益模式说明 + 广播混音说明
- **增益模式 "?" 提示**：增益开关旁新增问号按钮，悬停显示 ToolTip 说明延迟和卡顿风险
- **VB-Cable 未安装提示**：增益区域在未安装 VB-Cable 时显示"下载安装"按钮（`Qt.openUrlExternally`）
- **麦克风跟随系统默认**：麦克风下拉框新增"跟随系统默认"作为第一项（默认选中）
- **移除无效的输出设备选择器**：本地音频路由始终跟随 Windows 系统默认端点，选择器对 Route-A 无效，已从 HomePage 和 BroadcastPage 移除

### Bug 修复

1. **增益模式静音按钮失效**
   - 原因：增益 Slider 的 `muted` 绑定硬编码为 `false`，未连接到 ViewModel
   - 修复：MixerVM 新增 `boostMuted` 属性；`setBoostMuted` 将引擎增益切换为 0 或恢复 `m_boostGain`

2. **增益模式标签文字溢出**
   - 原因：标签从"手机音频"变为"手机音频（增益）"后超出 `Layout.preferredWidth: 64`，与滑块重叠
   - 修复：移除 VolumeSlider 标签的固定宽度约束，改用 `implicitWidth`

3. **Win11 显示为 Windows 10**
   - 原因：`RtlGetVersion` 在 Win11 上仍返回 major=10；未检查 build 号
   - 修复：`SystemChecker` 中增加 build ≥ 22000 判断，正确显示"Windows 11 Build XXXXX"

### 依赖变更
- 新增 `Qt6::Widgets`（`QApplication`、`QSystemTrayIcon`、`QMenu`、`QAction` 均需要）
- `QApplication` 替代 `QGuiApplication`

### Release
- **Tag:** v0.1.2
- **GitHub Release:** https://github.com/Xiangyu-Lou/BlueDrop/releases/tag/v0.1.2
- **包含:** BlueDrop.exe + Qt runtime + 内嵌字体 (~75MB zip)

---

## 2026-03-25 - v0.1.1 开发

### 主要变更
- 增益模式开关、增益滑块 UI（HomePage.qml）
- SessionVolumeController：IPolicyConfig 切换系统默认端点实现
- AudioEngine：boost 模式（loopback capture → 软件增益 × N → render 输出）
- 增益参数：render buffer 100ms，pre-fill 50ms，捕获 maxFrames 1920

---

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
