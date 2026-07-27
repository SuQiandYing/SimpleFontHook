# TyranoScript 适配器

## 职责

TyranoScript 5 通常运行在 Electron/Chromium 中，内置 `@font-face` 字体绕过游戏进程
的 GDI 字体创建路径。本适配器通过文件虚拟化、ASAR 覆盖和 JavaScript 桥接替换 Web
字体。

## 适配目标

Chromium 从解包目录或 ASAR 读取 Web 字体，并在渲染进程维护自己的字体缓存。通用 GDI
与 DirectWrite 创建钩子不是这条资源链的权威入口。适配器需要同时处理字体文件内容、
Electron 对 `app.asar` 与 `resources/app` 的选择，以及主进程和渲染进程中的缓存同步。

## 检测条件

支持两种布局：

- 解包目录：存在 `resources/app/tyrano`，且 `resources/app/package.json` 包含
  TyranoScript 标记。
- ASAR：能够解析 `resources/app.asar`，并在 `package.json` 或 `tyrano/` 资源域中找到
  TyranoScript 正向标记。

Electron 运行时、`app.asar` 文件名和 `data/scenario` 目录可被其他应用复用，不参与独立
身份判断。

检测结果在进程生命周期内缓存。ASAR 无法可靠建立索引时，不隐藏原 `app.asar`。

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
2. 字体导出器生成当前版本 SFNT，Web 字体处理器为 `.ttf`、`.otf`、`.ttc` 请求返回
   只读虚拟句柄。
3. 替换字体可用时，压缩 Web 字体请求表现为缺失，使 CSS 字体源继续尝试 SFNT fallback。
4. ASAR 索引把需要的归档条目映射到 `resources/app` 视图，只覆盖文本、脚本和缺失或
   尺寸不匹配的资源。
5. `main.js` 与 `tyrano/tyrano.js` 在内存视图中附加唯一标记的桥接代码，用于同步
   Electron 主进程与渲染进程字体状态。
6. 配置通知清理字体字节和注入结果缓存，并向桥接发布当前 `ConfigVersion`。

## 字体重定向

- `resources/app/data/others/font` 下的 `.ttf`、`.otf` 和 `.ttc` 请求返回当前选择
  字体的 SFNT 数据。
- `.woff` 和 `.woff2` 在替换数据准备完成后隐藏，使 Chromium 回退到可替换的 SFNT
  字体源。
- 字体字节按 `ConfigVersion` 和字体名称缓存，配置变化后清空。

## ASAR 覆盖

Electron 可以从 `resources/app` 加载解包资源，ASAR 索引为同名文本和脚本提供权威
内容。`main.js` 和 `tyrano/tyrano.js` 可附加带唯一标记的桥接脚本，重复读取共享同一
注入结果。

## 设计理由

- ASAR 以只读索引和覆盖视图参与运行，不修改原归档，也不依赖 Electron 内部模块。
- 只有索引可完整读取时才控制 `app.asar` 可见性，保证 `resources/app` 能获得权威内容。
- 大型音频和视频沿用真实文件或归档路径，覆盖器聚焦文本、脚本和必要的缺失资源。
- 压缩字体只在 SFNT 替换已准备时隐藏，避免 CSS 字体源全部失效。
- JavaScript 桥接承担运行时同步，原生文件重定向仍是一条独立可用的资源路径。
- 注入标记保证同一脚本重复打开时内容幂等，配置版本负责字体状态更新。

## 配置

- `EnableTyranoHook`
- `TyranoRedirectWebFonts`
- 通用的 `EnableFontHook`、`EnableFaceNameReplace` 和 `ForcedFontNameW`

## 不变量

- 只有完整 Tyrano 正向检测和替换字体准备成功后才隐藏或重定向文件。
- 所有文件打开和属性结果统一经过引擎文件分派器，A/W 入口共享分类逻辑。
- ASAR 头大小、JSON 大小、偏移和文件长度必须有上限并检查溢出。
- 只处理只读打开；写入、创建和截断请求交给真实文件 API。
- JavaScript 桥接是增强路径，失败时原生 Web 字体重定向仍可独立工作。
- 配置变化只清理缓存并通知桥接，不在文件钩子内重复导出字体。

## 验证

至少覆盖解包布局、纯 ASAR、损坏 ASAR、缺少正向标记、`.woff` fallback、SFNT
重定向、桥接去重、功能关闭、热切换和非 Tyrano Electron 程序。
