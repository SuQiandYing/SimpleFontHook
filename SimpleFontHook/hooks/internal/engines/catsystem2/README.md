# CatSystem2 适配器

## 职责

本目录适配 classic CatSystem2 的文件字体路径。该引擎会先从游戏根目录的 `font/`
加载 TTF、OTF 或 TTC 字节，再由 FreeType 生成字形；因此只替换 GDI `HFONT` 不能覆盖
屏幕上的实际文本。

本适配器与 Unity 目录中的 `CatSystemUnity` 无关。后者处理 Mono/IL2CPP 与 TextMeshPro
对象，本目录只处理 classic CatSystem2 的 Win32 文件字体和字体资源注册。

## 检测

适配器要求主模块同时满足以下契约：

- 存在 `CatScene` 或 `cs2confx` 引擎标记；
- 同时存在 `kcFontImage_Win` 和 `kcFontImage_FT` 字体后端标记。

通用 GDI 导入、`font` 目录、单个字体文件名和 `.int` 后缀均不参与独立身份判定。
检测结果按进程缓存，文件高频路径只读取缓存结果。

## 文件入口

- `font/*.ttf`
- `font/*.otf`
- `font/*.ttc`
- `font/*.otc`

物理目录存在时，原目录枚举保持不变，但每个只读字体文件打开及
`AddFontResourceA/W` 注册及对应的 `RemoveFontResourceA/W` 卸载会转向当前选择字体的
真实文件。

物理目录不存在、为空或目标字体项缺失时，适配器执行以下回退：

1. 为缺失的 `font` 根目录发布虚拟目录属性；
2. 在真实 `FindFirstFile` 或 `FindFirstFileEx` 失败后发布一个
   `SimpleFontHook.*` 字体项；
3. 搜索句柄来自实际字体文件，因此对应的 `FindNextFile` 和 `FindClose` 保持 Win32
   原语义；
4. 打开虚拟路径时仍通过统一文件分发器重定向到真实字体源。

游戏目录不会被创建、删除或写入。

## 字体源

字体源按以下顺序解析并按 `ConfigVersion` 缓存：

1. `SourceFontNameW` 对应的系统或当前用户字体文件；
2. `ForcedFontNameW` 对应的字体文件；
3. `FontFileName` 或 DLL 根目录中的本地 SFNT 文件。

适配器服从通用 `EnableFontHook` 和 `EnableFaceNameReplace` 开关，不定义引擎私有配置。
任一开关关闭、字体源不可用或身份检测不成立时，所有 API 保持原行为。

## 运行约束

- 仅重定向只读打开，不接管写入、创建或删除。
- 配置通知使字体源缓存失效；版本通知后的文件打开使用当前快照。
- CatSystem2 已创建的 FreeType face 不在未知线程上强制销毁。需要重新读取字体文件的
  场景由引擎自身重载或下次进程启动完成。
- 虚拟枚举只在原搜索失败后介入，物理 `font` 目录的其他资源和顺序不受影响。

## 诊断

启用 `EnableDebugLog=1` 后查看 `[DEBUG][CatSystem2]`：

- `engine-probe`：身份与字体后端契约；
- `font-source-ready`：当前选择字体解析到的真实文件；
- `font-open-redirect`：物理或虚拟字体文件打开重定向；
- `font-search-fallback`：缺失目录或条目的虚拟枚举；
- `font-register-redirect`、`font-unregister-redirect`：字体注册与卸载重定向。

Win32 与 x64 Release 均需保持可编译；classic CatSystem2 样本通常使用 Win32 产物。

## 证据与复刻

1. 在主模块可读映像中分别记录 `CatScene`/`cs2confx` 与
   `kcFontImage_Win`/`kcFontImage_FT` 的命中情况；四项按身份组和能力组保存。
2. 使用存在、空、缺失三种 `font/` 目录建立基线，分别调用属性查询、
   `FindFirstFile*`、`FindNextFile`、只读打开和字体注册 API。
3. 保存 `font-source-ready` 的源路径、`ConfigVersion` 和 SFNT 扩展名；检查虚拟条目是否
   能通过统一文件分派器打开同一来源。
4. 对写入、创建、删除、根目录外路径和非字体文件执行负向验证。

| 场景 | 预期日志/结果 |
| --- | --- |
| 两组标记均成立且目录有字体 | `engine-probe` 通过，只读打开重定向 |
| 两组标记均成立但目录缺失 | `font-search-fallback` 发布一个虚拟字体项 |
| 只有 `CatScene` 或只有 FreeType 标记 | 身份/能力不完整，文件 API 透传 |
| 写入或创建请求 | 调用真实文件 API，不生成游戏目录 |
| 配置通知 | 解析当前字体源，既有 FreeType face 由引擎生命周期管理 |

证据目录包含样本哈希、目录树、`[DEBUG][CatSystem2]` 单次日志、配置快照和 A/W API
返回值。记录格式见 [功能证据与复刻流程](../../../../../docs/reproduction.md)。

## 验证

覆盖 Win32/x64 编译、正向标记组合、相似非目标模块、物理目录三种状态、TTF/OTF/TTC/OTC、
注册与卸载、配置关闭、字体源缺失、非 ASCII 路径和只读/写入语义。
