# 奶螈桌宠 · 原生版

有同学写了Electron版「奶螈桌宠」但**体积太大**，所以我用C重写了一版。
Windows 10 / 11 x64 下单文件运行，不需要安装 Electron、Node.js、Python、.NET 或 VC++ 运行库，只依赖系统自带 DLL，也不需要管理员权限或联网。

[下载最新 Release](https://github.com/JohnWongHZW/naiyuan-desktop-pet-native/releases/latest) · [从源码构建](#构建)

## 特点

- 约 **15 MB**，相比原 Electron 版（约 120 MB）缩小约 **88%**。
- 单 EXE，双击即用；首次开启 BGM 时把内嵌 MP3 写到系统临时目录，正常退出时删除。
- 透明、无边框、置顶的 200 × 200 逻辑像素窗口；不显示任务栏入口。
- 保留原待机 GIF、点击 GIF、背景 MP3 的字节内容与原动画帧时长。
- 左键拖动；移动超过 3 个逻辑像素视为拖拽，不触发点击。
- 点击播放动画与音效，固定 **4.7 秒**后恢复待机；动画期间的重复点击不重新触发。
- 右键菜单：开启/关闭满屏弹跳、开启/关闭 BGM、退出。
- 弹跳碰壁反弹并开始 0.5°/步 的慢速旋转；按住暂停旋转，下次碰壁恢复；关闭弹跳复位角度。
- 拖拽释放的投掷速度限制为每步 ±15 逻辑像素；静止释放采用随机方向、速度 4。
- 点击时暂停 BGM，动画结束恢复；动画期间手动切换 BGM 会取消自动恢复。
- 高 DPI（Per-Monitor V2）缩放，按主显示器工作区限制移动范围。

点击音效由原 OGG 转为 48 kHz、双声道、16-bit PCM WAV，避免额外携带 OGG 解码库；解码后的 PCM 与原 OGG 一致，无重采样、裁剪或音量调整。

## 构建

### 环境

- Windows 10 / 11 x64
- [MinGW-w64](https://www.mingw-w64.org/) x86_64 工具链，需要 `g++.exe` 与 `windres.exe`（本仓库用 GCC 16.2.0, x86_64-ucrt-posix-seh 验证）
- PowerShell 5.1 或更高版本

运行桌宠本身不需要以上任何构建工具。

### 命令

```powershell
git clone https://github.com/JohnWongHZW/naiyuan-desktop-pet-native.git
cd naiyuan-desktop-pet-native
.\build.ps1 -Toolchain 'C:\path\to\mingw64\bin'
```

产物写到仓库的**上级目录**，文件名为 `奶螈桌宠 1.1.0-原生版.exe`。`-Toolchain` 。


Release 附件的 SHA-256：

```text
6991BAB200E891EA5437B30057EBFF64445EBA155C612624FA65538059D4BB1B
```

每次重新构建得到的哈希会因时间戳不同而变化，这不影响文件内容的一致性。

## 目录

| 路径 | 用途 |
| --- | --- |
| `pet.cpp` | 全部应用逻辑：窗口、GIF 动画、GDI+ 绘制、拖动、弹跳、音频 |
| `resources.rc` | 资源清单：图标、待机/点击 GIF、BGM、点击音效、版本信息 |
| `app.manifest` | 普通用户权限、Windows 10/11 兼容性、Per-Monitor V2 DPI |
| `build.ps1` | 一键构建脚本 |
| `assets/app.ico` | 程序图标 |
| `assets/idle.gif` | 待机动画 |
| `assets/click.gif` | 点击动画 |
| `assets/bgm.mp3` | 背景音乐 |
| `assets/click.wav` | 点击音效（由原 OGG 解码） |

仓库不包含构建产物

## 说明

- 这是对原程序的功能重写，**不是**对 Electron 原版的二进制重打包；窗口缩放与旋转使用 GDI+，边缘插值可能与 Chromium 略有差异。
- 动画、拖动、防重复点击、4.7 秒切换、速度限制、碰壁反弹、旋转状态和 100% / 125% / 150% / 200% DPI 绘制均做过自动检查；系统音频接口返回播放成功，但未做人耳音质与长期连续循环验收。
- 实际验证在单机完成，未覆盖所有 Windows 版本、声卡和混合 DPI 多显示器组合。
- 素材来自互联网以及**上海应用技术大学Minecraft社团**；本仓库为构建与分发该版而附带这些素材，再分发时请沿用原授权范围。
