# Mirai 适配器

## 职责

本目录负责 Mirai 将 Windows HFONT 的原始 SFNT 数据交给 FreeType/D3D 的字体路径。
适配器提供稳定的字体数据来源、Windows 字体文件重定向和字体表配置，使引擎读取到
与当前替换字体一致的数据。

## 通用路径覆盖范围

Mirai 会通过 `GetFontData` 或直接打开 Windows Fonts 下的文件，把字体交给 FreeType/D3D。
这些接口通常分多次查询表大小和内容，并假设同一字体来源在 face 生命周期内保持稳定。
通用 `HFONT` 替换若在连续查询间重新创建对象，可能让表目录、偏移和实际字节来自不同
资源；直接文件读取也完全绕过 GDI 对象。

适配器为 HDC、原始 HFONT 和源 `LOGFONT` 绑定稳定的替换数据源，并把只读系统字体文件
请求重定向到同一配置所选字体，使对象查询与文件读取获得一致 SFNT。

## 身份与能力

Mirai 身份要求主模块同时包含 FreeType 运行时契约，以及 `DEVICE_FONT_NAME`、
`getFontFile` 或 `FontFile =` 字体配置标记。两个证据直接从已映射模块的可读内存查询，
结果在进程内缓存。

字体文件后缀只在身份成立后参与 Windows Fonts 和本地字体来源分类。

## 文件入口

- `mirai_font_data.cppinc`：检测、稳定 HFONT 来源、`GetFontData` 处理、字体文件定位和
  文件重定向。
- `../common/engine_common.cpp`：共享系统字体注册表缓存和游戏根目录字体定位。
- 通用 `GetFontData` 钩子：优先调用 `MiraiTryGetPinnedFontData`。
- `engine_file_dispatch.cppinc`：处理字体文件打开和属性查询。

## 实现原理

1. FreeType 契约与 Mirai 字体配置标记共同确认身份和目标能力。
2. `GetFontData` 查询根据 HDC、原始 HFONT 和源 `LOGFONT` 查找固定槽中的稳定来源。
3. 缺少来源时创建替换 HFONT，应用所需字体表配置后保存到有界容器。
4. 完整 SFNT 与单表查询都从该来源读取，保持多次调用的目录和字节一致。
5. 文件路由只接受 Windows Fonts 下 SFNT/TTC 的只读请求，并定位系统或游戏本地源字体。
6. 配置通知清理路径与来源版本，使版本通知后的查询建立当前 `ConfigVersion` 绑定。

## 功能

- 按 HDC、原始 HFONT 和源 LOGFONT 建立稳定的替换字体数据来源。
- 在读取完整 SFNT 或单独字体表时应用代码页能力和垂直度量配置。
- 将 Windows Fonts 目录下的 TTF、OTF、TTC 读取重定向到当前字体源文件。
- 按 `ConfigVersion` 与字体名称缓存系统字体或游戏本地字体路径。
- 记录当前配置版本、字体名称和稳定数据源数量。

## 设计约束依据

- 固定数据源服务于 FreeType face 的多次读取契约，而不是单次 API 返回值替换。
- 容器上限为 64 项，防止高频字体查询形成无界对象与 GDI 句柄增长。
- 文件重定向限定在 Windows Fonts 和只读语义，游戏自己的写入与非字体文件保持原路径。
- 目标路径与来源相同时直接透传，避免同一路径重定向递归。
- 字体能力与垂直度量在 SFNT 层处理，文本字节的代码页选择仍属于通用编码模块。

## 配置

- `EnableMiraiHook`
- `MiraiReplaceFontDataQueries`
- `MiraiRedirectFontFiles`
- `MiraiPinFontDataSource`
- 通用字体名称、字符集能力和垂直度量配置

## 运行约束

- 稳定字体来源只绑定有效的 HDC、HFONT 和 LOGFONT 组合，容器上限为 64 项。
- 单个稳定来源在其生命周期内保持同一替换 HFONT，符合 FreeType face 的数据一致性要求。
- 文件重定向只处理 Windows Fonts 下 TTF、OTF、TTC 的只读请求。
- 目标路径与源路径相同时直接使用真实文件。
- 字体选择器线程使用通用字体查询路径。

## 证据与复刻

1. 固定主模块哈希，分别记录 FreeType 契约和 `DEVICE_FONT_NAME`、`getFontFile`、
   `FontFile =` 中至少一项配置标记。
2. 对同一 HDC/HFONT/LOGFONT 顺序执行完整 SFNT 长度查询、内容查询和单表查询，保存每次
   返回长度、表标签、字节哈希和固定来源槽。
3. 对 Windows Fonts 下 TTF/OTF/TTC 的只读打开记录请求路径、来源路径和目标文件哈希；
   对写入、根目录外文件和相同源/目标路径记录透传结果。
4. 连续创建 64 个及以上候选来源，确认容器上限和既有来源复用；配置切换后记录新版本绑定。

| 场景 | 预期结果 |
| --- | --- |
| FreeType + Mirai 字体配置标记 | 身份与字体数据能力成立 |
| 只有 FreeType 或通用 DAT 后缀 | 身份不成立 |
| 同一对象的长度/内容/单表查询 | 使用同一固定来源和配置版本 |
| Windows Fonts 只读 SFNT | 重定向到当前字体源 |
| 来源容器达到 64 项 | 不产生无界 HFONT/对象增长 |

证据材料包含 `[TRACE][Mirai]` 日志、查询序列、路径映射、字体哈希和非目标模块结果。
记录模板见 [功能证据与复刻流程](../../../../../docs/reproduction.md)。

## 验证

覆盖完整 SFNT、单表查询、系统字体文件、用户字体、游戏本地字体、稳定来源复用、
配置通知、功能开关、缺少 FreeType 或字体配置标记，以及共享 DAT 后缀的非 Mirai 程序。
