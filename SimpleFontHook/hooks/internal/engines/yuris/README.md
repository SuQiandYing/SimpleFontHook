# YU-RIS 适配器

## 职责

YU-RIS 适配器负责处理引擎从 YPF 归档读取预渲染字体页的路径。适配器在身份、归档
结构、字体页布局和解码入口全部匹配时，将 `Config::SourceFontNameW` 栅格化为对应
的 32 位图集，并把图集写入 YU-RIS 的 WebP、PNG 或 YDG 解码缓冲区。

资源识别、图集生成和解码替换属于字体资源路径；脚本编码、代码页转换、文字映射以及
普通图片处理属于其他模块。适配器只处理已登记的字体页，其他资源交给默认解码器。

## 入口与依赖

| 文件 | 作用 |
| --- | --- |
| `yuris_font.cppinc` | 按顺序聚合本目录实现；不单独编译 |
| `yuris_state.cppinc` | 运行状态、资源类型、配置快照、目录索引和有界缓存 |
| `yuris_profiles.cppinc` | 代码页候选、字节布局、图集尺寸、像素顺序和样式集合 |
| `yuris_catalog.cppinc` | YPF v500 目录解码、资源目录建立和能力确认 |
| `yuris_renderer.cppinc` | 使用 GDI `GGO_GRAY8_BITMAP` 生成字体页像素 |
| `yuris_runtime.cppinc` | WebP/PNG/YDG 入口、分块聚合、配置通知、窗口刷新和生命周期 |

`hooks/font_hooks.cpp` 将本目录分片加入聚合翻译单元。适配器依赖 `EngineCommon` 的
模块与路径查询、`Config` 的字体配置、GDI 字体 API 和 `HookWorkerThreadState` 工作线程。
文件隐藏、虚拟属性和资源重定向统一由 `engine_file_dispatch.cppinc` 处理，本适配器不
复制该分派器的逻辑。

## 身份、能力与路由

适配器将三个判断阶段分开处理：

1. **身份确认**：主模块同时包含 `ysbin\\yscfg.ybn` 和 `yu-ris1` 时确认 YU-RIS 运行时。
2. **能力确认**：工作线程枚举 `pac/*.ypf`，解析 YPF `0x1F4` 目录，按登记的资源配置
   检查完整路径、页号、样式、图片尺寸和解码模块。
3. **资源路由**：身份与能力成立后，按压缩尺寸、FNV-1a 指纹或 YDG 分块指纹定位
   字体页；未命中的请求调用默认解码器。

文件名、`.ypf` 后缀和归档提示名只用于候选顺序或资源分类。目录字段会按归档首选值、
进程 ACP/OEM、UTF-8 和已安装 Windows 代码页尝试解码；目录编码结论不参与字体页的
双字节代码页选择。

能力确认还包括解码入口：

- WebP：`YSWBP.DLL` ordinal 3。
- PNG：`YSPNG.DLL` ordinal 1、2。
- Relirium/YDG：x86 主模块中唯一匹配登记函数签名的 QOI 解码核心。

单图片配置按压缩尺寸与完整 FNV-1a 指纹分类。YDG 配置按完整分块序列分类，四个分块
需要在同一输出图集基址下匹配同一字体页；分块到达顺序和重复分块不会改变分类结果。

## 处理流程

1. `IsIdentityConfirmed` 缓存主模块身份结果。
2. `Start` 创建 `yuris-catalog` 工作线程，扫描候选 YPF 并构建目录索引和兼容页表。
3. `InstallInOpenDetourTransaction` 在 x86 目标中登记并挂接 Relirium 的 QOI 核心；
   `MaybeWrapGetProcAddress` 按模块名和 ordinal 返回 WebP/PNG 包装入口。
4. WebP 和 PNG 入口先调用默认解码器，再以目录索引判断字体页；YDG 入口收集四个分块，
   完成唯一匹配后才写回图集像素。
5. `RenderCatalogEntry` 使用 GDI 字形栅格化，检查 `Config::ConfigVersion`，把结果存入
   固定容量的图集缓存。
6. `NotifyConfigChanged` 清理待处理分块和图集缓存，向本进程窗口发布 `WM_FONTCHANGE`
   并请求重绘。渲染期间如果快照版本不同，结果直接丢弃并交给默认解码器。
7. `Stop` 停止目录线程、清理待处理 YDG 图集和图集缓存。

## 资源配置

表中的代码页是结构自动模式的首选值。启动阶段会枚举已安装 Windows 代码页，只保留能
按字节布局生成完整页数、槽数和保留位的候选；`YurisAtlasCodepage` 只在这些候选中选择
页表，不改变归档布局。

| 配置名 | 目录契约 | 页槽 | 画布与网格 | 首选代码页 |
| --- | --- | --- | --- | ---: |
| `cp932-s38` | `源ノ角ゴシック[_样式]/fnt_s38_nN.webp`，45 页 × 8 样式，文件直接位于样式目录 | CP932 双字节页，每页 188 槽，无效槽为 `U+FF65` | 1024x640，起点 `(5,11)`，19×10 个 48 px 单元格，字框 38 px | 932 |
| `gbk-s38` | 127 页 × 普通/粗体，`fnt_s38_nN.webp` 位于 `38/` 子目录 | 第 1–126 页使用 GBK 首字节 `0x81–0xFE`，每页 192 槽；第 127 页为 ASCII `0x20–0x7E` | 816x624，起点 `(5,11)`，16×12 个 48 px 单元格，字框 38 px | 936 |
| `big5-c20` | `font_c20/fnt_s20_nN.png`，89 页普通样式 | 首字节 `0xA1–0xF9`；次字节 `0x40–0x7E`、`0xA1–0xFE`，第 64/65 格留空 | 720x400，起点 `(6,9)`，16×10 个 30 px 单元格，字框 20 px | 950 |
| `big5-c10` | `font_c10/fnt_s10_nN.png`，38 页普通样式 | 与 `big5-c20` 相同的双字节槽布局 | 720x400，起点 `(8,10)`，16×10 个 26 px 单元格，字框 10 px | 950 |
| `big5-c10-89+ascii` | `font_c10/fnt_s10_nN.png`，90 页 | 第 1–89 页为 Big5，第 90 页为 ASCII | 720x400，起点 `(8,10)`，16×10 个 26 px 单元格，字框 10 px | 950 |
| `big5-c20-89+ascii` | `font_c20/fnt_s20_nN.png`，90 页 | 第 1–89 页为 Big5，第 90 页为 ASCII | 720x400，起点 `(6,9)`，16×10 个 30 px 单元格，字框 20 px | 950 |
| `big5-c30-89+ascii` | `font_c30/fnt_s30_nN.png`，90 页 | 第 1–89 页为 Big5，第 90 页为 ASCII | 720x440，起点 `(6,11)`，16×10 个 40 px 单元格，字框 30 px | 950 |
| `big5-c40-89+ascii` | `font_c40/fnt_s40_nN.png`，90 页 | 第 1–89 页为 Big5，第 90 页为 ASCII | 820x520，起点 `(7,13)`，16×10 个 50 px 单元格，字框 40 px | 950 |
| `relirium-s38` | `源ノ角ゴシック[_样式]/38/fnt_s38_nN.ydg`，46 页 × 8 样式 | CP932 双字节页并含 `0xF0` 用户定义首字节页，每页 188 槽 | 1024x640，起点 `(5,11)`，19×10 个 48 px 单元格，字框 38 px | 932 |
| `relirium-s31-bold` | `源ノ角ゴシック_太/31/fnt_s31_nN.ydg`，46 页 | 与 `relirium-s38` 相同 | 800x440，起点 `(5,10)`，19×10 个 41 px 单元格，字框 31 px | 932 |
| `relirium-s31-bold-shadow-outline` | `源ノ角ゴシック_太影袋/31/fnt_s31_nN.ydg`，页 1、2、3、46 | 与 `relirium-s38` 相同 | 800x440，起点 `(5,10)`，19×10 个 41 px 单元格，字框 31 px | 932 |

### Big5 资源树

PNG 配置接受以下已登记目录：

- `cgsys_c\\font_c20`、`cgsys_c\\font_c10` 的直接布局；
- `cgsys_c\\font\\font_c20`、`cgsys_c\\font\\font_c10` 的嵌套布局；
- 混合四字号布局中的 `font_c10`、`font_c20`、`font_c30`、`font_c40`。

标准四字号配置只读取严格命名的 `fnt_sNN_n1.png` 到 `fnt_sNN_n90.png`。带 `_`、`__`、
`_old` 后缀的变体、单字图片目录、按钮图片以及空的解包目录不属于字体页。相同压缩载荷
无法唯一确定字号或页码时，当前请求保留默认解码结果。

### 字形样式与像素规则

`FontToPicPreset` 集中定义 CP932/GBK 的合成样式：

| 目录标记 | 像素行为 |
| --- | --- |
| `太` | 向右叠加 2 px，形成粗体遮罩 |
| `影` | 使用 `(2, 1)` 黑色阴影 |
| `袋` | 使用 1 px 黑色轮廓 |

GBK 的 `0x7F`、`0xFF` 和解码失败槽保持透明，槽位序号保持稳定。Big5 PNG 使用白色填充
和 1 px `RGBA(75,75,75,224)` 轮廓。Relirium/YDG 使用已登记的 38/31 YDG 预设，
包含左缘引号、底部省略号、点号光学居中和标点偏移。

标点定位按图集布局独立处理：

- `cp932-s38`、`gbk-s38` 保留 `GLYPHMETRICS::gmptGlyphOrigin`、字体 bearing 和基线；
  `gbk-s38` 使用 `(5,4)` GDI 绘制偏移及 3 px 顶部透明区。
- Big5 使用 PNG 图集约定的全角 bearing：反引号和句号贴左下，逗号到问叹号水平居中，
  成对引号和书名号使用各自锚点，省略号与点号在字框中线光学居中。
- Relirium/YDG 对 `，、。．；：！？` 使用左 2 px、下 1 px 偏移；引号与角分秒符号贴左，
  `…⋯` 贴底，`‥・·•･‧` 使用 YDG 光学居中。

字形、描边和阴影都限制在所属槽内。缺少码点时，先选择同类标点候补，再按图集代码页
选择系统回退字体；字体 `.notdef` 图形不会写入图集。

## 图集生成与字体来源

渲染路径使用 `GGO_GRAY8_BITMAP` 取得字形灰度数据，再按配置的像素顺序写入：

- WebP：写入 `YSWBP` ordinal 3 约定的 BGRA 缓冲区；
- PNG：写入 `YSPNG` ordinal 2 约定的 RGBA 行缓冲区；
- YDG：写入已解码的 YDG RGBA 分块。

运行时不生成临时图片、不调用 Python，也不重打包 YPF。图集缓存最多保存 12 个条目，
缓存键包含目录条目、字符页表代码页和 `Config::ConfigVersion`，容量固定。

图集使用 `Config::SourceFontNameW` 选择 GDI face。系统字体、通过
`AddFontResourceExW(..., FR_PRIVATE, ...)` 注册的 TTF/OTF/TTC，以及 TTC 中按 family/full
name 选择的具体 face 共用同一入口。配置保存 TTC 的 face 名称，而不是集合文件名。

## 配置快照与生命周期

`Config::ConfigVersion` 标识一次完整的字体配置快照。字体名称、字号倍率、字重、页表代码页
和生成的像素都带有该版本：

1. 字体选择器发布配置通知，适配器清理待处理 YDG 分块和图集缓存。
2. 解码请求按指定的 `SourceFontNameW`、高度倍率、字重和页表代码页生成资源。
3. 适配器向本进程窗口发布 `WM_FONTCHANGE` 并请求重绘。
4. 渲染结果提交前再次检查版本；版本不一致的结果直接丢弃。
5. YU-RIS 已经上传到显存的纹理遵循引擎资源持有周期；请求按当前快照建立图集。

`EnableFontHook=0` 时，WebP、PNG 和 YDG 入口保持默认解码行为。身份、能力、指纹或布局
检查失败时也保持默认解码行为。

## 配置

适配器使用字体选择器的通用配置，并通过 `FontHook.ini` 读取页表诊断覆盖键：

| 配置键 | 作用 |
| --- | --- |
| `EnableFontHook` | 总开关；关闭时保持 YSWBP/YSPNG/YDG 的默认解码结果 |
| `FontNameW` | 目标 face，运行时对应 `Config::SourceFontNameW` |
| `EnableFontHeightScale` / `FontHeightScale1000` | 图集字号倍率，结果限制在 8 px 到当前单元格高度 |
| `EnableFontWeight` / `FontWeight` | 基础字重；`太` 样式继续叠加合成粗体 |
| `FontFileName` | 可选私有 TTF/OTF/TTC 文件，由通用字体加载器注册 |
| `YurisAtlasCodepage` | `0` 使用结构自动结果；非零值选择已安装且与槽位布局兼容的代码页 |

字体选择器只呈现跨引擎配置，`YurisAtlasCodepage` 直接由 INI 管理。该边界避免页表覆盖与
文字映射、字符集伪装或代码页重定向形成界面层关联。

`ForcedCharset=128` 只影响通用字体请求，不改变 YPF 页槽映射。`YurisAtlasCodepage` 只改变
字节槽对应的 Unicode 字符，不改变页数、槽数、网格或归档条目。例如，SPIN!2 使用 GBK 时，
45 页 × 188 槽布局无法容纳 GBK 的 126 个首字节页和 192 槽；代码页覆盖值 `936` 会被判为
布局不兼容，当前请求保持默认解码结果并记录 `atlas-codepage override incompatible`。

Big5 字符只有在通用文字映射显式启用时才进入 Unicode 映射；字体选择始终由
`Config::SourceFontNameW` 决定。目录编码首选 CP950，字体槽位使用标准 Big5 字符表；
CP950 独有的 `A3E1`、`F9D6–F9FE` 保持空槽，11 个符号差异位使用标准 Big5 码点。

## 不变量与运行约束

- 身份确认使用主模块标记；通用 GDI 导入、扩展名和单个文件名不能单独判定 YU-RIS。
- 只有完整命中 YPF 版本、目录路径、连续页号、样式集合、图片尺寸和解码入口的候选
  才能建立能力目录。
- 字体替换只写入已确认的字体页，不改变脚本编码、代码页重定向或普通图片。
- 解码入口只读取缓存目录和有界状态，不执行磁盘扫描、归档解析或无界诊断输出。
- 目录准备由工作线程完成；首次等待上限为 5 秒，超时请求回到默认解码器。
- 字体创建、字形提取和缓存访问使用保存的 `org*` API，避免递归进入通用字体钩子。
- 图集缓存容量固定为 12；版本、目录条目和页表代码页共同构成缓存身份。
- 退出流程停止已启动的目录线程并释放事件、待处理分块和缓存；`DLL_PROCESS_DETACH`
  的加载器锁路径只发布退出信号。

## 离线生成工具

离线脚本使用与运行时同名的显式预设，不接受任意编码猜测：

```powershell
python font_to_pic.py --profile cp932-spin2 --font FONT_FILE --font-index TTC_FACE
python font_to_pic.py --profile gbk-goodbye-world-index --font FONT_FILE --font-index TTC_FACE --output-root CGSYSF_ROOT
```

`FONT_FILE` 支持 TTF、OTF、TTC 和 OTC，`TTC_FACE` 从 0 开始计数。GBK 预设在普通/粗体
目录下创建 `38/` 子目录，CP932 预设保留直接样式目录输出。脚本及其规则文件只用于
离线资源准备，不参与 DLL 编译和运行时解码。

## 扩展步骤

1. 在 `yuris_profiles.cppinc` 定义明确的 `AtlasEncodingProfile`、`AtlasProfile` 或
   `ArchiveProfile`，同时写出页数、槽位、目录路径、图片尺寸和解码入口契约。
2. 在 `yuris_catalog.cppinc` 登记完整目录校验、指纹或分块匹配条件；身份、能力和资源
   路由保持三个独立阶段。
3. 在 `yuris_renderer.cppinc` 实现与布局绑定的像素顺序、标点定位和缺字回退规则，
   不把其他配置的坐标算法直接复用到该图集。
4. 在 `yuris_runtime.cppinc` 只挂接已确认的模块、ordinal 或函数签名，并让失败路径
   返回默认解码结果；文件重定向仍经过 `engine_file_dispatch.cppinc`。
5. 为正向样本、非目标程序、功能关闭、布局不完整、指纹歧义和快照切换准备验证记录。

## 证据与复刻

### 静态身份与目录

1. 固定主模块和 `pac/*.ypf` 的 SHA-256，记录架构、目录路径和配置快照。
2. 在主模块可读映像中确认 `ysbin\\yscfg.ybn` 与 `yu-ris1` 的组合标记；只有单个标记的
   样本作为非目标对照。
3. 对候选 YPF 读取 `0x1F4` 目录，保存版本、索引长度、条目路径、页号、样式和图片尺寸。
   目录编码依次记录归档首选值、ACP/OEM、UTF-8 和显式 Windows 代码页的解码结果。
4. 按完整资源配置核对以下 profile 标识：`cp932-s38`、`gbk-s38`、
   `big5-c20`/`big5-c10`、`big5-c10-89+ascii`、`big5-c20-89+ascii`、
   `big5-c30-89+ascii`、`big5-c40-89+ascii`、`relirium-s38`、
   `relirium-s31-bold`、`relirium-s31-bold-shadow-outline`。

### 运行时复刻

1. 记录 `YSWBP.DLL` ordinal 3、`YSPNG.DLL` ordinal 1/2 和 x86 QOI 核心的绑定结果。
2. 启动目录工作线程，保存 `identity confirmed`、`atlas encoding incompatible`、
   `atlas encoding resolved`、`atlas encoding candidate`、`archive matched`、
   `bitmap-font capability confirmed` 或 `capability unavailable` 的
   单次日志及耗时。
3. 对 WebP、PNG、YDG 各发送一份命中页和一份非字体资源，记录 `wrapped`、`rendered`、
   分块聚合、版本号和默认解码回退。
4. 对 `YurisAtlasCodepage` 设置自动值、兼容值和不兼容值，保存 `layout`、槽位数量、
   实际代码页和 `atlas-codepage override incompatible` 结果。
5. 使用 `font_to_pic.py` 的显式 profile 生成同一字体面，记录输入字体哈希、TTC face、
   输出图集尺寸、像素格式和目录树；离线结果与运行时图集逐槽比较。

| 场景 | 身份 | 能力/路由 | 预期结果 |
| --- | --- | --- | --- |
| 组合标记 + 完整 YPF profile + 解码入口 | 通过 | 通过 | 仅登记字体页被替换 |
| 组合标记缺失 | 不通过 | 不评估 | WebP/PNG/YDG 使用默认解码 |
| YPF 目录完整但 profile 不匹配 | 通过 | 不通过 | 记录原因并回退 |
| 代码页覆盖与槽位不兼容 | 通过 | 路由拒绝 | 保持默认解码，不改变资源布局 |
| 非字体图片或不完整 YDG 分块 | 通过 | 不命中 | 不写回图集 |
| `EnableFontHook=0` | 不进入替换 | 不路由 | 保持 YPF 默认结果 |
| 配置版本切换 | 通过 | 当前快照 | 版本不一致的渲染结果丢弃，缓存键含版本 |

证据目录包含 YPF 索引摘要、profile 表、解码入口、逐槽图集哈希、`[Yuris]` 日志、
配置快照和非目标样本。通用记录字段见 [功能证据与复刻流程](../../../../../docs/reproduction.md)。

## 验证

- 正向样本的诊断记录包含 `identity confirmed`、资源来源、扫描耗时、自动代码页、兼容
  代码页数量、资源数量和页数；WebP、PNG、Relirium 分别记录对应的包装入口和图集替换。
- 非 YU-RIS 程序、缺少登记字体页的程序、非字体 WebP/PNG 和不完整 YDG 分块不产生替换。
- `EnableFontHook=0` 时画面使用 YPF 来源的默认解码结果。
- 针对系统 face、TTC face 或页表代码页的验证，诊断记录包含对应的 `ConfigVersion`、实际 face
  和实际代码页。

YPF 目录字段和名称长度交换表参考 GARbro 的
[ArcYPF.cs](https://github.com/morkt/GARbro/blob/master/ArcFormats/YuRis/ArcYPF.cs)。
