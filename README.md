# SimpleFontHook

[![C++](https://img.shields.io/badge/Language-C++-00599C.svg?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-0078D4.svg?style=flat&logo=windows)](https://www.microsoft.com/windows)

**SimpleFontHook** 是一个为 Windows 平台（特别是 Galgame 和旧式应用程序）设计的高性能字体 Hook 工具。它能够实时拦截并替换应用程序中的字体调用，支持 GDI、GDI+ 和 DirectWrite 渲染引擎，并提供极其精美的 "Sakura Next" 样式配置界面。

---

## ✨ 核心特性

- **🚀 全面 Hook 支持**
  - **GDI**: 拦截 `CreateFont`, `TextOut`, `ExtTextOut`, `DrawText` 等 API。
  - **GDI+**: 拦截 `GdipDrawString`, `GdipCreateFont` 等，支持私有字体集注入。
  - **DirectWrite**: 拦截接口虚函数，完美解决现代游戏的字体替换问题。
- **🌸 Sakura Next GUI**
  - 内置精美的侧边栏导航界面。
  - 支持毛玻璃效果与动态动画。
  - **实时预览**: 修改字体、缩放、粗细后立即生效，无需重启游戏。
  - **模糊搜索**: 快速从系统成百上千的字体中找到所需。
- **🔧 深度字体控制**
  - **比例缩放**: 自由调整字体的宽度和高度比例。
  - **粗细调节**: 实时修改 `FontWeight`。
  - **编码仿真 (Codepage Spoofing)**: 自动修补字体 `OS/2` 表，解决日文/中文乱码显示问题。
- **🛡️ 灵活部署**
  - 支持通过 `winmm.dll` 代理进行零配置注入（DLL Hijacking）。

---

## 🛠️ 技术栈

- **语言**: C++17
- **Hook 库**: Microsoft Detours
- **UI 框架**: 自研沉浸式 Win32 UI (Sakura 主题)
- **字体处理**: GDI/GDI+ & 自定义 TrueType 表解析

---

## 🚀 快速开始

### 1. 编译
使用 Visual Studio 2022 打开 `SimpleFontHook.sln`，选择 `Release` 配置进行编译。
- 生成 `SimpleFontHook.dll`。

### 2. 安装
将编译生成的 `SimpleFontHook.dll` 重命名为 `winmm.dll` 放置在游戏根目录。
或者使用配套的 `FontLoader.exe` 进行注入。

### 3. 使用
- 运行游戏。
- 默认按下唤起热键（通常为特定组合键或自动弹出）即可打开 **Font Picker** 界面。
- 选择你喜欢的字体并即时调整参数。

---

## 📂 项目结构

```text
├── SimpleFontHook/
│   ├── dllmain.cpp          # 主要入口与 Hook 逻辑实现
│   ├── font_picker.cpp      # Sakura Next UI 实现
│   ├── font_patcher.cpp     # 字体 OS/2 表实时补丁
│   ├── utils.cpp            # 配置加载与通用工具
│   └── winmm_proxy.h        # 导出函数转发定义
├── build_x64.bat           # 自动化编译脚本
└── detours.h                # Detours 静态链接库
```

---

## ⚙️ 配置说明

可以通过修改 `Config.ini` 或直接在 UI 中调整：

| 参数 | 说明 |
| :--- | :--- |
| `EnableFontHook` | 是否启用全局字体替换 |
| `FontHeightScale` | 字体高度缩放比例 (1.0 = 原大) |
| `FontWidthScale` | 字体宽度缩放比例 |
| `CodepageSpoof` | 是否启用字符集欺骗 (解决乱码) |
| `ForcedFontName` | 强制替换的字体名称 |

---

## 🤝 贡献与反馈

欢迎提交 Issue 或 Pull Request 来改进本项目。如果你喜欢这个项目，请给它一个 **Star**！

---

## 📄 开源协议

本项目采用 [MIT License](LICENSE) 协议。

