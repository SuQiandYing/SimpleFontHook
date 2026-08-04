# SimpleFontHook

SimpleFontHook 是面向 Windows 游戏的字体兼容与替换 DLL。产物以 `winmm.dll` 代理形式
随目标进程加载，在保留系统 `winmm` 导出的同时接入 GDI、GDI+、DirectWrite 和部分
引擎运行时。项目把字体对象、字体数据、文本编码和引擎资源视为不同层次，按实际调用
路径选择最小适配面。

## 主要能力

- 替换 ANSI、Unicode、GDI+ 和 DirectWrite 字体创建结果。
- 调整字体高度、宽度、字距、行距、粗细和垂直度量。
- 独立控制字符集伪装、代码页重定向和繁简文字映射。
- 通过字体选择器实时切换字体并保存 `FontHook.ini`。
- 在字体选择器内提供普通用户指南，说明字体选择、应用、显示修正和排版调整流程。
- 为已确认的 YU-RIS CP932、GBK、Big5 与 Relirium YDG 位图字体资源实时生成系统字体图集。
- 为 Unity Mono/IL2CPP、classic CatSystem2、BGI、Artemis 与 Artemis Legacy、KiriKiri、
  Entis、Softpal、Escude、Mirai、Majiro、DxLib、TinkerBell、TyranoScript、Ren'Py 和
  YU-RIS 提供兼容路径。

## 工作原理

一次字体应用由以下层次协作完成：

1. Windows 从程序目录加载代理 `winmm.dll`，代理导出继续转发到系统 `winmm`。
2. `FontHooks::Install` 读取 `FontHook.ini`，在同一 Detours 事务中安装通用 API 钩子和
   已确认能力的引擎入口。
3. 通用字体模型根据源 `LOGFONT`、目标字体和配置版本创建替换 `HFONT`，并为绘制、
   度量、枚举和字体数据查询提供一致视图。
4. 引擎适配器处理通用 API 看不到的资源，例如预渲染字体、磁盘缓存、归档内容、
   Python 映射以及 Mono/IL2CPP 托管对象。
5. 字体选择器保存配置并递增 `Config::ConfigVersion`；缓存和引擎资源只接受对应版本的
   数据，从而让一次切换形成完整快照。

这套分层的目的，是让字体替换只改变字体相关状态。字符集伪装、代码页转换和文字映射
各有独立开关与数据流，替换字体的字符集不会被当作源文本编码依据。

## 功能证据与复刻

模块文档使用四类信息描述功能：输入与前置条件、执行入口、输出与回退、验证方法。
引擎适配器把身份确认、版本能力和资源路由分别记录，通用 GDI 导入、文件扩展名或单个
文件名不构成引擎身份。结论可沿着源码文件、符号、命令或带字段的运行日志回到证据来源。

在另一份样本上复刻时，先记录样本 SHA-256、架构和 `FontHook.ini` 快照，再建立关闭目标
适配器时的基线；随后收集 PE、文件结构和运行时证据，提取最小输入输出契约，并执行正向、
非目标、能力缺失和边界场景。完整步骤与记录模板见
[功能证据与复刻流程](docs/reproduction.md)。

## 快速使用

1. 确认目标程序架构，32 位程序使用 Win32 产物，64 位程序使用 x64 产物。
2. 备份目标程序目录中已有的 `winmm.dll` 和 `FontHook.ini`。
3. 将对应架构的 `winmm.dll` 放到目标可执行文件同级目录。
4. 启动程序，按游戏文本的实际编码选择中文、日文、繁体或韩文字符集，再选择并应用字体。

字体选择器优先注册单键 `Insert`，冲突时依次尝试 `Pause` 和 `Scroll Lock`。界面页脚与
指南显示实际注册成功的按键。配置文件保存在 DLL 同级目录，使用 UTF-8 编码。

## 编译

前置条件：Visual Studio 2022、v143 C++ 工具集和 Windows 10 SDK。仓库根目录已经
包含 Win32 与 x64 使用的 Detours 头文件和静态库。

```bat
build_x32.bat
build_x64.bat
```

Release 产物：

- Win32：`Release/winmm.dll`
- x64：`x64/Release/winmm.dll`

完整的编译和发布检查见 [docs/build-and-release.md](docs/build-and-release.md)。

## 目录结构

| 路径 | 职责 |
| --- | --- |
| `SimpleFontHook/dllmain.cpp` | 进程加载入口和退出信号 |
| `SimpleFontHook/hooks/` | Detours 钩子、字体替换模型和引擎适配器 |
| `SimpleFontHook/font/` | TTF/OTF/TTC 解析与字体表修改 |
| `SimpleFontHook/ui/` | 字体选择器、配置应用和绘制 |
| `SimpleFontHook/utils.cpp` | 配置持久化、自定义字体加载和诊断设施 |
| `Release/`、`x64/Release/` | Release 构建产物 |

## 模块边界

- `dllmain.cpp` 只发布安装入口和退出信号，业务逻辑位于钩子、字体、UI 与配置模块。
- `hooks/font_hooks.cpp` 聚合 `.cppinc` 实现，使原始 API 指针和引擎状态保持内部链接。
- `font/` 只处理 SFNT/TTC 字节，不读取游戏文本，也不参与引擎身份判断。
- `ui/` 只修改配置并发布版本通知，不直接操作引擎内部对象。
- `engine_file_dispatch.cppinc` 是文件隐藏、虚拟属性与重定向的唯一跨引擎分派入口。

## 文档导航

- [架构说明](docs/architecture.md)
- [配置说明](docs/configuration.md)
- [功能证据与复刻流程](docs/reproduction.md)
- [编译与发布](docs/build-and-release.md)
- [诊断与问题定位](docs/diagnostics.md)
- [贡献与编码守则](CONTRIBUTING.md)
- [自动化代理守则](AGENTS.md)
- [钩子模块结构](SimpleFontHook/hooks/README.md)
- [引擎适配器索引](SimpleFontHook/hooks/internal/engines/README.md)
- [UI 模块结构](SimpleFontHook/ui/README.md)

## 核心不变量

- 字体替换只负责字体对象和字体数据，不得隐式改变文本编码。
- 文本编码判断以程序原始字符集和显式配置为依据，不从替换字体反推。
- 引擎适配器必须有可验证的正向检测条件，不能仅凭通用 GDI 导入判定具体引擎。
- 钩子内部调用真实 Win32 API 时使用保存的 `org*` 指针，避免递归进入钩子。
- 高频路径不得重复扫描可执行文件、资源归档或字体文件。
- 代码修改必须同时保持 Win32 与 x64 Release 可编译。
