# 诊断与问题定位

## 日志文件

运行日志写入目标程序目录中的 `FontHook.trace.log`。每个进程会话第一次写入时重建
该文件，日志行包含时间、PID、TID 和模块标签。

基础安装、配置和引擎探测日志始终可能出现。详细 API 采样、异常记录和卡顿监视由
`EnableDebugLog` 控制。

## 用于复现的临时调试配置

```ini
[FontHook]
EnableDebugLog=1
DebugSlowMs=50
DebugTraceSampleLimit=128
DebugPickerThreadLogLimit=32
```

只在复现问题期间启用。完成定位后恢复：

```ini
EnableDebugLog=0
DebugTraceSampleLimit=0
DebugPickerThreadLogLimit=0
```

## 结构化字段

每条真实日志的固定前缀为本地时间、PID 和 TID；消息体包含模块标签。部分适配器消息在
标签后使用 `key=value` 片段，其他消息使用固定文本。下表是整理证据时使用的归一化字段，
不表示每条 `FontHook.trace.log` 都包含全部字段：

| 字段 | 来源 | 判定用途 |
| --- | --- | --- |
| `module` | 消息体中的适配器或通用钩子标签 | 确认处理边界 |
| `phase` | 根据固定消息映射为 `identity`、`capability`、`route`、`render` 等阶段 | 区分决策层次 |
| `decision` | 根据消息结果归一化为 `pass`、`skip`、`fallback`、`error` | 确认分支结果 |
| `evidence` | 标记、导出、资源布局或运行时接口摘要 | 回溯身份/能力依据 |
| `configVersion` | `Config::ConfigVersion` | 关联配置快照和缓存 |
| `request` / `route` | 原始请求与规范化/虚拟路径 | 检查分发结果 |
| `fallback` | 回退原因或 `none` | 区分能力缺失与资源缺失 |
| `duration_us` | 调用计时器，或由 `scan-ms` 等原字段换算 | 定位高频路径耗时 |

同一复现记录中，日志、配置和样本哈希必须使用同一进程架构。缺少字段时标记为“待确认”，
不以最终画面单独推断分支。

## 日志标签

| 标签 | 含义 |
| --- | --- |
| `[TRACE]` | 安装、配置版本和 API 采样 |
| `[DEBUG][TextSub]` | 文字映射输入和输出 |
| `[DEBUG][BGI]` | BGI 检测、缓存和度量适配 |
| `[DEBUG][CatSystem2]` | classic CatSystem2 字体源、文件重定向和缺失目录枚举回退 |
| `[DEBUG][UnityMono]` | Unity Mono 运行时绑定和对象应用 |
| `[DEBUG][CatSystemUnity]` | Unity IL2CPP/TMP 运行时适配 |
| `[RenPy]` | 字体源解析、Python 虚拟文件回调和缓存刷新 |
| `[Tyrano]` | ASAR 检测、Web 字体重定向和桥接 |
| `[Yuris]` | YPF 目录编码探测、兼容页表、图集替换和覆盖值回退 |
| `[DIAG][crash]` | 首次机会异常的有限采样 |
| `[DIAG][hang-watchdog]` | 长时间阶段和窗口无响应探测 |

## 定位流程

1. 使用关闭所有可选兼容功能的配置确认基础加载是否正常。
2. 只启用字体名称替换，区分字体问题与编码、度量或引擎缓存问题。
3. 分别启用字符集伪装、代码页重定向和文字映射，不同时改变多个变量。
4. 查看进程启动阶段的引擎探测结果，确认没有误判。
5. 对照首次错误输出前后的 API 样本，而不是只分析最终截图。
6. 记录目标程序架构、DLL SHA-256、配置文件和最短复现步骤。

### 证据复刻步骤

1. 复制样本到隔离目录，使用 `Get-FileHash -Algorithm SHA256` 保存所有输入哈希。
2. 使用关闭目标适配器的配置启动一次，保存基线日志和截图。
3. 每次只改变一个配置组，记录 `ConfigVersion`、适配器日志和结果哈希。
4. 使用正向、相似但非目标、能力缺失和边界样本验证身份、能力、路由三个阶段。
5. 将命令、配置、日志和产物路径写入 [功能证据与复刻流程](reproduction.md) 的记录模板。

## 常见问题

### 换字体后文字变成错误汉字

先确认是否错误启用了代码页重定向或文字映射。字体替换本身不应改变文本字节流。
查看原始字体请求检测到的字符集，以及 `[DEBUG][TextSub]` 中实际解码后的 Unicode。

### 字体切换后没有变化

检查 `ConfigVersion` 是否更新、目标引擎是否使用内部缓存，以及对应适配器是否记录了
缓存清理或资源重建。不要直接把问题归因于字体名称替换失败。

### 启动卡顿

检查日志中的慢调用和 watchdog 阶段。重点排查高频钩子中的重复文件扫描、PE/ASAR
解析、字体源或资源生成、运行时类型查找和无界日志。

### 仅某个引擎异常

先确认引擎检测证据，再临时关闭该引擎总开关进行对照。通用 GDI 导入不能作为具体
引擎身份的充分证据。

### YU-RIS 页表布局不兼容

先看 `atlas encoding resolved` 的 `layout`、`auto-codepage` 和兼容代码页数量，再确认
`YurisAtlasCodepage`。出现 `atlas-codepage override incompatible` 表示指定代码页不能
装入当前固定页数和槽位；例如 CP932 的 45×188 图集不能承载 GBK 的 127×192 页表。
这时需要提供与目标代码页匹配的字体资源布局，而不是改 `ForcedCharset`、转码或文字映射选项。
也不要用 EXE 中单个 `push 932` 或 `MultiByteToWideChar` 导入判断页表；现有 CP932、GBK
和 Big5 样本都保留同一套 CRT 转换代码，该证据不能区分实际字体槽位。

### YU-RIS 标点偏移或出现碎点

先区分资源的“槽步长”“实际字框”和“文字绘制原点”。WebP `gbk-s38` 使用
816x624 画布、16x12 槽、48 px 步长和 38x38 字框。运行时 GDI 的首槽绘制原点由槽起点
`(5,11)` 和绘制偏移 `(5,4)` 合成 `(10,15)`，用于保留参考 Pillow 图集约 6 px 的槽顶透明区；不能把黑框强制居中，也不能套用 Relirium 的贴左、
贴底坐标。Relirium 与 Big5 也不能共用同一组偏移：Relirium/YDG 才使用统一的左下偏移
和底部省略号；Big5 的 `、。`、全宽问叹号、成对括号、小型形式符号与居中省略号分别
按 PNG 图集的 bearing 类别定位。所有配置均按单槽裁剪，并在字体缺字时使用同类符号或
代码页回退字体，
避免相邻槽串图和 `.notdef` 碎片。

## 日志提交内容

提供问题材料时包含：目标程序架构、复现步骤、`FontHook.ini`、对应构建哈希、完整的
单次 `FontHook.trace.log` 和错误画面。删除与问题无关的重复日志，避免混合不同配置
版本的记录。

## 证据判定

| 结果 | 允许的结论 |
| --- | --- |
| 身份日志 `decision=pass`，能力日志 `decision=pass` | 可以检查专用资源路由 |
| 身份通过，能力 `fallback` | 适配器保持通用路径，记录回退原因 |
| 身份 `skip` | 不检查引擎专用资源，不修改导入、缓存或文本 |
| 只有最终截图变化 | 结果可见，但分支来源待确认 |
