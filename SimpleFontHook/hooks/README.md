# 钩子模块

## 职责

`hooks/` 负责 Detours 安装、字体替换模型、字体 API 查询、文本绘制和引擎适配。
公共入口为 `FontHooks::Install(HMODULE)`、配置通知与正常退出接口。

通用钩子的核心任务，是让同一个替换字体在“创建、选择、绘制、度量、身份查询和原始
字体数据查询”中保持一致。只改 `CreateFont*` 会让程序从查询接口看到互相矛盾的字体
名称、字符集或尺寸，因此这些 API 由统一模型和缓存共同处理。

## 聚合结构

`font_hooks.cpp` 按固定顺序包含 `internal/**/*.cppinc`，形成单一钩子翻译单元。该结构让
原始 API 指针、Detours 目标、字体缓存和引擎状态保持内部链接。

| 路径 | 职责 |
| --- | --- |
| `font_hooks.h` | 钩子子系统公共接口 |
| `font_hooks.cpp` | 实现聚合和包含顺序 |
| `hook_policy.*` | 钩子启用、兼容策略和调试采样 |
| `internal/font_hooks_*.cppinc` | 安装、运行时、绘制、代码页和文字映射 |
| `internal/model/` | 字体替换缓存、逻辑字体视图、度量和字形索引 |
| `internal/queries/` | 字体标识、枚举、度量和字体数据查询 |
| `internal/engines/` | 引擎检测、资源、缓存和运行时桥接 |

## 钩子类别

- `AttachFontCreationHooks`：`CreateFont*` 与 `CreateFontIndirect*`。
- `AttachRuntimeLibraryHooks`：GDI+、DirectWrite 和延迟模块加载。
- `AttachTextDrawingHooks`：`TextOut`、`ExtTextOut` 与 `DrawText`。
- `AttachDcFontSelectionHooks`：HDC 与 HFONT 选择状态。
- `AttachGlyphOutlineHooks`：字形位图、轮廓和索引。
- `AttachTextLayoutMetricHooks`：宽度、范围、ABC 度量与字距。
- `AttachFontIdentityQueryHooks`：`GetObject`、`GetTextMetrics` 与 `GetTextFace`。
- `AttachFontEnumerationHooks`：字体族与扩展 LOGFONT 枚举。
- `AttachGlyphMetricSupplementHooks`：字距调整和字形定位信息。
- `AttachFontDataQueryHooks`：SFNT 表、字符范围、语言和轮廓度量。
- `AttachDirectWriteHooks`：`DWriteCreateFactory` 与工厂虚表。

## 调用流程

1. `FontHooks::Install` 加载配置并准备需要在 Detours 事务前完成的静态检测。
2. `AttachHooksByCategory` 按职责挂接字体、编码、Profile、文件和运行时 API。
3. 字体创建钩子保存源 `LOGFONT`，构造目标 `LOGFONT` 与替换 `HFONT`，并写入带
   `ConfigVersion` 的缓存项。
4. 绘制与查询钩子根据 HDC、原始字体和替换字体查找同一缓存视图，分别返回真实绘制
   结果与面向程序的逻辑属性。
5. 引擎文件分派、托管运行时和延迟模块路径覆盖通用字体 API 看不到的资源。
6. 配置通知清理或版本化各类缓存，并把应用动作投递到要求的线程。

## 字体替换模型

`internal/model/` 将源 LOGFONT、替换 LOGFONT、HFONT、配置版本和对外查询视图组织为统一
缓存。模型支持字体名称、字符集、尺寸、字重、垂直度量、字距、行距和字形索引别名。
编码转换与文字映射使用独立状态，不从替换字体推断文本代码页。

## 引擎适配

引擎适配器处理内部字体资源、预渲染缓存、归档内容或托管运行时对象。完整引擎列表、
检测规则和各目录文档见 [internal/engines/README.md](internal/engines/README.md)。

文件型适配统一调用 `engine_file_dispatch.cppinc`。运行时型适配通过窗口消息、工作线程、
Python pending call、Mono/IL2CPP 线程附加或引擎函数钩子执行。

## 设计理由

- 聚合 `.cppinc` 让原始 API 指针、递归保护和缓存状态保持内部链接，同时用文件边界
  分隔实现职责。
- `org*` 指针表示真实系统入口；钩子内部通过它们调用 API，防止内部探测形成递归。
- 逻辑字体视图把“引擎实际使用的替换对象”和“程序查询时应看到的属性”分开，避免
  为兼容某个查询而破坏真实渲染对象。
- `ConfigVersion` 参与缓存身份，使字体名称相同但度量、字符集能力或字体数据不同的
  配置也能获得独立结果。
- 高开销检测位于安装、准备或工作线程阶段；每帧路径只执行缓存查找和有界转换。

## 生命周期

1. `FontHooks::Install` 加载配置并准备静态适配状态。
2. Detours 事务按钩子类别挂接公共 API 与引擎函数。
3. 事务提交后启动延迟模块、运行时和字体选择器线程。
4. 配置应用递增 `ConfigVersion` 并通知各缓存与引擎。
5. 正常退出停止工作线程并关闭句柄；进程分离路径只发布退出信号。

## 扩展规则

1. 新 API 归入职责相符的 `Attach*Hooks` 类别。
2. 原始函数指针与钩子处理函数位于同一聚合翻译单元。
3. 兼容决策集中在 `hook_policy.*`。
4. 高频路径使用缓存状态、有界查找和采样日志。
5. 引擎专用文件行为通过统一文件分派器发布。
6. 新引擎目录提供独立 README，并登记到引擎索引。

## 验证

通用钩子覆盖字体创建、HDC 选择、绘制、查询、缓存命中、配置关闭和选择器线程。引擎
适配覆盖正向检测、负向检测、配置版本刷新和目标架构。C++ 修改同时编译 Win32 与 x64
Release。
