# BlueDrop — Claude 开发说明

## 项目启动

```
# 编译
F:\Project\BlueDrop\build.bat

# 运行（必须通过 run.bat，它设置 Qt PATH + vcvarsall）
F:\Project\BlueDrop\run.bat

# 带日志运行（日志写入 bluedrop_debug.log）
set PATH=A:\Qt\6.11.0\msvc2022_64\bin;%PATH%
F:\Project\BlueDrop\build\src\BlueDrop.exe --log
```

**注意**：不要直接运行 `build\src\BlueDrop.exe`，会因缺少 Qt DLL 而报错。

## 开发规范

- **每次重大修改后执行 git commit**（功能完成、Bug 修复、配置变更均算重大修改）
- **不要主动打包**（release 目录）。只有用户明确要求时才构建发布包
- 测试用 `run.bat` 或 `--log` 模式，不用专门构建 release

## 项目架构

```
src/
  app/           Application 核心（初始化检查）
  audio/         WASAPI 音频引擎（AudioEngine、CaptureStream、RenderStream）
  bluetooth/     BT A2DP 连接管理（AudioPlaybackConnection）
  qml/           QML UI（pages/ + components/）
  system/        Logger、SystemChecker
  viewmodel/     MVVM ViewModels（BluetoothVM、DeviceVM、MixerVM、SettingsVM）
tests/           GTest 单元测试
```

## 增益模式（Boost Mode）技术说明

路径：BT → VB-Cable（切换系统默认端点）→ WASAPI Loopback 捕获 → 软件增益 → 耳机渲染

关键参数：
- 渲染 buffer：100ms（100,000 hns），pre-fill 50ms，防止 underrun
- 捕获 maxFrames：1920（4× 单周期），防止 WASAPI 捕获 buffer 积压溢出
- 支持增益 0 ~ 10.0×（1000%），使用 softClip 防止削波失真
