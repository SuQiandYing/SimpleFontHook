# ✧ SimpleFontHook ✧

[![Sakura Night Aesthetic](https://img.shields.io/badge/Aesthetic-Sakura%20Night-pink)](https://github.com/your-repo)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Build](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](BUILD)

**SimpleFontHook** 是一款专为游戏（尤其是 Galgame）设计的动态字体替换与优化工具。它通过强大的 API Hooking 技术，实现在无需修改游戏原始资源的情况下，实时替换字体、优化渲染效果并修复由于区域设置（Locale）引起的乱码问题。

---

## ✨ 核心特性

- **🚀 多引擎兼容**: 深度支持 GDI, GDI+, 以及现代化的 DirectWrite 渲染引擎。
- **🌓 字符集伪装 (Codepage Spoofing)**: 针对日区游戏或乱码情况，支持将 Shift-JIS 等字符集实时伪装为 GB2312 渲染，解决乱码困扰。
- **📁 本地字体自动加载**: 仅需将 `.ttf` / `.otf` / `.ttc` 字体文件放入 DLL 同级目录，工具将自动加载并提供 OS/2 表实时补丁以增强兼容性。
- **🛠️ 模块化设计**: 采用 Detours 实现高效、稳定的代码注入。

---

## 📸 界面预览

> <img width="480" height="720" alt="image" src="https://github.com/user-attachments/assets/b792dc66-a6d0-41ad-a11b-fc7dfc74ab15" />

- **快速搜索**: 点击搜索框快速定位系统或本地字体。
- **实时应用**: 双击列表项目即可部分引擎（已知softpal引擎）瞬间全局应用字体变更，无需重启游戏，其他引擎需要具体问题具体分析比如设置更新或者SL。
- **缩放控制**: 实时缩放 UI 界面以适配不同分辨率的游戏窗口。

---

## 🛠️ 如何使用

### 1. 部署
1. 将编译生成的 `winmm.dll`放入游戏根目录。
2. 将你喜欢的字体文件（可选）也放入同级目录。

### 2. 操作
- 工具随游戏启动后，选择字体并按 **Enter** 或双击。

### 3. 构建
项目支持 x86 与 x64 架构，直接运行提供的批处理文件即可：
- `build_x32.bat`: 构建 32 位版本。
- `build_x64.bat`: 构建 64 位版本。


## 🤝 贡献与许可

基于 **MIT License** 开源。欢迎提交 Issue 或 Pull Request 来完善对更多特殊引擎的支持。

> *“在这个充满幻想的世界里，每一行文字都值得被更加优美地呈现。”*
