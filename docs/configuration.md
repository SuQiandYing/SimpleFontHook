# 配置说明

`FontHook.ini` 位于 `winmm.dll` 同级目录。字体选择器保存配置时使用带 BOM 的 UTF-8，
加载器忽略空行、以 `;` 或 `#` 开头的注释以及节标题。当前配置保存在 `[FontHook]`
节中，但解析时以键名为准。

`FontNameW` 是有效配置的必需键。缺少该键时，配置文件不会应用。

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
| `EnableCodepageRuntimeReplace` | `0` | 启用运行时字符集替换路径 |
| `SpoofFromCharset` | `134` | 需要伪装的源字符集 |
| `SpoofToCharset` | `128` | 对外暴露的目标字符集 |
| `EnableCodepageRedirect` | `0` | 启用代码页转换重定向 |
| `CodepageRedirectFrom` | `932` | 被重定向的源代码页 |
| `CodepageRedirectTo` | `65001` | 实际转换代码页，65001 为 UTF-8 |

字符集伪装、代码页重定向和字体替换相互独立。字体替换只影响字体对象与字体数据。
解析器接受兼容别名 `Enable`、`CodePageConvertEnable`、`FromCodePage` 和
`ToCodePage`；上表键名是配置文件的规范写法。

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
| `CompatSkipDrawTextA` | `1` | 跳过风险较高的 `DrawTextA` 替换路径 |
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
| [Unity](../SimpleFontHook/hooks/internal/engines/unity/README.md) | `EnableCatSystemUnityHook=1` |
| [TyranoScript](../SimpleFontHook/hooks/internal/engines/tyrano/README.md) | `EnableTyranoHook=1`、`TyranoRedirectWebFonts=1` |
| [Ren'Py](../SimpleFontHook/hooks/internal/engines/renpy/README.md) | `EnableRenPyHook=1`、`RenPyRedirectFonts=1`、`RenPyRefreshFontOnSwitch=1` |

Artemis 还保存 `ArtemisFontPath`、`ArtemisFontSize=0` 和 `ArtemisRubySize=-1`。
DxLib 的 `DxLibCachedFontNameW` 是兼容缓存状态，不建议手工修改。

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
字体选择器负责保存配置、递增版本并通知所有运行时缓存；手工编辑的值在下次加载时生效。
