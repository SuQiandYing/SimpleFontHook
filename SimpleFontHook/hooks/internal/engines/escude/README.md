# Escu:de 适配器

## 职责

本目录负责 Escu:de `configure.cfg` 中字体选项的运行时视图。适配器通过 Win32 Profile
API 返回当前替换字体，使引擎进入可由通用 GDI 钩子处理的字体分支。

## 通用路径覆盖范围

Escu:de 在创建字体前从 `configure.cfg` 读取 `[Font]` 选项。这个配置值决定引擎选择哪条
字体路径，因此仅拦截 GDI 创建阶段无法保证引擎进入系统字体分支。直接修改根目录或用户
目录中的配置文件还会改变游戏持久状态，并且不能覆盖进程内热切换。

适配器在 Profile API 边界提供只对目标节、目标键和目标产品生效的虚拟值。引擎按照
自己的配置读取流程进入 GDI 路径，磁盘配置和其他 Profile 请求保持原语义。

## 检测条件

根目录配置或当前 Profile API 实际请求的 `configure.cfg` 在 `[General]` 节提供：

- `Company=ESCUDE`
- 非空的 `Product`

产品名用于识别根目录配置和 `Documents/ESCUDE/<Product>/configure.cfg` 用户配置。用户
配置可以作为首次身份来源，因此资源位于用户目录时也能完成识别。

## 文件入口

- `escude_config.cppinc`：引擎检测、配置路径识别和 Profile API 返回值处理。
- `newGetPrivateProfileStringA/W`：提供 `[Font] Face` 与 `[Font] Font` 的虚拟值。
- 通用 Profile API Detours：把其他节和键交给真实 API。

## 实现原理

1. 读取候选 `configure.cfg` 的 `[General]`，以 `Company=ESCUDE` 和非空 `Product`
   共同确认身份。
2. 产品名派生根目录配置和 `Documents/ESCUDE/<Product>/configure.cfg` 的允许路径。
3. Profile 钩子同时匹配文件、节名、键名、字体总开关和虚拟配置开关。
4. 命中 `[Font] Face` 或 `[Font] Font` 时，按 A/W 调用返回对应 `ForcedFontName`。
5. 复制函数复现 Win32 缓冲区截断、终止符和返回长度语义；未命中请求调用 `org*` API。

## 功能

- 同时支持 ANSI 与 Unicode Profile API。
- 识别游戏根目录和当前产品的用户配置路径。
- 为 `[Font]` 节的 `Face`、`Font` 键返回 `ForcedFontNameA/W`。
- 保持 Win32 Profile API 的缓冲区长度、截断和返回值语义。
- 使用有限日志记录产品、配置路径和字体值。

## 设计约束依据

- 在读取边界提供视图，既覆盖根目录与用户目录配置，也保持磁盘文件不受运行时选择影响。
- 公司与产品共同构成身份，`configure.cfg` 文件名只负责候选定位。
- A/W 入口共享相同的身份和路径状态，字符串复制分别遵循对应 Win32 契约。
- 字体选择器线程读取真实配置，避免 UI 自己消费为游戏准备的虚拟值。
- Profile 整数和无关节键完全透传，使适配面限定在两个字体字符串。

## 配置

- `EnableEscudeHook`
- `EscudeVirtualFontConfig`
- `EnableFontHook`
- `EnableFaceNameReplace`
- `ForcedFontNameA`、`ForcedFontNameW`

## 运行约束

- 公司、产品、节名、键名和配置路径全部匹配后提供虚拟字体值。
- 字体选择器线程使用真实配置读取路径。
- Profile 整数读取和无关字符串读取保持系统 API 行为。
- 已确认的产品名和身份按进程生命周期缓存；动态配置请求可在根目录探测完成后补充身份。

## 证据与复刻

1. 固定根目录和用户目录的 `configure.cfg`，记录 `[General]` 的 `Company`、`Product`、
   文件哈希和实际 Profile API 路径。
2. 分别调用 `newGetPrivateProfileStringA/W` 读取 `[Font] Face`、`[Font] Font`，保存缓冲
   区长度为 `0`、`1`、正常长度和截断长度时的返回值与终止符。
3. 对其他节、其他键、整数读取、写入请求和非匹配产品调用真实 `org*` API，并记录结果。
4. 将 `ForcedFontNameA/W`、总开关、虚拟配置开关和 `ConfigVersion` 纳入每条日志。

| 场景 | 预期结果 |
| --- | --- |
| `Company=ESCUDE` 且 `Product` 非空 | 根目录/用户路径获得身份候选 |
| 缺少任一身份字段 | Profile 请求透传 |
| A/W 字体键命中 | 返回对应编码的替换字体，并保持缓冲区语义 |
| 其他键或写入 | 返回真实 Profile 结果 |
| 任一开关关闭 | 不提供虚拟字体值 |

证据目录包含两种路径的配置哈希、A/W 返回值、截断测试、`[DEBUG][Escude]` 日志和非目标
配置。记录模板见 [功能证据与复刻流程](../../../../../docs/reproduction.md)。

## 验证

覆盖根目录配置、用户配置、ANSI/Unicode 调用、`Face`/`Font` 键、缓冲区截断、功能
开关、其他节键和具有同名配置文件的非 Escu:de 程序。
