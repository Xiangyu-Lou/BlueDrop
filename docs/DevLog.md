# BlueDrop 开发日志

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

- [ ] iPhone 可通过蓝牙发现并连接 PC
- [ ] PC 以 A2DP Sink 身份接收立体声音频
- [ ] 用户可选择物理麦克风、监听耳机、虚拟声卡输出
- [ ] 路由 A：手机音频可从指定耳机播出
- [ ] 路由 B：手机音频 + 麦克风混合后可在 VB-Cable Input 端被第三方软件读取
- [ ] 各音量推子和静音开关实时生效
- [ ] 蓝牙断开后 UI 正确提示，重连后自动恢复
