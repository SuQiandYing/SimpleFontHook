# TyranoScript 适配器

## 职责

TyranoScript 5 通常运行在 Electron/Chromium 中，内置 `@font-face` 字体绕过游戏进程
的 GDI 字体创建路径。本适配器通过文件虚拟化、ASAR 覆盖和 JavaScript 桥接替换 Web
字体。

## 适配目标

Chromium 从解包目录或 ASAR 读取 Web 字体，并在渲染进程维护自己的字体缓存。通用 GDI
与 DirectWrite 创建钩子不是这条资源链的权威入口。适配器需要同时处理字体文件内容、
Electron 启动时选定的 `app.asar` 或 `resources/app` 存储根，以及主进程和渲染进程中的
缓存同步。运行期配置变化不改变已经选定的存储根。

## 检测条件

支持两种布局：

- 解包目录：存在 `resources/app/tyrano`，且 `resources/app/package.json` 包含
  TyranoScript 标记。
- ASAR：能够解析 `resources/app.asar`，并在 `package.json` 或 `tyrano/` 资源域中找到
  TyranoScript 正向标记。

Electron 运行时、`app.asar` 文件名和 `data/scenario` 目录可被其他应用复用，不参与独立
身份判断。

检测结果在进程生命周期内缓存。ASAR 头、JSON 长度或索引边界校验失败时，不改变
`app.asar` 的可见性。

## 文件结构

- `tyrano_web_fonts.cppinc`：聚合入口、Web 字体重定向和配置刷新。
- `tyrano_state.cppinc`：探测、字体字节、ASAR 索引和日志状态。
- `tyrano_detection.cppinc`：解包/ASAR 检测及 `app.asar` 可见性决策。
- `tyrano_asar_parser.cppinc`：有界解析 ASAR 头和文件索引。
- `tyrano_asar_overlay.cppinc`：为解包路径提供归档内容和虚拟属性。
- `tyrano_bridge.cppinc`：注入 Electron 主进程和渲染进程字体同步桥。
- `../common/engine_font_export.cppinc`：共享 GDI 字体导出和 SFNT 数据校验。

## 实现原理

1. 解包布局通过 `package.json` 与 `tyrano/` 框架结构确认；归档布局通过有界 ASAR 解析
   和归档内 Tyrano 资源共同确认。
2. 字体导出器生成当前版本 SFNT，Web 字体处理器为 `data/others` 直属或子目录中的
   `.ttf`、`.otf`、`.ttc` 请求返回只读虚拟句柄。
3. 替换字体可用时，压缩 Web 字体请求表现为缺失，使 CSS 字体源继续尝试 SFNT fallback。
4. ASAR 索引把需要的归档条目映射到 `resources/app` 视图，只覆盖文本、脚本和缺失或
   尺寸不匹配的资源；路径规范化折叠 `.` 和 `..`，并拒绝越过应用根的请求。
5. `main.js` 与 `tyrano/tyrano.js` 在内存视图中附加唯一标记的桥接代码，用于同步
   Electron 主进程与渲染进程字体状态；CSS 字族列表同时包含本地化名、英文名及去除
   数字字重后缀的 typographic-family 候选。
6. 配置通知清理字体字节和注入结果缓存，并向桥接发布当前 `ConfigVersion`。

## 字体重定向

- `resources/app/data/others` 直属或子目录下的 `.ttf`、`.otf` 和 `.ttc` 请求返回当前
  选择字体的 SFNT 数据，兼容生成器常用的 `data/others/<face>.ttf` 与手工整理的
  `data/others/font/<face>.ttf` 两种布局。
- `.woff` 和 `.woff2` 在替换数据准备完成后隐藏，使 Chromium 回退到可替换的 SFNT
  字体源。
- 字体字节按 `ConfigVersion` 和字体名称缓存，配置变化后清空。

## ASAR 覆盖

Electron 可以从 `resources/app` 加载解包资源，ASAR 索引为同名文本和脚本提供权威
内容。已经从 `app.asar` 启动的进程继续使用归档根，适配器不会在热切换时隐藏归档。
`main.js` 和 `tyrano/tyrano.js` 可附加带唯一标记的桥接脚本，重复读取共享同一注入结果。
持久化的 `app.asar.sfh` 还会校验桥接槽内容；DLL 更新桥接实现后，即使原归档时间戳未变，
旧缓存也会自动重建。

## 设计约束依据

- ASAR 以只读索引和覆盖视图参与运行，不修改原归档，也不依赖 Electron 内部模块。
- `app.asar` 可见性在进程运行期保持稳定，避免后续 `./data`、`./tyrano` 相对请求切换到
  不存在或不完整的 loose 目录。
- 大型音频和视频沿用真实文件或归档路径，覆盖器聚焦文本、脚本和必要的缺失资源。
- 压缩字体只在 SFNT 替换已准备时隐藏，避免 CSS 字体源全部失效。
- JavaScript 桥接承担运行时同步，原生文件重定向仍是一条独立可用的资源路径。
- 注入标记保证同一脚本重复打开时内容幂等，桥接内容参与 ASAR 缓存身份，配置版本负责
  字体状态更新。

## 配置

- `EnableTyranoHook`
- `TyranoRedirectWebFonts`
- 通用的 `EnableFontHook`、`EnableFaceNameReplace` 和 `ForcedFontNameW`

## 不变量

- 只有完整 Tyrano 正向检测和替换字体准备成功后才隐藏或重定向文件。
- 所有文件打开和属性结果统一经过引擎文件分派器，A/W 入口共享分类逻辑。
- 归档路径折叠 `.` 和 `..` 后必须仍位于 `resources/app` 根内。
- ASAR 头大小、JSON 大小、偏移和文件长度必须有上限并检查溢出。
- 只处理只读打开；写入、创建和截断请求交给真实文件 API。
- JavaScript 桥接是增强路径，失败时原生 Web 字体重定向仍可独立工作。
- 配置变化只清理缓存并通知桥接，不在文件钩子内重复导出字体。

## 证据与复刻

1. 固定 `resources/app`、`resources/app.asar`、`package.json`、`tyrano/` 和字体目录的
   SHA-256；解包与 ASAR 样本分开记录。
2. 对解包布局检查 `resources/app/tyrano`、`package.json` 标记和字体路径；对 ASAR 保存头大小、
   JSON 大小、索引条目、偏移和长度的边界结果。
3. 对 `data/others` 直属和 `data/others/font` 子目录中的 `.ttf`、`.otf`、`.ttc`、
   `.woff`、`.woff2` 依次执行只读打开、属性和 Web 字体回退，记录 SFNT 字节哈希与
   `ConfigVersion`。
4. 在 `main.js` 和 `tyrano/tyrano.js` 中记录桥接标记、注入位置、重复读取结果及主/渲染进程
   消息顺序；索引校验失败时检查 `app.asar` 是否保持真实文件。
5. 对写入、创建、截断、非 Tyrano Electron 和损坏归档执行负向样本。

| 场景 | 预期结果 |
| --- | --- |
| 解包框架 + Tyrano 标记 | 文件字体和桥接路径可用 |
| ASAR 索引完整 + Tyrano 资源 | 归档视图与解包视图按请求路由 |
| ASAR 边界/JSON/资源校验失败 | 不改变 `app.asar` 可见性，调用真实 API |
| SFNT 准备完成 | 两种 `data/others` 字体布局均按配置重定向，压缩字体作为回退源处理 |
| 本地化 GDI 名含 `75W` 等字重后缀 | CSS 依次尝试原名、英文名与 typographic-family 候选 |
| DLL 桥接实现更新、原 ASAR 未变化 | `app.asar.sfh` 因桥接槽不匹配自动重建 |
| 写入或配置关闭 | 文件写入透传，缓存与桥接保持引擎状态 |

证据目录包含 ASAR 索引摘要、字体哈希、桥接日志、主/渲染线程标识和非目标样本。
记录模板见 [功能证据与复刻流程](../../../../../docs/reproduction.md)。

## 验证

至少覆盖解包布局、纯 ASAR、损坏 ASAR、缺少正向标记、`.woff` fallback、SFNT
重定向、桥接去重、功能关闭、热切换和非 Tyrano Electron 程序。
