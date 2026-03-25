# BlueDrop 开发日志

## 2026-03-24 - 项目初始化

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

### Step 1: 项目脚手架
- 创建 CMake + Qt6 QML + vcpkg 项目结构
- Qt6 通过 Qt Installer 安装，CMake 通过 `Qt6_ROOT` 定位
- vcpkg 仅管理 GTest 依赖
