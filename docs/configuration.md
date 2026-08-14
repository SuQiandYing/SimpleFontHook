# 配置说明

`FontHook.ini` 位于 `winmm.dll` 同级目录。字体选择器保存配置时使用带 BOM 的 UTF-8，
加载器忽略空行、以 `;` 或 `#` 开头的注释以及节标题。当前配置保存在 `[FontHook]`
节中，但解析时以键名为准。

`FontNameW` 是有效配置的必需键。缺少该键时，配置文件不会应用。

配置实现位于 `SimpleFontHook/utils.cpp`：默认值在 `Config` 命名空间中声明，
`Utils::SaveConfig` 负责按 `[FontHook]` 节写入，`Utils::LoadConfig` 负责解析和边界归一化。
解析器使用紧凑的线性键值表按键名查找；节名不参与查找，重复键保持最后一个值。布尔值
使用整数解析，`0` 表示关闭，非 `0` 表示开启。未出现的键使用源码默认值。

## 基础字体设置

| 配置键 | 默认值 | 说明 |
| --- | ---: | --- |
| `FontNameW` | `galgame` | Unicode 字体名称；空值会关闭字体和字体名称替换 |
| `FontNameA` | `galgame` | ANSI 字体名称；缺失时由 `FontNameW` 转换 |
| `EnableFontHook` | `0` | 启用字体钩子总开关 |
| `EnableFaceNameReplace` | `0` | 替换字体名称 |
| `PickerShowOnStartup` | `1` | 启动时显示字体选择器 |
| `EnableCharsetReplace` | `0` | 替换字体请求中的字符集 |
| `ForcedCharset` | `1` | 目标字符集，`1` 为 `DEFAULT_CHARSET` |

常用东亚字符集值：`128` 为 Shift-JIS，`129` 为韩文，`134` 为 GB2312，
`136` 为 Big5。

### 进程内字体文件状态

`FontFileName` 不是 INI 键。它的初始值为空；`LoadCustomFont` 消费非空值，并可在本地
字体候选匹配后写回实际文件名。CatSystem2、Artemis、Mirai、Ren'Py 和通用字体文件入口
读取该字段；`SaveConfig` 与 `LoadConfig` 不持久化它。

## 字体度量

每项度量均由独立开关控制。数值存在但开关为 `0` 时不应用。

| 开关 | 数值键 | 默认值 | 说明 |
| --- | --- | ---: | --- |
| `EnableFontHeightScale` | `FontHeightScale1000` | `1000` | 高度倍率，1000 表示 1.0 |
| `EnableFontWidthScale` | `FontWidthScale1000` | `1000` | 宽度倍率，1000 表示 1.0 |
| `EnableFontCharSpacing` | `FontCharSpacing` | `0` | 额外字符间距 |
| `EnableFontVerticalMetrics` | `FontAscentPermille` | `880` | 上升部，占 em 的千分比 |
| `EnableFontVerticalMetrics` | `FontDescentPermille` | `-120` | 下降部，占 em 的千分比 |
| `EnableFontLineSpacing` | `FontLineSpacing` | `0` | 额外行距 |
| `EnableFontWeight` | `FontWeight` | `400` | 字重 |

## 字符集与代码页

| 配置键 | 默认值 | 说明 |
| --- | ---: | --- |
| `EnableCodepageSpoof` | `0` | 启用字符集能力伪装 |
| `EnableCodepageRuntimeReplace` | `0`（派生） | 记录运行时替换状态，由伪装开关及源、目标字符集计算 |
| `SpoofFromCharset` | `134` | 需要伪装的源字符集 |
| `SpoofToCharset` | `128` | 对外暴露的目标字符集 |
| `EnableCodepageRedirect` | `0` | 启用代码页转换重定向 |
| `CodepageRedirectFrom` | `932` | 被重定向的源代码页 |
| `CodepageRedirectTo` | `65001` | 实际转换代码页，65001 为 UTF-8 |

字符集伪装启用且源、目标字符集不同时，运行时字符集替换自动生效；两者相同时只修改
字体能力标识。界面将 `EnableCodepageRuntimeReplace` 作为派生状态，模式由配置自动确定。
字符集伪装、代码页重定向和字体替换相互独立。字体替换只影响字体对象与字体数据。
解析器接受兼容别名 `Enable`、`CodePageConvertEnable`、`FromCodePage` 和
`ToCodePage`；上表键名是配置文件的规范写法。

别名的优先级为规范键、`CodePageConvertEnable`、由 `Enable` 与兼容键组合推导的值；
代码页数值按 `atoi` 读取。负的文字映射代码页回退为 `932`，零值使用系统 ACP；无效的
`YurisAtlasCodepage` 归一化为 `0`。空的 `FontNameW` 同时关闭 `EnableFontHook` 和
`EnableFaceNameReplace`。

## 文字映射

| 配置键 | 默认值 | 说明 |
| --- | ---: | --- |
| `EnableTextSubstitution` | `0` | 启用文字映射 |
| `TextSubstitutionMode` | `0` | `0` 日文旧字形映射，`1` 繁转简，`2` 简转繁 |
| `TextSubstitutionCodepage` | `932` | 无法检测原始字符集时的 ANSI 解码回退值 |

程序通过原始字体创建请求检测到明确字符集时，文字映射优先使用该字符集对应的代码页，
例如 `GB2312_CHARSET` 使用 CP936。替换字体和 `ForcedCharset` 不决定源文本编码。

## 通用兼容开关

| 配置键 | 默认值 | 说明 |
| --- | ---: | --- |
| `CompatSkipDrawTextA` | `1` | 跳过当前兼容策略未覆盖的 `DrawTextA` 替换路径 |
| `CompatSkipFontDataQueries` | `1` | 默认跳过部分字体数据查询替换 |
| `CompatSelectObjectTrackedOnly` | `0` | 只处理已跟踪字体的 `SelectObject` |
| `CompatHookCreateFontW` | `1` | 钩住 `CreateFontW` |
| `CompatHookCreateFontIndirectW` | `1` | 钩住 `CreateFontIndirectW` |
| `CompatHookGetTextFace` | `0` | 替换 `GetTextFace` 查询结果 |

## 引擎开关

引擎总开关默认开启，但只有正向检测匹配后才应执行适配逻辑。

| 引擎 | 配置键及默认值 |
| --- | --- |
| [BGI](../SimpleFontHook/hooks/internal/engines/bgi/README.md) | `EnableBgiHook=1`、`BgiPatchGdiImports=1`、`BgiClearGlyphCacheOnSwitch=1` |
| [Artemis](../SimpleFontHook/hooks/internal/engines/artemis/README.md) | `EnableArtemisHook=1`、`ArtemisPatchTables=1`、`ArtemisRedirectFontFiles=1`、`ArtemisClearFontCacheOnSwitch=1` |
| [Artemis Legacy](../SimpleFontHook/hooks/internal/engines/artemis_legacy/README.md) | 共享 Artemis 开关、字体路径、字号、间距和文字映射配置 |
| [KiriKiri](../SimpleFontHook/hooks/internal/engines/krkr/README.md) | `EnableKrkrHook=1`、`KrkrDisablePrerenderedFonts=1` |
| [Entis](../SimpleFontHook/hooks/internal/engines/entis/README.md) | `EnableEntisHook=1`、`EntisDisableBitmapFonts=1`、`EntisRefreshFontOnSwitch=1` |
| [Softpal](../SimpleFontHook/hooks/internal/engines/softpal/README.md) | `EnableSoftpalHook=1`、`SoftpalDisableDefaultFontDat=1`、`SoftpalForceDefaultOptionToSystemFont=1` |
| [Escude](../SimpleFontHook/hooks/internal/engines/escude/README.md) | `EnableEscudeHook=1`、`EscudeVirtualFontConfig=1` |
| [Mirai](../SimpleFontHook/hooks/internal/engines/mirai/README.md) | `EnableMiraiHook=1`、`MiraiReplaceFontDataQueries=1`、`MiraiRedirectFontFiles=1`、`MiraiPinFontDataSource=1` |
| [Majiro](../SimpleFontHook/hooks/internal/engines/majiro/README.md) | `EnableMajiroHook=1`、`MajiroDisableFontCache=1` |
| [DxLib](../SimpleFontHook/hooks/internal/engines/dxlib/README.md) | `EnableDxLibHook=1`、`DxLibDisableFontCache=0`、`DxLibReplaceFontDataQueries=1`、`DxLibClearRuntimeFontCacheOnSwitch=0` |
| [TinkerBell](../SimpleFontHook/hooks/internal/engines/tinkerbell/README.md) | `EnableTinkerBellHook=1` |
| [classic CatSystem2](../SimpleFontHook/hooks/internal/engines/catsystem2/README.md) | 使用 `EnableFontHook`、`EnableFaceNameReplace`、`FontNameW` 和进程内 `FontFileName`；无引擎私有配置 |
| [Unity](../SimpleFontHook/hooks/internal/engines/unity/README.md) | `EnableCatSystemUnityHook=1` |
| [TyranoScript](../SimpleFontHook/hooks/internal/engines/tyrano/README.md) | `EnableTyranoHook=1`、`TyranoRedirectWebFonts=1` |
| [Ren'Py](../SimpleFontHook/hooks/internal/engines/renpy/README.md) | `EnableRenPyHook=1`、`RenPyRedirectFonts=1`、`RenPyRefreshFontOnSwitch=1` |
| [YU-RIS](../SimpleFontHook/hooks/internal/engines/yuris/README.md) | `YurisAtlasCodepage=0`（INI 诊断覆盖项）；使用 `EnableFontHook`、`FontNameW` 和通用度量配置 |

BGI 的 `EnableBgiHook` 同时控制 x86/x64 DSC 解码后的 LINE 字体脚本层和 Win32 GDI
栅格层；`BgiPatchGdiImports`、`BgiClearGlyphCacheOnSwitch` 只作用于 Win32 GDI 路径。
LINE 脚本能力默认自动检测，不要求单独配置或外置 `scrdrv._bp`。

Artemis 还保存 `ArtemisFontPath`、`ArtemisFontSize=0` 和 `ArtemisRubySize=-1`。
DxLib 的 `DxLibCachedFontNameW` 是兼容缓存状态；读取该键会填充进程内缓存，缺少该键时
清空缓存。
YU-RIS 的资源配置与 `ForcedCharset` 相互独立；例如转区运行时可使用
`FontNameW=MSMSMS` 和 `ForcedCharset=128`，字符集值不会改变位图页的资源索引。
适配器根据完整 YPF 契约选择 `45 页 × 8 样式`、`127 页 × 2 样式`、
`89 页 c20 + 38 页 c10`，或 `4 × (89 页 Big5 + 1 页 ASCII)` 布局；归档目录名编码
自动探测，页表代码页按槽位结构筛选。适配器枚举 `pac/*.ypf`，按内部路径、完整字体页
集合和图片布局确认能力，不限定归档文件名或归档总条目数；解包目录不参与运行时识别。
`YurisAtlasCodepage` 由 `FontHook.ini` 管理，字体选择器仅呈现跨引擎的字体、文字映射和
代码页重定向配置。值 `0` 使用自动结果，非零值可选择已安装的 Windows 代码页；不兼容
当前页数和槽位的值保持原始解码结果。该键不复用字符集伪装、系统 ACP、转码或文字映射状态。

## 调试设置

| 配置键 | 默认值 | 说明 |
| --- | ---: | --- |
| `EnableDebugLog` | `0` | 启用详细日志、异常采样和卡顿监视 |
| `DebugSlowMs` | `50` | 慢调用记录阈值，单位毫秒 |
| `DebugTraceSampleLimit` | `0` | 每类高频 API 的跟踪样本上限，0 表示关闭 |
| `DebugPickerThreadLogLimit` | `0` | 字体选择器线程日志上限，0 表示关闭 |

排障结束后应恢复 `EnableDebugLog=0` 和两个采样上限为 `0`，避免高频日志影响性能。

## 运行时状态

`DetectedCharset`、`ConfigVersion`、`NeedFontReload` 和 `SourceFontNameW` 仅存在于进程内。
`ForcedFontNameW`、`ForcedFontNameA` 和各开关由加载器写入进程状态；`FontFileName` 由
本地字体加载路径消费并在候选命中时写回，属于进程内字段。
字体选择器保存配置、递增版本并通知运行时缓存；文件中的值在下一次 `LoadConfig` 调用时
生效。`ConfigVersion` 不从 INI 读取。

## 配置来源与复刻

### 最小可运行配置

下面的文件只提供必需键和总开关，可用于建立无引擎干预的基线：

```ini
[FontHook]
FontNameW=galgame
FontNameA=galgame
EnableFontHook=1
EnableFaceNameReplace=1
EnableDebugLog=0
DebugTraceSampleLimit=0
DebugPickerThreadLogLimit=0
```

### 单变量验证配置

在基线文件上一次只改变一组相关键，并为每组记录 `ConfigVersion`：

```ini
[FontHook]
FontNameW=TARGET_FONT
FontNameA=TARGET_FONT
EnableFontHook=1
EnableFaceNameReplace=1
EnableCharsetReplace=0
EnableCodepageSpoof=0
EnableCodepageRedirect=0
EnableTextSubstitution=0
EnableDebugLog=1
DebugSlowMs=50
DebugTraceSampleLimit=128
DebugPickerThreadLogLimit=32
```

### 解析验证矩阵

| 输入 | 观测 | 判定 |
| --- | --- | --- |
| 缺少 `FontNameW` | `LoadConfig` 返回失败 | 文件不应用 |
| `FontNameW=` | 字体名为空且两个字体开关为 `0` | 字体替换路径关闭 |
| `FontNameA` 缺失 | 从 `FontNameW` 转换 ANSI 名称 | ANSI 名称可追踪 |
| `TextSubstitutionMode` 超出 `0..2` | 值归一化为 `0` | 使用日文旧字形模式 |
| 负 `DebugSlowMs`、采样上限或字体尺寸 | 值归一化为非负范围 | 日志和尺寸计算有界 |
| 无效 `YurisAtlasCodepage` | 值归一化为 `0` | 使用自动代码页选择 |
| 仅存在 `FromCodePage`/`ToCodePage` | 结合 `Enable` 推导重定向开关 | 兼容键组合 |

逐项验证时保存 `FontHook.ini`、`FontHook.trace.log` 和进程内版本号；完整的样本固定与
证据字段见 [功能证据与复刻流程](reproduction.md)。
