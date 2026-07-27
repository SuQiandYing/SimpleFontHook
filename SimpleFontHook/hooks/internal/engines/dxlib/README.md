# DxLib 适配器

## 职责

本目录负责 DxLib `_FONTSET.MED` 字形缓存与当前替换字体之间的一致性，并为 DxLib 的
字体数据查询提供引擎身份判断。缓存模型使用游戏原生的根目录 `_FONTSET.MED` 文件。

## 为什么需要这层适配

`_FONTSET.MED` 保存由某个字体生成的字形缓存。缓存存在时，DxLib 可以直接读取字形数据，
当前进程中的字体创建和 `GetFontData` 路径便没有机会生成对应内容。字体名称变化后继续
使用旧缓存，会让配置界面显示新字体而画面仍来自旧字形。

适配器不解析或改写 MED 内容，而是维护“这个原生缓存对应哪个字体”的一致性状态。
需要重建时，只让读取和枚举表现为缓存缺失，写入仍由 DxLib 按自己的格式完成。

## 身份与资源

DxLib 身份要求主模块同时提供 DxLib 库标记和 DxLib 字体契约标记。字体契约覆盖
`CreateFontToHandle`、`DrawStringToHandle` 与 `_FONTSET` 运行时路径。检测直接查询已映射
模块内存并缓存结果。

`_FONTSET.MED` 只表示当前请求属于 DxLib 字形缓存功能。MED 后缀、缓存是否已经生成，
以及同目录的其他 MED 文件均不参与引擎身份判断。

## 文件入口

- `dxlib_font_cache.cppinc`：路径分类、引擎检测、缓存文件访问和配置通知。
- `engine_file_dispatch.cppinc`：调用本模块的打开、属性和目录搜索处理函数。
- 通用 `GetFontData` 钩子：通过 `DxLibShouldReplaceFontDataQueries` 使用 DxLib 身份。

## 实现原理

1. 主模块标记同时确认 DxLib 身份和字体相关运行时契约。
2. 路径分类只接受游戏根目录的 `_FONTSET.MED` 及其搜索模式。
3. `DxLibCachedFontNameW` 与当前 `ForcedFontNameW` 相同且配置允许缓存时，读请求进入真实
   文件 API。
4. 字体不匹配或明确禁用缓存时，打开、属性和目录搜索返回一致的缺失语义。
5. DxLib 以写入方式创建缓存时，适配器保留原生句柄，并在成功打开后记录当前字体名。
6. 字体数据查询按独立配置进入通用 SFNT 替换与字体表处理路径。

## 功能

- 比较 `DxLibCachedFontNameW` 与当前 `ForcedFontNameW`。
- 字体发生变化且启用缓存更新时，让 `_FONTSET.MED` 的只读查询返回缺失状态。
- 保持写入请求指向原生 `_FONTSET.MED`，并在成功打开写入句柄后保存缓存字体名。
- 在字体数据查询阶段提供替换字体的 SFNT 数据和字体表配置。
- 使用有限采样记录缓存命中、重建请求和配置版本。

## 设计理由

- 让引擎自行重建比维护专有 MED 序列化器更可靠，也保留版本差异和写入时序。
- 读、写、属性和搜索语义分别处理，避免“隐藏缓存”同时阻断引擎创建新文件。
- 缓存字体名持久化到配置，使下一次进程启动也能判断磁盘内容是否匹配。
- MED 后缀只用于已确认 DxLib 的资源路由，身份仍由模块内运行时契约提供。
- `GetFontData` 替换和磁盘缓存控制使用独立开关，便于按实际能力组合。

## 配置

- `EnableDxLibHook`
- `DxLibDisableFontCache`
- `DxLibReplaceFontDataQueries`
- `DxLibClearRuntimeFontCacheOnSwitch`
- `DxLibCachedFontNameW`：记录 `_FONTSET.MED` 对应的字体名

## 运行约束

- 只分类游戏根目录的 `_FONTSET.MED` 和对应搜索模式。
- 读路径、写路径和目录搜索分别处理，写入结果保持 DxLib 原生格式。
- 模块标记查询限定在主模块已映射且可读的内存区域，检测结果在进程内缓存。
- 字体选择器线程绕过缓存虚拟化。
- 字体名称负责缓存一致性，文字编码由通用编码模块处理。

## 验证

覆盖已有缓存、缺少缓存、字体名一致、字体名变化、只读打开、写入重建、功能开关、
配置保存、共享 MED 后缀和模块标记缺失的非 DxLib 程序。
