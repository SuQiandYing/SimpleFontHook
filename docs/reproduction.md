# 功能证据与复刻流程

本文定义 SimpleFontHook 适配器的功能描述、证据记录和复刻步骤。文档中的“观测”表示运行或静态检查直接得到的事实；“推断”表示由多个观测归纳出的接口契约；“待确认”表示当前样本没有足够证据支撑的结论。

## 目标与边界

复刻对象是一个可重复验证的输入到输出契约，而不是某个样本的文件副本。每个适配器需要回答以下问题：

| 问题 | 记录内容 |
| --- | --- |
| 身份如何确认 | PE 导入/导出、字符串、目录结构、运行时模块和调用证据 |
| 能力如何确认 | 目标 API、资源格式、编码、字形来源和失败条件 |
| 请求如何路由 | 原始路径、规范化路径、虚拟路径、资源分类和处理分支 |
| 结果如何验证 | 正向样本、非目标样本、边界样本、日志字段和构建架构 |

适配器不把字体替换、代码页转换和引擎识别合并为一个判断。身份、版本能力和资源请求分类按独立阶段记录。

## 证据等级

每条结论使用一个证据等级，并保留来源位置：

| 等级 | 含义 | 允许写入的表述 |
| --- | --- | --- |
| `O` 观测 | 可由命令、日志或测试输出直接复核 | “日志字段 `decision=pass` 表示……” |
| `I` 推断 | 由两条或以上观测得到的稳定契约 | “据此推断资源分类在字体替换前完成” |
| `U` 待确认 | 缺少样本、符号或可重复输出 | “该路径的版本上限待确认” |

来源至少包含文件路径、符号名或命令；运行时证据还要包含样本哈希、架构和配置快照。

## 复刻步骤

### 1. 固定样本

在隔离目录保存待分析程序和资源，记录路径、架构、文件大小、修改时间和 SHA-256：

```powershell
$Sample = 'samples\TARGET'
New-Item -ItemType Directory -Force 'artifacts' | Out-Null
Get-ChildItem -LiteralPath $Sample -Recurse -File |
  ForEach-Object { Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256 } |
  Export-Csv -NoTypeInformation -Encoding UTF8 'artifacts\TARGET.sha256.csv'
```

记录运行环境：Windows 版本、进程架构（Win32 或 x64）、构建配置、字体文件路径、`FontNameW`、`ConfigVersion` 和适配器开关。配置快照直接复制到证据目录，避免依赖当前用户配置。

### 2. 建立基线

以适配器关闭或目标不匹配的状态启动样本，记录原始字体查询、资源打开结果和渲染截图。基线至少包含一条英文、中文、全角标点和缺字形字符。每条记录写入时间、线程、模块、API、参数摘要、返回值和耗时；文本内容使用长度和哈希表示，避免日志无限增长。

### 3. 收集静态证据

对 PE 文件执行以下检查，并把输出保存到文本文件：

```powershell
dumpbin /headers 'samples\TARGET\TARGET.exe' > 'artifacts\TARGET.headers.txt'
dumpbin /imports 'samples\TARGET\TARGET.exe' > 'artifacts\TARGET.imports.txt'
dumpbin /exports 'samples\TARGET\TARGET.exe' > 'artifacts\TARGET.exports.txt'
```

从导入、导出、字符串、RTTI、目录结构和归档目录中分别提取证据。通用 `GDI` 导入、窗口类名、文件扩展名和归档名称只能作为线索，不能单独确认引擎身份。每个字符串保留命中偏移；每个归档条目保留容器、路径和解析方式。

### 4. 确认身份、能力和路由

按下列顺序建立决策记录：

1. 身份阶段：调用 `EngineIdentityPolicy::Confirm` 或适配器对应的确认函数，写出所有命中标记和缺失标记。
2. 能力阶段：确认版本、资源格式、编码和字形来源是否满足适配器契约；能力不足时记录 `fallback` 原因。
3. 路由阶段：把请求规范化为资源分类，再交给 `engine_file_dispatch.cppinc` 的统一重定向入口。文档列出原始路径、规范化路径和虚拟路径的对应关系。

每个阶段都提供一个正向样本和一个非目标样本。非目标样本应具有至少一项相似特征，以验证组合条件而非单一标记。

### 5. 提取最小契约

把观测压缩成可实现的最小输入输出表：

| 输入 | 前置条件 | 输出 | 失败行为 |
| --- | --- | --- | --- |
| 模块/文件/导出 | 身份确认通过 | 资源分类或 API 包装 | 透传原调用 |
| 编码声明 | 代码页在配置快照中有效 | 字形目录或转换结果 | 记录原因并回退 |
| 字体资源 | 文件存在且格式可解析 | 字体句柄、字节流或图集 | 返回原资源或空结果 |

只实现能够由证据支撑的条件。未确认的版本分支保留为 `U`，不以猜测补齐。

### 6. 在正确边界实现

实现位置按调用链选择：

- 引擎身份和能力判断放在对应 `*_detection*` 或 `EngineIdentityPolicy` 模块。
- 文件重定向通过 `engine_file_dispatch.cppinc`，不复制分发逻辑。
- 字体表、代码页和图集处理放在 `font/` 或适配器的资源生成模块。
- 高频钩子只做缓存查询和轻量参数判断；归档解析、磁盘扫描和图集生成放在初始化或后台阶段。

配置变更通过 `NotifyConfigChanged` 传播，并让每个适配器在自己的生命周期边界读取配置快照。

### 7. 验证矩阵

每个适配器的 README 至少包含以下矩阵：

| 场景 | 身份 | 能力 | 路由 | 预期结果 |
| --- | --- | --- | --- | --- |
| 正向样本 | 通过 | 通过 | 命中目标分类 | 使用适配器资源 |
| 非目标样本 | 不通过 | 不评估 | 不路由 | 原调用结果 |
| 能力缺失 | 通过 | 不通过 | 记录原因 | 明确回退 |
| 边界输入 | 通过 | 通过 | 空路径、长路径、未知编码 | 有界处理，不崩溃 |
| 配置切换 | 通过 | 按快照 | 同一请求重复执行 | `ConfigVersion` 可追踪 |

Win32 与 x64 分别执行正向、非目标和边界场景。导出表、日志和截图放入同一证据目录，并以样本哈希命名。

## 日志记录格式

复刻记录把真实日志归一化为以下字段。`FontHook.trace.log` 的固定前缀是时间、PID、TID，
消息体使用模块标签和适配器自己的固定文本；只有部分消息直接包含下列 `key=value` 片段：

```text
time=2026-01-01T00:00:00.000Z module=Yuris phase=identity
decision=pass evidence=ysbin/yscfg.ybn+yu-ris1 config_version=7
source=TARGET.exe request=font/abc.ttf route=virtual/abc.ttf
result=handled fallback=none duration_us=142
```

归一化字段为 `time`、`module`、`phase`、`decision`、`evidence`、`config_version`、
`source`、`request`、`route`、`result`、`fallback`、`duration_us`。路径和文本参数使用长度、
哈希或截断摘要；高频路径不输出完整资源内容。

## 复刻记录模板

将下列模板复制到适配器 README 或诊断记录中：

```text
样本标识: TARGET
样本 SHA-256: SHA256
架构: Win32 | x64
构建配置: CONFIGURATION
配置快照: PATH_TO_CONFIG

观测 O-01:
  来源: FILE:LINE / SYMBOL / COMMAND
  输入: INPUT
  输出: OUTPUT

推断 I-01:
  依据: O-01, O-02
  契约: CONTRACT

待确认 U-01:
  缺口: MISSING_EVIDENCE

正向命令: COMMAND
非目标命令: COMMAND
边界命令: COMMAND
预期日志: module=MODULE phase=PHASE decision=DECISION
预期产物: ARTIFACT_PATHS
```

## 文档表述规则

- 使用“函数返回”“日志记录”“文件包含”“配置读取”等可复核动词。
- 将事实、推断和待确认项分栏；不使用“可靠”“完善”“简单”等无法度量的评价词。
- 功能文档描述当前接口和约束；代码历史、原因演变和提交背景放入提交记录或 ADR。
- 示例命令必须给出输入、输出位置和判定条件；省略条件时标记为待确认。
- 任何结论都能沿着文件、符号、命令或日志字段回到证据来源。
