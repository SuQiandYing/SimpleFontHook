# SimpleFontHook

SimpleFontHook 是一个面向 Windows 视觉小说/galgame 的字体替换与兼容性 DLL。它通过 `winmm.dll` 代理加载进入游戏进程，拦截 GDI/GDI+/部分 DirectWrite 字体相关 API，让旧游戏可以在运行时切换系统字体或本地字体，并处理常见的日文引擎字形、字体缓存、预渲染字体和编码问题。

## 主要功能

- 游戏内字体选择器：搜索系统字体和同目录本地字体，预览后双击或回车应用。
- 字体替换：替换 `CreateFont*` / `CreateFontIndirect*` 创建出来的字体句柄，并跟踪 `SelectObject` 后的 HDC 字体状态。
- 文本绘制兼容：覆盖 `TextOut`、`ExtTextOut`、`DrawText`、`GetGlyphOutline`、`GetTextExtent*`、`GetCharWidth*`、`GetCharABCWidths*`、`GetGlyphIndices*` 等常见 GDI 文本路径。
- 字体参数微调：字宽、字高、字距、上行、下行、行距、粗细。
- 字体表修补：可修补字体 `OS/2` 代码页范围、垂直度量、name 表，以及部分 Unicode `cmap` 映射。
- 文本映射：支持日文旧字体/异体字到繁体字、繁简互转、指定代码页文本替换。
- 代码页重定向：拦截 `MultiByteToWideChar` / `WideCharToMultiByte`，用于处理硬编码 CP932、UTF-8 补丁等场景。
- 引擎兼容层：针对 Artemis、旧版 Artemis、KiriKiri、Softpal、Escu:de、Mirai、Majiro、DxLib 做了专门处理。
- 调试和诊断：输出 `FontHook.trace.log`，包含启动、热切换、字体数据、引擎兼容层、崩溃和卡死诊断信息。

## 快速使用

1. 下载或编译得到 `winmm.dll`。
2. 确认游戏位数：
   - 绝大多数旧 galgame 是 32 位程序，使用 `Release/winmm.dll`。
   - 64 位游戏才使用 `x64/Release/winmm.dll`。
3. 把 `winmm.dll` 复制到游戏主程序 `.exe` 同目录。
4. 如果想让工具加载本地字体，把 `.ttf` / `.otf` / `.ttc` 放到同目录。
5. 启动游戏。字体选择器会弹出，选择字体后双击或按回车应用。
6. 之后配置会写入游戏目录下的 `FontHook.ini`。再次启动时会自动恢复上次字体。

如果游戏目录已有同名 `winmm.dll`，先备份原文件。DLL 位数必须和游戏位数一致，否则不会加载。

## 字体选择器

默认启动后显示字体选择窗口。保存过配置后，可以通过 `PickerShowOnStartup=0` 让窗口启动时隐藏。

常用操作：

- `Ctrl+Alt+F`：显示/隐藏字体选择器。
- 如果热键被占用，会依次尝试 `Ctrl+Alt+Shift+F`、`Ctrl+Alt+F8`。
- 搜索框：按字体名过滤。
- 上下键 / PageUp / PageDown / Home / End：切换选中项。
- 回车或双击：应用字体。
- Esc：隐藏窗口。
- 标题栏 `A-` / `A+`：缩放选择器界面。

选择器里的参数栏可以调整字宽、字高、字距、上行、下行、行距和粗细。部分参数会创建临时的字体克隆并修补字体表，用来影响直接读取字体数据的引擎。

## 配置文件

配置文件位于游戏目录：

```ini
FontHook.ini
```

典型配置示例：

```ini
[FontHook]
FontNameW=Microsoft YaHei
FontNameA=Microsoft YaHei
EnableFontHook=1
EnableFaceNameReplace=1
EnableCharsetReplace=1
ForcedCharset=128
PickerShowOnStartup=1
EnableDebugLog=0

EnableTextSubstitution=0
TextSubstitutionMode=0
TextSubstitutionCodepage=932

EnableCodepageSpoof=0
EnableCodepageRuntimeReplace=0
EnableCodepageRedirect=0
CodepageRedirectFrom=932
CodepageRedirectTo=65001

EnableArtemisHook=1
ArtemisPatchTables=1
ArtemisRedirectFontFiles=1
ArtemisClearFontCacheOnSwitch=1

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
EnableMajiroHook=1
MajiroDisableFontCache=1
EnableDxLibHook=1
DxLibDisableFontCache=0

EnableFontHeightScale=0
EnableFontWidthScale=0
EnableFontCharSpacing=0
EnableFontVerticalMetrics=0
EnableFontLineSpacing=0
EnableFontWeight=0
```

常用开关说明：

| 配置项 | 作用 |
| --- | --- |
| `FontNameW` / `FontNameA` | 当前目标字体名。选择器会自动写入。 |
| `EnableFontHook` | 是否启用字体替换主逻辑。 |
| `EnableFaceNameReplace` | 是否替换字体名。 |
| `EnableCharsetReplace` | 是否强制 `LOGFONT` 字符集。 |
| `ForcedCharset` | 强制字符集，例如 `128` 是 Shift-JIS，`134` 是 GB2312。 |
| `EnableTextSubstitution` | 启用字符映射。 |
| `TextSubstitutionMode` | `0` 日文旧字体/异体字到繁体，`1` 繁转简，`2` 简转繁。 |
| `EnableCodepageRedirect` | 重定向 WinAPI 代码页转换。 |
| `CodepageRedirectFrom` / `CodepageRedirectTo` | 例如 `932 -> 65001` 可把 CP932 解码请求改为 UTF-8。 |
| `EnableCodepageSpoof` | 修补字体代码页声明，供会读取字体表的引擎使用。 |
| `EnableDebugLog` | 输出更详细的 `FontHook.trace.log`。 |
| `Compat*` | 兼容性保护开关，遇到个别游戏崩溃或卡死时再调整。 |

## 引擎兼容层

这些兼容层不是简单的字体名替换，而是针对引擎绕过 GDI、预渲染位图字体、字体缓存或私有配置的情况做补丁。

### Artemis

支持现代 Artemis 的资源表和字体路径虚拟化：

- 修补 `_windows` / `_windows_ja` 等表中的字体路径和字号字段。
- 为引擎提供虚拟字体文件，例如 `_base/font/FontHook.ttf`。
- 支持从系统字体或本地字体导出/修补代理字体数据。
- 配置热切换时尝试清理 Artemis 字体缓存。

### 旧版 Artemis

支持 loose `.iet` 脚本和旧式资源路径：

- 修补 `system/*.iet`、`scenario/*.iet` 中的 `face`、`rubyface`、`rendered`、字号和间距字段。
- 支持 `face="$g.font_face"` 这种变量间接引用，会修补对应 `[var name="g.font_face" data="..."]`。
- 可把旧版 `.rft` 预渲染字体路径重定向到运行时代理字体。
- 对直接读取字体文件的路径提供内存虚拟字体。

### KiriKiri / TVP

部分 KiriKiri 游戏使用 `.tft` 预渲染字体缓存，普通 GDI 字体替换不会生效。兼容层会尝试禁用 `mapPrerenderedFont` 注册或隐藏 `.tft`，让脚本回退到普通字体渲染。

### Softpal

Softpal 的默认字体选项可能优先使用 `DEFAULT_FONT.DAT` 位图字体。兼容层会尝试把默认字体类型切换到系统字体分支，并隐藏默认位图字体文件。

### Escu:de

Escu:de 会从 `configure.cfg` 读取字体配置。兼容层会虚拟化 `[Font] Face` / `Font` 配置项，让游戏表现得像配置工具已经选择了当前字体。

### Mirai

Mirai 部分游戏会先创建 HFONT，再把原始 sfnt 字体数据交给 FreeType/D3D。兼容层会重定向 `GetFontData` 和字体文件来源，避免只在 `TextOut` 时替换导致失效。

### Majiro

Majiro 会在 `savedata` 下缓存预渲染字形页。兼容层会绕过 `fc_*.fcd` / `fca_*.fcd` 等缓存读取，并尝试刷新运行时字体缓存，让引擎重新通过被 hook 的 GDI 路径生成字形。

### DxLib

DxLib 可使用 `_FONTSET.MED` 字体缓存。兼容层默认不生成额外的 SFH 缓存文件，只在启用 `DxLibDisableFontCache=1` 且缓存字体和当前选择字体不一致时，让游戏重建原始 `_FONTSET.MED`。

## 支持范围与限制

SimpleFontHook 主要处理“游戏运行时仍然存在文本或字体数据”的路径。

通常可处理：

- 使用 GDI/GDI+/部分 DirectWrite 创建字体和绘制文本的游戏。
- 使用系统字体、TTF/OTF/TTC、可被替换的私有字体文件的游戏。
- 有预渲染字体缓存，但能被禁用或重建的引擎。
- Artemis、KiriKiri、Softpal、Escu:de、Mirai、Majiro、DxLib 等已做专门适配的路径。

通常不能直接处理：

- 文本已经烘焙成整张图片的情况。
- 完全自研的位图字体图集，且没有可切回系统字体/TTF 的路径。
- 不加载 `winmm.dll`、不走 WinAPI 字体栈、或字体渲染完全封闭在自有渲染器里的游戏。
- 受强校验、完整性检查或特殊加载器保护而拒绝旁加载 DLL 的游戏。

遇到这类情况，通常需要逆向具体字库格式、生成替换字库，或编写更深的引擎级 hook。

## 构建

需求：

- Windows
- Visual Studio 2022 或兼容的 MSVC 工具链
- Desktop development with C++
- Detours 头文件和库文件。本仓库根目录已包含 `detours.h`、`detours.lib`、`detours_x64.lib`

构建 32 位版本：

```bat
build_x32.bat
```

输出：

```text
Release\winmm.dll
```

构建 64 位版本：

```bat
build_x64.bat
```

输出：

```text
x64\Release\winmm.dll
```

也可以直接用 MSBuild：

```bat
msbuild SimpleFontHook.sln /p:Configuration=Release /p:Platform=Win32 /t:Rebuild
msbuild SimpleFontHook.sln /p:Configuration=Release /p:Platform=x64 /t:Rebuild
```

## 项目结构

```text
SimpleFontHook/
  dllmain.cpp                 DLL 入口，只负责初始化诊断和 hook
  winmm_proxy.h               winmm 导出转发
  framework.h                 全局配置和公共接口
  utils.cpp                   配置、日志、诊断、本地字体加载
  font/
    font_patcher.*            TTF/OTF/TTC 表修补
  hooks/
    font_hooks.cpp            Detours hook 聚合翻译单元
    hook_policy.*             hook 策略和兼容性开关
    internal/                 GDI/GDI+/DWrite/API hook 细分模块
    internal/engines/         各引擎专用兼容层
  ui/
    font_picker.cpp           字体选择器入口
    internal/                 字体选择器状态、绘制、输入和配置应用
```

内部 `*.cppinc` 文件由对应的 `.cpp` 聚合编译，不是独立编译单元。

## 调试

打开详细日志：

```ini
EnableDebugLog=1
```

日志文件：

```text
FontHook.trace.log
```

建议排查顺序：

1. 确认 `winmm.dll` 位数和游戏一致。
2. 确认 `FontHook.trace.log` 里出现 hook 安装记录。
3. 确认字体选择器能打开，且 `ApplySelectedFont` 有记录。
4. 如果引擎使用字体缓存，尝试删除游戏自己的字体缓存或打开对应引擎兼容开关。
5. 如果是 Artemis / 旧版 Artemis，查看是否有 `Artemis` / `ArtemisLegacy` 的 `table-patched`、`iet-patched`、`font-sync`、`font-redirect` 记录。
6. 如果文本是乱码，优先检查 `EnableTextSubstitution` 和 `EnableCodepageRedirect`。
7. 如果游戏崩溃或卡死，保留 `FontHook.trace.log`，并尝试关闭相关 `Compat*` 或引擎专用开关定位。

## 安全说明

本项目通过 DLL 代理和 API hook 修改当前游戏进程内的字体行为。请只在你合法拥有并允许本地修改的游戏副本上使用。不要把它用于带有在线校验、反作弊或多人对战环境的程序。

字体文件也受授权限制。使用第三方字体时，请确认该字体允许在你的使用场景中加载和分发。
