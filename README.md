# SimpleFontHook

SimpleFontHook 是一个面向 Windows 视觉小说/galgame 的运行时字体替换 DLL。

它被构建成 `winmm.dll` 代理：把生成的 DLL 放到游戏主程序同目录后，游戏会优先加载这个本地 `winmm.dll`。DLL 会继续把原本的 `winmm` 导出转发到系统 `C:\Windows\System32\winmm.dll`，同时在游戏进程内用 Microsoft Detours 挂钩字体、文本、编码转换、字体文件读取和若干引擎特定路径。

这个项目的目标不是做通用翻译器，而是解决游戏里常见的字体替换问题：

- 游戏硬编码了日文字体、宋体、黑体或某个不存在的字体。
- 字体枚举、字符集、字体数据查询和实际绘制结果不一致。
- 引擎使用预渲染字库、字体缓存或私有字体文件，单纯改 `CreateFont` 不生效。
- 更换字体后行距、字距、字形索引缓存、字体表信息导致显示错位或乱码。

## 快速使用

前提：目标游戏进程需要会加载 `winmm.dll`。如果游戏本身不导入或加载 `winmm.dll`，只把代理 DLL 放到目录里不会自动生效。

1. 确认游戏位数。
   - 32 位游戏使用 `Release\winmm.dll`。
   - 64 位游戏使用 `x64\Release\winmm.dll`。
2. 把对应的 `winmm.dll` 复制到游戏 `.exe` 同目录。
3. 启动游戏。首次没有配置时会弹出字体选择窗口。
4. 选择字体后点击应用，配置会写入游戏目录下的 `FontHook.ini`。
5. 之后如果窗口不自动显示，可以用热键呼出：
   - 优先注册 `Ctrl+Alt+F`
   - 如果冲突，回退到 `Ctrl+Alt+Shift+F`
   - 再冲突，回退到 `Ctrl+Alt+F8`

如果想使用游戏目录里的字体文件，把 `.ttf`、`.otf`、`.ttc` 或 `.fon` 放在游戏主程序同目录。字体选择器启动时会用私有字体方式加载这些文件并加入列表。

## 主要功能

### 字体选择器

字体选择器运行在游戏进程里的独立 UI 线程，窗口标题为 `Font Selection Assistant`。它支持：

- 搜索和筛选字体。
- 枚举系统字体和游戏目录下的本地字体文件。
- 按字符集查看字体，覆盖默认、简体中文、日文、繁体中文、韩文等常见 GDI 字符集。
- 双击或按 Enter 应用字体。
- 上下、PageUp/PageDown、Home/End 导航列表，Esc 隐藏窗口。
- A-/A+ 调整选择器自身缩放。
- 运行时调整字体高度、宽度、字距、纵向指标、行距和字重。
- 根据参考字体自动推导纵向指标。
- 切换代码页伪装、文本替换和代码页重定向选项。

设置会保存到 `FontHook.ini`。如果配置中 `PickerShowOnStartup=0`，已有配置时选择器默认隐藏，但热键仍然注册。

### GDI 字体替换

核心路径围绕 GDI 实现，覆盖游戏最常见的文本渲染方式：

- `CreateFontA/W`
- `CreateFontIndirectA/W`
- `CreateFontIndirectExA/W`
- `SelectObject`
- `GetCurrentObject`
- `TextOutA/W`
- `ExtTextOutA/W`
- `DrawTextA/W`
- `DrawTextExA/W`

启用字体替换后，Hook 会尽量把游戏创建或选入 DC 的字体替换为配置字体，并维护原始字体到替换字体的缓存，避免每次绘制重复创建字体。

### 字体查询和布局兼容

很多游戏不会只看绘制结果，还会查询字体名称、字符集、字形宽度、字形轮廓、字体表或 Unicode 范围。项目因此也挂钩了大量查询函数，例如：

- `GetTextMetricsA/W`
- `GetTextCharset`
- `GetTextCharsetInfo`
- `EnumFontFamiliesExA/W`
- `EnumFontsA/W`
- `EnumFontFamiliesA/W`
- `GetGlyphOutlineA/W`
- `GetGlyphIndicesA/W`
- `GetCharacterPlacementA/W`
- `GetCharWidth*`
- `GetCharABCWidths*`
- `GetTextExtent*`
- `GetOutlineTextMetricsA/W`
- `GetKerningPairsA/W`
- `GetFontData`
- `GetFontLanguageInfo`
- `GetFontUnicodeRanges`

`GetTextFaceA/W` 默认不启用替换，因为部分游戏会依赖真实字体名判断内部逻辑；需要时可以通过 `CompatHookGetTextFace=1` 打开。

### GDI+ 和 DirectWrite

项目包含有限的 GDI+ 与 DirectWrite 支持：

- GDI+：挂钩字体族创建和从 `LOGFONT` 创建字体的路径，按当前配置字体名做替换。
- DirectWrite：挂钩 `DWriteCreateFactory`，再处理 `IDWriteFactory::CreateTextFormat` 和 `CreateTextLayout`。目前主要用于字体族替换、部分字号/字重调整和创建布局时的文本替换。

这部分不是完整的 DirectWrite 渲染管线替换。如果游戏完全绕过 GDI/GDI+ 并自己管理字形贴图，仍然可能需要引擎兼容层配合。

### 字体表、字符集和字形缓存处理

当启用相关选项时，SimpleFontHook 会对运行时字体和返回给游戏的字体信息做补丁：

- 修改 `OS/2` 表的代码页范围，解决游戏按字体表判断字符集的问题。
- 修改 `hhea`/`OS/2` 纵向指标，缓解行高、上沿、下沿不合适的问题。
- 为文本替换场景维护虚拟字形索引，减少游戏缓存旧 glyph index 后显示错字的情况。
- 对部分字体文件或内存字体克隆改写 name/cmap/metric 信息。

字体选择器在启用代码页伪装或纵向指标补丁时，会尽量从当前字体文件创建一个内部克隆字体。内部克隆字体名形如 `SFxxxx`，保存配置时会尽量保存原始字体名，而不是这个临时名字。

### 文本与代码页处理

项目包含三类文本/编码相关能力：

- 代码页伪装：让字体或字体查询看起来属于另一个 GDI 字符集，例如把 GB2312 字体伪装成 Shift-JIS 可用字体。
- 代码页重定向：挂钩 `MultiByteToWideChar` 和 `WideCharToMultiByte`，把指定来源代码页重定向到目标代码页。
- 文本替换：内置字符映射表，支持日文兼容/繁体代理、繁转简、简转繁三种模式。

这些功能会影响进程内 API 行为，不建议无脑全开。通常先只改字体；如果字体能替换但文字乱码、缺字或游戏坚持使用日文字体，再逐项开启。

## 引擎兼容层

代码里包含针对若干常见 galgame 引擎的兼容处理。检测通过文件布局、资源标记或可执行文件内容完成，只有命中对应特征时才会走更深的路径。

### Artemis

当前 Artemis 路径支持：

- 检测 `.pfs`、`system\table` 等目录结构。
- 虚拟化 `system\table\list_windows*.tbl`。
- 修改表里的 `face`、`rubyface`、`kerning`、`spacemiddle`、`size`、`rubysize` 等字体相关字段。
- 支持从 PFS 资源或 loose file 读取表数据。
- 提供虚拟字体路径，默认类似 `FontHook.ttf`，并在配置变更时附加版本后缀以绕过缓存。
- 在 32 位 Artemis 上尝试刷新部分内部 FreeType 对象和字体 atlas；64 位路径避免不可靠的内部地址扫描。

### Artemis Legacy

旧 Artemis 路径主要处理 loose `.iet` 脚本和旧式资源：

- 修改 `.iet` 里的 `face`、`rubyface`、`font_face`、`g.font_face`、`kerning`、`spacemiddle`、`size`、`rubysize` 等字段。
- 可对脚本文本做内置文本替换。
- 隐藏 `.rft` 预渲染字体文件。
- 提供虚拟字体文件，并可对字体 name/cmap/metric/codepage 表做补丁。

旧 Artemis 没有安全的通用运行时缓存布局，因此配置变更后主要同步虚拟文件和诊断信息，不做现代 Artemis 那种对象扫描。

### KiriKiri/KRKR

KRKR 兼容层主要处理预渲染字体：

- 检测 `.xp3` 和 `font\*.tft`。
- 把主模块里的 `mapPrerenderedFont` 字符串改为 `sfhPrerenderedFont`，使引擎不再命中原预渲染路径。
- 隐藏 `.tft` 文件，迫使游戏走可替换字体路径。

### Softpal

Softpal 兼容层主要处理 `Pal.dll` 和 `DEFAULT_FONT.DAT`：

- 检测 `dll\Pal.dll` 和 `.pac` 资源。
- 挂钩 `PalFontBegin`、`PalFontSetType`、`PalFontGetType`。
- 可隐藏 `DEFAULT_FONT.DAT`。
- 可把默认字体选项映射到系统字体路径。
- 在替换字体构造时保留 Softpal 更依赖的自然宽度行为。

### Escu:de

Escu:de 兼容层处理配置文件里的字体项：

- 检测 `configure.cfg` 里的 `[General] Company=ESCUDE`。
- 虚拟化 `[Font]` 下的 `Face` 或 `Font`。
- 同时覆盖游戏目录和 `Documents\ESCUDE\<Product>\configure.cfg` 的读取。

### Mirai

Mirai 兼容层围绕 FreeType 和字体文件读取：

- 检测 `arc0.dat`、`script.dat`、`Setting.exe` 及可执行文件中的 FreeType/字体配置标记。
- 可替换 `GetFontData` 返回的数据源。
- 可把游戏读取的 Windows 字体文件重定向到当前选择字体。
- 可在配置变更时固定一份字体数据源，减少运行时切换导致的查询不一致。

### Majiro

Majiro 兼容层主要处理字体缓存：

- 检测 `.arc` 和 Majiro 相关可执行文件标记。
- 可隐藏或绕过 `savedata` 下的字体缓存文件。
- 配置变更时尝试刷新运行时字体缓存和脏标记。
- 对部分 A 系文本路径提供 CP932 fallback。

### DxLib

DxLib 兼容层处理 DxLib 字体缓存和字体数据查询：

- 检测可执行文件里的 `DxLib`、`GetGlyphOutlineA`、`CreateFontToHandle` 等标记。
- 可替换 `GetFontData` 查询结果。
- 可按配置隐藏或绕过 `_FONTSET.MED` 等字体缓存文件。
- 可记录缓存字体名。
- 可选运行时清理字体缓存；默认关闭，因为这类内部缓存布局更容易随游戏版本变化。

### TinkerBell/Cyberworks

TinkerBell 兼容层偏保守：

- 检测 `Arc00.dat`、`Arc01.dat`、`render.dll` 和可执行文件中的 TinkerBell/Cyberworks 标记。
- 命中后会跳过部分宽字符字体创建 Hook。
- 对未被 SimpleFontHook 跟踪的 `SelectObject` 字体句柄尽量放行，减少引擎内部字体对象被误替换。

## 配置文件

配置文件路径固定为游戏主程序同目录的 `FontHook.ini`，保存时使用 UTF-8 BOM。读取配置时只要求存在 `FontNameW`；没有配置时默认弹出字体选择器。

常见字符集数值：

| 名称 | 数值 |
| --- | ---: |
| `ANSI_CHARSET` | `0` |
| `DEFAULT_CHARSET` | `1` |
| `SHIFTJIS_CHARSET` | `128` |
| `HANGUL_CHARSET` | `129` |
| `GB2312_CHARSET` | `134` |
| `CHINESEBIG5_CHARSET` | `136` |

文本替换模式：

| `TextSubstitutionMode` | 含义 |
| ---: | --- |
| `0` | 日繁映射中文 |
| `1` | 繁体中文转简体中文 |
| `2` | 简体中文转繁体中文 |

下面是当前代码会保存和读取的主要字段示例：

```ini
[FontHook]
FontNameW=Microsoft YaHei
FontNameA=Microsoft YaHei
EnableFontHook=1
EnableFaceNameReplace=1
EnableCharsetReplace=1
ForcedCharset=134

EnableCodepageSpoof=0
EnableCodepageRuntimeReplace=0
SpoofFromCharset=134
SpoofToCharset=128

EnableCodepageRedirect=0
CodepageRedirectFrom=932
CodepageRedirectTo=65001

EnableTextSubstitution=0
TextSubstitutionMode=0
TextSubstitutionCodepage=932

PickerShowOnStartup=1
EnableDebugLog=0
DebugSlowMs=50
DebugTraceSampleLimit=0
DebugPickerThreadLogLimit=0

CompatSkipDrawTextA=1
CompatSkipFontDataQueries=1
CompatSelectObjectTrackedOnly=0
CompatHookCreateFontW=1
CompatHookCreateFontIndirectW=1
CompatHookGetTextFace=0

EnableArtemisHook=1
ArtemisPatchTables=1
ArtemisRedirectFontFiles=1
ArtemisClearFontCacheOnSwitch=1
ArtemisFontPath=
ArtemisFontSize=0
ArtemisRubySize=-1

EnableKrkrHook=1
KrkrDisablePrerenderedFonts=1

EnableSoftpalHook=1
SoftpalDisableDefaultFontDat=1
SoftpalForceDefaultOptionToSystemFont=1

EnableEscudeHook=1
EscudeVirtualFontConfig=1

EnableMiraiHook=1
MiraiReplaceFontDataQueries=1
MiraiRedirectFontFiles=1
MiraiPinFontDataSource=1

EnableMajiroHook=1
MajiroDisableFontCache=1

EnableDxLibHook=1
DxLibDisableFontCache=0
DxLibReplaceFontDataQueries=1
DxLibClearRuntimeFontCacheOnSwitch=0
DxLibCachedFontNameW=

EnableTinkerBellHook=1

EnableFontHeightScale=0
EnableFontWidthScale=0
EnableFontCharSpacing=0
EnableFontVerticalMetrics=0
EnableFontLineSpacing=0
EnableFontWeight=0
FontHeightScale1000=1000
FontWidthScale1000=1000
FontCharSpacing=0
FontAscentPermille=880
FontDescentPermille=-120
FontLineSpacing=0
FontWeight=400
```

说明：

- `FontNameW` 是主要字体名，选择器应用字体时会更新它。
- `FontNameA` 用于 ANSI 路径的往返保存，通常保持和 `FontNameW` 一致即可。
- `EnableFontHook=1` 才会启用核心替换。没有字体名时，代码会强制关闭字体替换。
- `EnableFaceNameReplace=1` 替换字体名。
- `EnableCharsetReplace=1` 替换 `LOGFONT` 字符集。
- `ForcedCharset` 是替换后的 GDI 字符集。
- `FontHeightScale1000=1000` 表示 100%，`1200` 表示 120%。
- `FontWidthScale1000=1000` 表示 100%，`800` 表示 80%。
- `FontAscentPermille`、`FontDescentPermille` 和 `FontLineSpacing` 用于纵向指标补丁，选择器里的自动按钮会尝试从参考字体推导。
- `DebugTraceSampleLimit=0` 表示关闭受采样限制的详细 trace；大于 `0` 时才记录对应的有限样本。

注意：源码中存在内部变量 `Config::FontFileName`，但当前 `FontHook.ini` 的读写逻辑没有把 `FontFile` 作为配置项保存或读取。需要加载本地字体时，推荐把字体文件放到游戏目录，让字体选择器自动扫描并加载。

## 诊断日志

项目会在游戏目录写入诊断日志：

- `FontHook.trace.log`

普通 `Log` 输出走 `OutputDebugString`，需要调试器或 DebugView 这类工具查看。

常用排查方法：

- 游戏没反应：先确认 DLL 位数和游戏位数一致。
- 选择器没出现：尝试热键 `Ctrl+Alt+F`、`Ctrl+Alt+Shift+F`、`Ctrl+Alt+F8`，并检查日志是否加载成功。
- 字体能换但乱码：先尝试切换字符集，再考虑代码页伪装、文本替换或代码页重定向。
- 字体能换但行距错：使用选择器里的高度、宽度、字距、纵向指标和行距调节。
- 字体换了但部分文本没变：游戏可能使用预渲染字库、贴图字库、引擎缓存或 DirectWrite/FreeType 私有管线，检查是否命中了对应引擎兼容层。
- 游戏崩溃或卡死：关闭更激进的兼容项，例如运行时缓存清理、代码页重定向、`GetTextFace` 替换，保留最小字体替换配置再逐项打开。

诊断模块还包含：

- 首次机会异常和未处理异常记录。
- 关键阶段 watchdog。
- 配置变更后的引擎状态 trace。
- 字体切换后窗口响应状态检查。

## 编译

需要：

- Visual Studio 2022 或兼容 MSVC v143 工具链。
- Windows SDK。
- MSBuild。
- 仓库根目录下的 Detours 头文件和库文件：
  - `detours.h`
  - `detours.lib`
  - `detours_x64.lib`

构建 32 位：

```bat
build_x32.bat
```

构建 64 位：

```bat
build_x64.bat
```

脚本会通过 `vswhere` 查找 Visual Studio，再调用对应的 `vcvars32.bat` 或 `vcvars64.bat`，最后用 MSBuild 重新构建 Release 配置。

也可以手动构建：

```bat
msbuild SimpleFontHook.sln /p:Configuration=Release /p:Platform=Win32 /t:Rebuild
msbuild SimpleFontHook.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild
```

输出文件：

- `Release\winmm.dll`
- `x64\Release\winmm.dll`

## 项目结构

```text
SimpleFontHook.sln
SimpleFontHook\
  dllmain.cpp
  framework.h
  utils.cpp
  winmm_proxy.h
  font\
    font_patcher.*
  hooks\
    font_hooks.cpp
    hook_policy.*
    internal\
      font_hooks_*.cppinc
      model\
      queries\
      engines\
  ui\
    font_picker.cpp
    internal\
build_x32.bat
build_x64.bat
detours.h
detours.lib
detours_x64.lib
```

关键文件：

- `dllmain.cpp`：进程附加入口，安装诊断和 Hook。
- `winmm_proxy.h`：转发系统 `winmm.dll` 导出。
- `utils.cpp`：配置读写、日志、诊断、字体加载辅助。
- `font/font_patcher.*`：解析和修改 TTF/TTC/OTF 相关表。
- `hooks/font_hooks.cpp`：聚合所有 Hook 实现。
- `hooks/hook_policy.*`：集中管理兼容策略和是否安装某些高风险 Hook。
- `hooks/internal/model`：字体替换缓存、指标调整、代码页伪装、虚拟 glyph index。
- `hooks/internal/queries`：字体枚举、字体身份、字体数据和布局查询 Hook。
- `hooks/internal/engines`：各引擎兼容层。
- `ui/font_picker.cpp`：字体选择器入口。

## 使用边界

- 必须匹配游戏进程位数。32 位 DLL 不能加载到 64 位游戏，反之亦然。
- 这是进程内 Hook DLL，不适合带反作弊、联网对战或不允许注入的程序。
- 对已经完全渲染成图片的文字无能为力，除非对应引擎兼容层能阻止它使用预渲染缓存。
- 对自研引擎、强混淆引擎或深度定制字体缓存，可能只能覆盖一部分文本路径。
- 代码页重定向和文本替换会影响整个进程内相关 API，应按游戏逐项验证。

## 建议调试顺序

1. 只复制对应位数的 `winmm.dll`，启动游戏，看选择器和日志是否出现。
2. 只选择字体，不开启代码页伪装、文本替换或代码页重定向。
3. 如果字体列表里没有目标字体，把字体文件放到游戏目录后重启。
4. 如果游戏仍使用旧字体，检查是否需要引擎兼容层或是否被预渲染缓存挡住。
5. 如果缺字或乱码，再尝试字符集替换、代码页伪装、文本替换。
6. 如果布局不对，再调整高度、宽度、字距、纵向指标、行距和字重。
7. 每次只改一个方向，并保留 `FontHook.trace.log` 方便回退和定位。
