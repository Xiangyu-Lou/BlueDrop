# 聚音 BlueDrop

> 将手机音频无线接入电脑，与电脑音频混流并可超过 100% 增益放大，适合监听、录制和扩音场景。

**平台：** Windows 10 2004 (Build 19041) 及以上 · x64

---

## 功能概览

### 蓝牙监听（主要功能）
通过蓝牙将手机（或其他 A2DP 设备）的音频接入电脑，无需数据线，在电脑端实时监听手机正在播放的内容。

### 增益模式（Boost）
当手机音频音量不足时，在软件层对音频信号进行放大，支持 0%–1000% 的增益范围，内置软限幅（Soft Clip）防止削波失真。

> **增益模式依赖 VB-Cable。** 需要提前安装 [VB-Cable 虚拟声卡](https://vb-audio.com/Cable/)。

### 广播模式（Route B）
将手机音频与电脑麦克风混合后输出到虚拟声卡（VB-Cable），适合直播、录音等需要混流的场景。

### 其他
- 系统托盘图标，支持最小化到托盘后台运行
- 关闭行为可配置（询问 / 最小化到托盘 / 直接退出）
- 内置自动检查更新（GitHub Releases）
- 可开启调试日志（`--log` 参数或设置页开关）

---

## 系统要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Windows 10 2004（Build 19041）或更高版本 |
| 架构 | x64 |
| 蓝牙 | 支持 A2DP Sink 的蓝牙适配器 |
| 增益模式 | 需额外安装 [VB-Cable](https://vb-audio.com/Cable/)（免费） |

---

## 安装与使用

### 安装
1. 从 [Releases](https://github.com/Xiangyu-Lou/BlueDrop/releases) 下载最新版本的 `BlueDrop-vX.X.X-win64.zip`
2. 解压到任意目录（建议不含中文和空格的路径）
3. 运行 `BlueDrop.exe`

### 连接手机
1. 打开 BlueDrop，进入**主页**
2. 点击**开始监听**
3. 在手机蓝牙设置中找到并连接电脑（设备名为你的电脑名）
4. 连接成功后，手机播放的音频会自动通过电脑播放

### 开启增益模式
1. 确保已安装 VB-Cable
2. 手机连接后，在主页找到**增益模式**开关并打开
3. 调整增益滑块至合适大小（建议从 150%–200% 开始）

---

## 已知限制

### AirPods 开启麦克风时出现卡顿

**现象：** 使用 AirPods 作为耳机监听时，不开麦克风一切正常；开启 AirPods 内置麦克风后，音频刚开始流畅，运行一段时间后出现卡顿。

**根本原因（协议层限制，无法软件修复）：**

蓝牙有两种底层链路类型：
- **ACL**（异步）：A2DP 高质量音乐使用，弹性调度，支持重传
- **SCO**（同步）：HFP 麦克风使用，**强制预留固定时隙**，不可剥夺

当 AirPods 麦克风开启时，Windows 强制将 AirPods 切换到 HFP 模式，SCO 链路在蓝牙时分调度中打出规律性"空洞"，导致 A2DP 音频数据包的传输变得碎片化。AirPods 使用 AAC 编解码器对传输抖动（jitter）高度敏感，内部播放缓冲区会缓慢耗尽，通常在数分钟后触发 underrun 并出现卡顿。

**解决方案：使用独立麦克风**（USB 麦克风、3.5mm 麦克风等），让 AirPods 始终保持纯 A2DP 模式。

---

## 版本历史

### v0.1.6
- **修复：** 蓝牙连接建立后延迟打开音频端点（`OpenAsync`），消除 AirPods 等高质量蓝牙耳机在空闲时被 A2DP 带宽干扰导致的卡顿
- **修复：** 增益模式引入 1ms 高精度计时器（`timeBeginPeriod`）+ 自适应 Sleep 策略，解决 AirPods 在增益模式下因渲染缓冲区周期性震荡引起的卡顿
- **优化：** 增益模式预填充静音帧从 50ms 提升至 65ms，增大抗抖动余量

### v0.1.5
- **新增：** 自动检查更新功能（GitHub Releases API）
- **新增：** 独立 Updater.exe，解决 Windows 无法覆盖运行中程序的问题
- **修复：** 蓝牙连接时有条件地恢复系统默认音频端点，避免 AirPods 触发 A2DP→HFP 切换

### v0.1.4
- **新增：** 日志文件轮转（最大 5MB，自动备份为 .bak）
- **新增：** 日志存储位置改为应用程序目录
- **修复：** 部分耳机蓝牙连接后静音的问题

### v0.1.3
- **新增：** 关闭行为设置（询问 / 最小化 / 退出）
- **新增：** 设置页检查更新 UI 预留
- **修复：** 托盘菜单断开连接调用 stopListening 而非 disconnect

---

## 从源码构建

### 依赖
- Visual Studio 2022 Build Tools（含 MSVC v143 + Windows SDK）
- Qt 6.11+ for MSVC 2022 x64
- CMake 3.20+（VS 附带版本即可）
- vcpkg（用于 GTest）

### 构建
```bat
# Debug 构建
build.bat

# Release 打包
package.bat
```

---

## 技术说明

| 技术 | 用途 |
|------|------|
| `Windows.Media.Audio.AudioPlaybackConnection` (WinRT) | 将 PC 暴露为蓝牙 A2DP Sink，接收手机音频 |
| WASAPI Loopback Capture | 捕获蓝牙虚拟端点或 VB-Cable 的音频数据 |
| WASAPI Render | 将处理后的音频输出到耳机 |
| `IPolicyConfig`（未文档化 COM 接口）| 切换系统默认播放端点（增益模式音频重定向） |
| MMCSS "Pro Audio" | 音频处理线程最高优先级调度 |
| Qt 6 + QML | UI 框架 |

---

## License

MIT
