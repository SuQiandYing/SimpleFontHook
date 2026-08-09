# BGI 字体兼容适配器

## 职责

本目录维护两个彼此独立的 BGI 字体兼容层：

- LINE 脚本字体层：在 BGI 解码归档内 DSC 资源后，识别 `scrdrv._bp` 中“固定字体查表并
  复制 `FontRecord`”的语义，把它改为复制游戏当前选择的字体记录。
- Win32 GDI 栅格层：修补目标模块 IAT、归一化替换字体度量，并刷新已验证布局的字形缓存。

两层只共享 BGI 身份结果和 `EnableBgiHook` 总开关。LINE 能力、GDI 导入能力和缓存布局
分别确认，任一能力缺失不会关闭另一条可用路径。文本代码页仍由通用编码模块管理。

## 支持范围

| 能力 | Win32 | x64 |
| --- | --- | --- |
| DSC 解码后 LINE 字体脚本适配 | 支持 fastcall 与旧 ESI/EDI 解码器 | 支持 Win64 fastcall 解码器 |
| BGI 专用 GDI IAT、度量与缓存适配 | 支持 | 不激活 |
| 通用 GDI 字体替换 | 支持 | 支持 |

LINE 层按引擎解码器 ABI 和 BP 字节码布局识别版本，不检查游戏名、可执行文件名或固定 RVA。

## 文件结构

- `bgi_compat.cppinc`：命名空间与实现聚合入口。
- `bgi_identity.cppinc`：架构无关的 BGI 身份确认和结果缓存。
- `bgi_line_font.cppinc`：DSC 解码器能力检测、x86/x64 挂接、BP 语义匹配与等长补丁。
- `bgi_compat_state.cppinc`：Win32 导入、缓存布局、实例版本和度量状态。
- `bgi_import_detection.cppinc`：Win32 PE 导入表检查和栅格化流程特征。
- `bgi_cache_detection.cppinc`：在有界代码范围内定位 Win32 缓存访问布局。
- `bgi_cache_hooks.cppinc`：缓存函数钩子、版本观察和 GDI 导入修补。
- `bgi_font_metrics.cppinc`：字体探测、宽高比例和垂直度量辅助函数。
- `bgi_runtime.cppinc`：独立能力安装、度量归一化、文本输出作用域和配置刷新接口。

## 激活流程

1. 检查默认开启的 `EnableBgiHook`。
2. 由 `Buriko General Interpreter`、`BURIKO ARC` 模块标记或 BURIKO 归档头确认身份。
3. LINE 层检查映像中的 `DSC FORMAT 1.00`，再唯一匹配当前架构的解码器函数结构和内部
   `call` 目标范围。
4. LINE 层在现有 Detours 事务中挂接已确认的解码器；x86 fastcall、x86 旧寄存器 ABI 和
   x64 fastcall 使用各自入口。
5. Win32 GDI 层独立解析 PE 导入并确认栅格能力，再按配置挂接缓存布局和修补 IAT。
6. LINE 活动状态与 Win32 `g_active` 分开维护，不用 GDI 能力替代脚本能力。

`.arc` 后缀、通用 GDI 导入和单个资源名都不是身份依据。归档魔数只在安装阶段读取，
解码钩子不会扫描磁盘。

## LINE 字体脚本适配

原始 LINE 初始化逻辑按字体名称查表，再把对应的完整 `FontRecord` 复制到 LINE 专用记录。
因此只替换普通设置界面的字体或 GDI 对象时，LINE 仍可能保留脚本写死的字体记录。

适配器在原 DSC 解码器成功返回后处理输出缓冲区：

1. 验证 BP 头大小、正文大小和保留字段，限制脚本最大扫描长度。
2. 匹配“字体名称查表、索引落入局部槽、按 524 字节步长复制 `FontRecord`、初始化 LINE
   样式”的完整指令序列。
3. 验证 `push_string` 指向缓冲区内非空、语法有效的 CP932 字体名；字体名内容不参与
   profile 选择。
4. 要求所有已知布局合计只有一个语义匹配，再用等长指令读取该布局的当前字体索引。
5. 保持脚本长度、后续指令地址、分支标签、字符串区和归档内容不变。

当前布局覆盖两代已验证的 BGI `FontRecord` 全局表：

| BP 布局 | 字体表 | 当前字体索引 | LINE 记录 |
| --- | --- | --- | --- |
| `bp-modern-font-record-0c00` | `0x80000C00` | `0x80000C0C` | `0x520` |
| `bp-legacy-font-record-0bd4` | `0x80000BD4` | `0x80000BDC` | `0x51C` |

这些值是完整字节码布局的一部分，不作为单独签名使用。未知布局、重复匹配、无效字符串
引用或解码失败均保留原缓冲区和原返回值。

### 归档边界

运行时处理的是引擎从 `sysprg.arc` 解码到内存的 BP 内容。游戏目录不需要、也不应要求
存在解包后的 `scrdrv._bp`；松散文件、开发者分析目录和测试夹具均不参与检测或挂接。

## Win32 GDI 栅格适配

Win32 路径确认字体创建、选择、栅格输出和辅助 GDI 导入后，把目标模块 IAT 接到已安装
的通用字体钩子。度量模块测量源字体和替换字体的墨迹、总宽度与 `TEXTMETRIC`，在有限
候选范围归一化宽高，并按 LOGFONT 与 `ConfigVersion` 缓存结果。

字形缓存只在布局和实例均验证后按渲染线程观察到的配置版本刷新。探测字体通过
`ScopedInternalFontProbe` 旁路替换，保持源字体和目标字体可区分。

## 配置

- `EnableBgiHook`：BGI 总开关，默认开启；同时控制 LINE 层与 Win32 GDI 层安装。
- `BgiPatchGdiImports`：只控制 Win32 GDI IAT 修补。
- `BgiClearGlyphCacheOnSwitch`：只控制 Win32 已验证字形缓存的版本刷新。

LINE 适配是 `EnableBgiHook` 下的自动能力，不增加按游戏配置，也不把它绑定到代码页选项。

## 日志

LINE 层使用结构化 `[DEBUG][BGI]` 日志：

- `phase=line-font-capability decision=attached evidence=x64-fastcall-dsc`：解码器 profile
  唯一且 Detours 挂接成功。
- `phase=line-font-capability decision=skip ... fallback=original-script`：DSC 或解码器能力
  不完整，保留原脚本。
- `phase=line-font-apply decision=patched evidence=bp-... size=... offset=...`：解码后的 BP
  唯一命中并完成等长补丁。
- `phase=line-font-apply decision=skip evidence=multiple-layout-matches`：输入歧义，保持原内容。

成功与歧义日志都有进程内上限，解码其他 DSC/BP 资源不会产生无界 miss 日志。

## 不变量

- 不依赖外置或预先解包的 `scrdrv._bp`。
- 不用游戏名、路径、固定 RVA、单个字体名或单个字节偏移识别能力。
- 身份、DSC 解码器能力、BP 语义路由和 Win32 GDI 能力保持独立阶段。
- 解码器签名必须在可执行节唯一命中，内部相对 `call` 必须落在可执行映像范围。
- BP 修改必须通过头部、边界、字符串引用、完整指令序列和唯一性检查，并保持等长。
- 未知架构、未知 ABI、未知 BP 布局和任何边界失败都保留原函数返回值与输出。
- 本适配器不解码 ANSI 文本、不强制 CP932，也不改变文字映射代码页。
- 高频解码钩子不访问磁盘、不解析归档目录、不分配无界缓存且不输出无界日志。

## 证据与复刻

1. 对主模块记录架构、SHA-256、映像节和 `DSC FORMAT 1.00`，分别统计每个解码器 profile
   的匹配数；预期只允许一个 profile 唯一命中。
2. 对 DSC 输出验证 BP 头与正文长度，记录语义布局、匹配数、补丁偏移和补丁前后大小。
3. 用 BP 反汇编器确认替换后读取当前字体索引，且下一条 `FontRecord` 复制指令地址不变。
4. 运行时同时保存 capability 与 apply 日志；没有 apply 日志时不能仅凭画面推断已路由。
5. 对只有 BGI 身份但没有已知解码器、只有解码器但没有 LINE 布局、重复布局和损坏 BP
   分别验证原内容回退。

| 场景 | 身份 | LINE 能力 | BP 路由 | 预期结果 |
| --- | --- | --- | --- | --- |
| BGI + 唯一 x86/x64 DSC profile + 已知 LINE 布局 | 通过 | 通过 | 唯一命中 | 等长改为当前字体记录 |
| BGI + 唯一 DSC profile + 普通 BP | 通过 | 通过 | 不命中 | 原输出不变 |
| BGI + 未知或重复 DSC profile | 通过 | 不通过 | 不执行 | 原解码器与脚本 |
| BGI + 已知 profile + 重复/损坏 LINE 布局 | 通过 | 通过 | 拒绝 | 原输出不变并有限记录 |
| 只有 GDI 导入或 DSC 字符串 | 不通过 | 不执行 | 不执行 | 不安装 BGI 专用钩子 |

通用证据字段和记录格式见 [功能证据与复刻流程](../../../../../docs/reproduction.md)。

