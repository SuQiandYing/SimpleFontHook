# Ren'Py 适配器

## 职责

Ren'Py 使用自带的 FreeType/HarfBuzz 渲染路径。本适配器在 Python 解释器线程维护
Ren'Py 字体替换映射，并通过 `renpy.config.file_open_callback` 将稳定虚拟字体名连接到
当前源字体文件。

## Python 调度边界

Ren'Py 的文本布局、字体加载与缓存都位于 Python 和原生 FreeType/HarfBuzz 扩展中，
字体创建不会经过 GDI 或 DirectWrite。直接替换磁盘文件既无法覆盖脚本字体别名，也会
破坏游戏自定义的 `file_open_callback` 和字体映射。

适配器使用 Ren'Py 公共配置对象表达替换关系：映射表把脚本字体导向稳定虚拟名称，文件
回调再把虚拟名称导向当前源字体。缓存刷新也通过 Ren'Py 自身入口执行。

## 检测条件

Ren'Py 框架布局要求 `renpy/` 与 `renpy/__init__.py` 存在，并至少包含 `renpy/text`、
`renpy/display` 或 `renpy/common` 中的一个核心子系统。游戏脚本目录名称和单个字体模块
文件不参与身份判断。

检测结果在进程生命周期内缓存。

## 文件结构

- `renpy_font.cppinc`：适配器聚合入口。
- `renpy_state.cppinc`：配置快照、工作线程、Python API 和版本状态。
- `renpy_detection.cppinc`：根目录识别和路径辅助函数。
- `renpy_font_source.cppinc`：定位并校验源字体，解析 TTC 字体面索引。
- `renpy_python_bridge.cppinc`：绑定 Python API、注册虚拟文件回调并构建应用脚本。
- `renpy_runtime.cppinc`：工作线程、配置通知和退出。

## 字体源访问

适配器优先按用户实际选择的 `SourceFontNameW` 定位系统或当前用户安装的字体文件，
找不到时再检查 `FontFileName` 指向的游戏本地字体。普通 TTF/OTF 只读取 12 字节文件头
完成基本校验；TTC/OTC 才读取集合目录，并按字体名称选择正确的 face 索引。

Ren'Py 映射使用稳定的 `__simplefonthook__/selected.*` 虚拟名称。文件加载回调收到该名称
后，以只读二进制方式打开源字体路径。运行时状态由源路径、虚拟名称、TTC face 索引和
配置版本组成。

`SourceFontNameW` 是进程内状态，用来保留用户实际选择的字体。垂直度量或代码页修补创建
内存字体克隆时，通用 GDI 钩子仍可使用克隆名，而 Ren'Py 始终解析原始字体文件。

## Python 应用流程

1. 工作线程解析当前 `ConfigVersion` 对应的源字体路径和 TTC face 索引。
2. 在已加载模块中定位 Python，并解析 `Py_IsInitialized`、`Py_AddPendingCall` 和
   `PyRun_SimpleString`。
3. 使用 `Py_AddPendingCall` 将应用回调投递到解释器线程。
4. 包装 `renpy.config.file_open_callback`，并串联游戏提供的回调。
5. 包装或恢复 `renpy.config.font_replacement_map`。
6. 调用 `renpy.text.font.free_memory()` 清理字体缓存。
7. 启用刷新时调用 `restart_interaction` 重新开始当前交互。

映射关闭时恢复进入适配器前的文件回调，并清理挂在 `renpy.config` 上的路径状态。
绝对路径按 UTF-8 十六进制传入 Python，避免 Python 2 源码编码和 Windows 非 ASCII 路径问题。

## 设计约束依据

- 稳定虚拟名称把游戏脚本中的字体标识与 Windows 绝对路径分开，热切换只更新回调状态。
- 包装现有 `file_open_callback` 并串联原回调，保持游戏自己的资源加载契约。
- `Py_AddPendingCall` 把状态修改投递到解释器线程，工作线程只执行文件定位和字节校验。
- `SourceFontNameW` 保存真实来源名称，使通用 GDI 字体表克隆不会污染文件定位。
- TTC/OTC 只解析集合目录与目标面，普通 TTF/OTF 使用最小头校验，降低准备阶段 I/O。
- 应用前后核对 `ConfigVersion` 和字体名，过期准备结果不会覆盖较新的用户选择。

## 配置

- `EnableRenPyHook`
- `RenPyRedirectFonts`
- `RenPyRefreshFontOnSwitch`
- 通用的 `EnableFontHook`、`EnableFaceNameReplace` 和 `ForcedFontNameW`

## 线程不变量

- 工作线程只解析和校验字体源，不能直接执行 Python 脚本。
- Python 状态只在待处理回调所在的解释器线程修改。
- 配置快照在互斥锁内复制，文件读取和 Python 调用在锁外执行。
- 提交准备结果前再次校验请求版本与字体名称。
- 退出时唤醒工作线程，并在正常退出路径有界等待。

## 证据与复刻

1. 记录 `renpy/`、`renpy/__init__.py` 和至少一个核心子系统的存在性及文件哈希；只有脚本
   目录或字体文件的样本作为非目标对照。
2. 保存源字体路径、SFNT/TTC 头、目标字体面、虚拟名称和 `ConfigVersion`。
3. 分别在 Python 初始化前、初始化后和解释器退出阶段观察 `Py_IsInitialized`、
   `Py_AddPendingCall`、`PyRun_SimpleString` 的绑定与投递结果。
4. 安装游戏自定义 `file_open_callback`，记录包装前后回调顺序、虚拟文件返回值和普通资源
   透传结果。
5. 启用/关闭映射并切换字体，记录 `font_replacement_map`、`free_memory()`、
   `restart_interaction` 和非 ASCII 路径结果。

| 场景 | 预期结果 |
| --- | --- |
| 完整框架清单 + Python API 可用 | 在解释器线程安装映射和文件回调 |
| 框架成立但 Python 尚未初始化 | 工作线程等待并重试，不执行脚本 |
| 自定义文件回调存在 | 虚拟字体由适配器处理，其他请求串联原回调 |
| TTC/OTC | 虚拟名称保存目标 face 索引 |
| 映射关闭 | 恢复进入适配器前的回调与映射状态 |

证据目录包含框架清单、字体哈希、Python API 绑定、`[RenPy]` 日志和正负样本。
字段规范见 [功能证据与复刻流程](../../../../../docs/reproduction.md)。

## 验证

覆盖 Ren'Py 正向与负向检测、Python 初始化时序、普通 TTF/OTF、TTC 不同字体面、
非 ASCII 源路径、游戏自定义 `file_open_callback`、字体映射启用/关闭、热切换和退出。
