# 引擎适配器

## 职责

`engines/` 存放 GDI、GDI+ 和 DirectWrite 通用钩子之外的引擎兼容逻辑。每个适配器负责
目标环境检测、引擎资源或缓存处理、配置版本刷新和诊断信息。具体字体替换、字符集、
代码页与文字映射仍由各自的通用模块提供。

## 通用路径覆盖范围

通用钩子只能覆盖经过 Windows 字体 API 的调用。游戏引擎还可能读取预渲染字形、把字体
打包进归档、长期持有内部缓存，或直接使用 FreeType、Python、Mono、IL2CPP 与 Chromium
渲染。此时替换 `HFONT` 只能影响旁路流程，屏幕上的真实文本仍由引擎资源决定。

适配器负责把当前字体配置转换成引擎能够消费的资源或对象，并在配置版本变化时维护其
缓存生命周期。适配器不接管通用字体模型，也不因为识别到某个引擎而自动改变文本编码。

## 编译结构

引擎实现以 `.cppinc` 分片参与聚合翻译单元：

- `hooks/font_hooks.cpp` 包含字体、文件和运行时适配器。
- `hooks/hook_policy.cpp` 包含只返回钩子策略的适配器。
- `.cppinc` 使用内部链接共享已保存的 `org*` API、Detours 目标和进程状态。
- 每个引擎目录的 `README.md` 是该适配器职责、检测条件和配置的入口。

## 文件 API 边界

- `engine_file_dispatch.cppinc` 维护文件隐藏、重定向、虚拟属性和引擎优先级。
- `engine_file_hooks.cppinc` 适配 Win32 ANSI 与 Unicode 文件 API。
- 引擎目录提供路径分类、资源内容和状态更新函数。
- 分派器调用真实文件 API 时使用已保存的 `org*` 目标。

文件打开、属性查询和目录搜索共享同一套分类规则。A/W 入口只负责参数转换与结果发布。

统一分派器的处理顺序具有语义：先判断文件是否需要隐藏，再依次处理缓存文件、真实字体
文件重定向、Web/ASAR 覆盖和 Artemis 虚拟资源。每个处理器只有在完整匹配时才声明请求，
未处理请求回到保存的 `org*` API。线程级递归保护和 `EngineCommon::IsInternalFileQuery`
保证适配器自己的探测与字体读取看到真实文件系统。

## 身份、能力与路由

适配器按三个阶段工作：

1. 身份确认使用引擎模块标记、运行时导出、框架目录结构，或“归档格式解析成功并找到
   引擎资源”的组合证据。
2. 能力确认检查当前版本是否提供目标 API、方法、缓存布局或配置字段。
3. 资源路由只在身份和能力成立后，根据真实请求路径选择对应处理器。

文件扩展名、通用归档名、通用 GDI 导入和任意单个文件名只用于缩小查找范围。`.arc`、
`.dat`、`.pfs`、`.med`、`.tft` 等共享后缀本身不参与引擎定案。静态身份结果按进程缓存；
动态模块导出在模块加载后重新查询。高频钩子只读取缓存状态并执行有界路径分类。

三个阶段的证据要求如下：

- 身份回答“当前进程属于哪个引擎”，需要稳定且具有区分度的证据。
- 能力回答“这个版本能否使用某条适配路径”，允许同一引擎的不同版本采用不同入口。
- 路由回答“当前请求是否属于该能力”，只处理已经确认身份后的具体资源。

把扩展名直接当作身份会让共享 `.arc`、`.dat`、`.med` 或 `.tft` 的程序进入错误处理器；
把能力并入身份则会让一个缺少可选接口的版本失去其他可用适配能力。

## 通用工作流程

1. 安装或首次低频查询收集身份信息，并把静态结果缓存到进程状态。
2. 适配器检查配置开关和版本能力，定位需要的导出、方法、归档资源或缓存布局。
3. 通用钩子命中具体文件、字体查询或运行时调用时，只执行有界分类并读取准备结果。
4. 适配器返回虚拟文件、字体数据或运行时对象；未命中路径保持原 API 语义。
5. `ConfigVersion` 变化时，适配器使旧缓存失效，并在正确线程准备或应用当前版本资源。

## 配置版本

`Config::ConfigVersion` 标识一次完整的运行时字体配置。字体字节、资源内容、度量、对象
句柄和缓存刷新记录对应版本。`FontHooks::NotifyConfigChanged` 将版本通知分派给具备热切换
能力的引擎适配器。

配置版本参与字体字节、虚拟路径、资源对象和刷新记录的身份。这样即使字体族名相同，
字重、度量、字符集能力或 `cmap` 配置变化也会形成新的资源快照。

## 引擎索引

| 引擎 | 主要职责 | 文档 |
| --- | --- | --- |
| 公共基础 | 身份策略、路径边界、模块查询、字体来源和虚拟文件 | [common/README.md](common/README.md) |
| Artemis | 表文件、PFS、字体资源和运行时缓存 | [artemis/README.md](artemis/README.md) |
| Artemis Legacy | IET/文本脚本、RFT 和版本化字体资源 | [artemis_legacy/README.md](artemis_legacy/README.md) |
| BGI | Win32 GDI 栅格度量、IAT 与字形缓存 | [bgi/README.md](bgi/README.md) |
| classic CatSystem2 | FreeType 文件字体、`font` 目录与字体资源注册 | [catsystem2/README.md](catsystem2/README.md) |
| DxLib | `_FONTSET.MED` 缓存一致性 | [dxlib/README.md](dxlib/README.md) |
| EntisGLS | 位图字体注册、内部字体对象和字形栅格 | [entis/README.md](entis/README.md) |
| Escu:de | `configure.cfg` 字体配置视图 | [escude/README.md](escude/README.md) |
| KiriKiri / TVP | `.tft` 预渲染字体路径 | [krkr/README.md](krkr/README.md) |
| Majiro | FCD 磁盘缓存和运行时字形缓存 | [majiro/README.md](majiro/README.md) |
| Mirai | FreeType 字体数据源和字体文件重定向 | [mirai/README.md](mirai/README.md) |
| Ren'Py | Python 字体映射、虚拟文件回调和缓存刷新 | [renpy/README.md](renpy/README.md) |
| Softpal | Pal 默认字体类型和位图字体入口 | [softpal/README.md](softpal/README.md) |
| TinkerBell | 宽字符字体创建与 DC 跟踪策略 | [tinkerbell/README.md](tinkerbell/README.md) |
| TyranoScript | ASAR 覆盖、Web 字体和 Electron 桥接 | [tyrano/README.md](tyrano/README.md) |
| Unity | Mono/IL2CPP、TMP 字体资源和线程调度 | [unity/README.md](unity/README.md) |
| YU-RIS | YPF 位图字体识别、CP932/GBK/Big5/YDG 资源配置和解码后图集替换 | [yuris/README.md](yuris/README.md) |

## 证据索引

| 适配器 | 身份证据 | 能力/资源入口 | 负向样本 |
| --- | --- | --- | --- |
| Artemis | PFS `pf2/pf6/pf8` + `list_windows*.tbl` 资源族 | 表字段、PFS 字体和缓存 | PFS 无表资源 |
| Artemis Legacy | PFS + `system/_base` 中的 `.iet` | IET、RFT、虚拟 SFNT | 只有 Artemis 表资源 |
| BGI | Buriko 模块标记或 BURIKO 魔数 | Win32 GDI 导入、缓存布局、度量 | 只有 GDI 导入 |
| classic CatSystem2 | `CatScene/cs2confx` + `kcFontImage_Win/FT` | `font/*.ttf/.otf/.ttc/.otc` | 只有一组标记 |
| DxLib | DxLib 模块标记 + 字体契约 | `_FONTSET.MED`、`GetFontData` | 只有 MED 后缀 |
| EntisGLS | `SGLFont::m_pFontStock` + 位图加载器类证据 | BMF、RTTI/vtable、引用字体 | 缺少任一类证据 |
| Escu:de | `Company=ESCUDE` + 非空 `Product` | Profile `[Font] Face/Font` | 缺少公司或产品 |
| KiriKiri/TVP | ANSI/Unicode `mapPrerenderedFont` | `.tft` 隐藏和实时 GDI 路径 | 只有一个方法标记 |
| Majiro | `MajiroObj/MajiroArcV/MajiroSavV` | `savedata/fc_*.fcd`、运行时表 | 只有 `.fcd` 后缀 |
| Mirai | FreeType 契约 + 字体配置标记 | `GetFontData` 固定来源、Windows Fonts | 只有 FreeType |
| Ren'Py | `renpy/`、`__init__.py` 和核心子系统 | Python 回调、虚拟字体名 | 只有脚本目录 |
| Softpal | `PalFontBegin/SetType/GetType` 导出集合 | 类型 `4 -> 1`、`DEFAULT_FONT.DAT` | 导出不完整 |
| TinkerBell | `Software\\TinkerBell\\` 或 Cyberworks 标记组合 | `HookPolicy` 创建/选择策略 | 相似 DAT/DLL |
| TyranoScript | 解包 `resources/app/tyrano` 或 ASAR + Tyrano 资源 | Web 字体、ASAR、Electron 桥接 | Electron 无 Tyrano 资源 |
| Unity | `UnityPlayer` + Mono 或 `GameAssembly`/IL2CPP | Mono/IL2CPP TMP 与 UI.Text | 运行时导出缺失 |
| YU-RIS | `ysbin\\yscfg.ybn` + `yu-ris1` | YPF profile、WebP/PNG/YDG | profile 不完整或非字体图片 |

表中每一行只列身份和能力的最小组合；完整字段、命令和预期日志位于对应 README 的
“证据与复刻”章节及 [功能证据与复刻流程](../../../../docs/reproduction.md)。

## 文档规范

每个引擎 README 维护以下内容：职责、检测条件、文件入口、功能、配置、运行约束、
证据来源、复刻步骤和验证矩阵。
代码标识符、路径、配置键和运行时名称保持源码拼写。文档描述当前实现和稳定接口，排障
过程统一记录在 [诊断文档](../../../../docs/diagnostics.md)。

## 扩展检查

扩展适配器目录时先回答以下问题：

1. 通用字体路径缺少哪一段可观察或可控制的调用？
2. 哪些证据确认引擎身份，哪些证据只确认版本能力？
3. 资源请求如何通过统一文件分派器或运行时调度入口进入适配器？
4. 配置关闭、身份不匹配、能力缺失和准备失败时如何保持原行为？
5. 哪些工作属于高开销准备，哪些状态可以在高频路径有界读取？
6. 当前版本资源如何创建、发布、失效和释放？

## 统一复刻记录

每个适配器至少保存一条正向样本、一条相似非目标样本、一条能力缺失样本和一条边界输入。
记录以下字段：样本 SHA-256、架构、配置快照、身份标记、能力入口、原始请求、规范化路由、
`ConfigVersion`、日志 `decision`/`fallback` 和预期产物。文件、导出、运行时接口和资源
解析证据按 [功能证据与复刻流程](../../../../docs/reproduction.md) 的 `O/I/U` 等级标注。

| 结果类型 | 身份 | 能力 | 路由 | 预期行为 |
| --- | --- | --- | --- | --- |
| 正向 | 通过 | 通过 | 命中 | 使用适配器资源或对象 |
| 非目标 | 不通过 | 不评估 | 透传 | 保持 `org*` API 语义 |
| 能力缺失 | 通过 | 不通过 | 记录回退 | 不执行专用资源处理 |
| 边界 | 通过 | 通过 | 空/长/未知请求 | 有界处理并保留错误码 |
