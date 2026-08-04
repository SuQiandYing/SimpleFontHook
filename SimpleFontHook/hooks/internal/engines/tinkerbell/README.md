# TinkerBell 适配器

## 职责

本目录为 TinkerBell/Cyberworks 的 GDI 调用方式提供钩子策略。适配器选择字体创建入口
和 `SelectObject` 跟踪范围，避免宽字符字体创建与未跟踪 DC 对该引擎渲染状态产生干扰。

## 策略适配边界

TinkerBell 的兼容点位于通用 GDI 钩子的安装范围与 `SelectObject` 处理边界，而不是专有
字体资源。为此单独安装一组引擎钩子会复制字体模型，并让同一个 HFONT 同时经过两套状态。

本模块只回答“哪些通用入口参与当前进程”和“未跟踪对象是否透传”。实际字体创建、对象
缓存、度量和绘制仍由通用钩子完成。

## 身份与能力

TinkerBell 身份由主模块中的 `Software\\TinkerBell\\`、
`Cyberworks "TinkerBell"` 强标记，或 `Cyberworks` 与 `TinkerBell` 标记对确认。查询范围
限定在主模块已映射且可读的内存区域，结果在进程生命周期内缓存。

DAT 归档名、`render.dll` 和其他通用文件名不参与引擎定案。

## 文件入口

- `tinkerbell_compat.cppinc`：模块身份扫描和策略查询。
- `hook_policy.cpp`：包含本模块并调用兼容策略。
- `HookPolicy::ShouldAttach`：决定 `CreateFontW` 与 `CreateFontIndirectW` 的挂接。
- `HookPolicy::ShouldPassThroughUntrackedSelectObject`：控制未跟踪字体选择。

## 实现原理

1. 首次策略查询在主模块可读映像范围内收集 TinkerBell/Cyberworks 强标记。
2. `IsCompatibilityActive` 组合引擎身份和 `EnableTinkerBellHook`。
3. 安装阶段通过 `HookPolicy::ShouldAttach` 决定宽字符字体创建入口是否进入 Detours 事务。
4. `SelectObject` 高频路径只读取缓存策略；未跟踪 HFONT 按配置直接调用真实 API。
5. 已被字体模型跟踪的对象继续经过替换、HDC 选择状态和查询视图维护。

## 功能

- 为匹配的 TinkerBell 进程采用 ANSI/通用字体创建兼容路径。
- 对未被字体替换模型跟踪的 `SelectObject` 调用使用透传策略。
- 保持已跟踪 HFONT 的替换、度量和绘制功能。
- 将引擎识别结果集中提供给通用 `hook_policy`，无需在高频钩子重复扫描。

## 设计约束依据

- 策略模块不拥有字体或 HDC 状态，避免产生第二套缓存和生命周期。
- 标记扫描只执行一次，`SelectObject` 等每帧入口只读取布尔结果。
- 安装点策略与运行时透传策略分别表达，便于组合不同版本的调用特征。
- 通用 DAT、DLL 和目录布局不参与身份判断，策略只对有强模块证据的进程生效。
- 配置关闭时所有策略回到通用默认值，模块本身没有需要回滚的内存资源。

## 配置

- `EnableTinkerBellHook`
- `CompatHookCreateFontW`
- `CompatHookCreateFontIndirectW`
- `CompatSelectObjectTrackedOnly`
- 通用字体名称、度量和字符集配置

## 运行约束

- 模块强标记决定引擎身份，通用目录与文件名不参与策略选择。
- 模块扫描只访问已提交且可读的映像内存。
- 检测只在策略首次查询时执行一次。
- 本模块只返回策略决策，字体对象创建与 DC 跟踪由通用钩子负责。

## 证据与复刻

1. 固定主模块哈希，记录 `Software\\TinkerBell\\`、`Cyberworks "TinkerBell"` 以及
   `Cyberworks` + `TinkerBell` 标记对的命中组合。
2. 对每个组合调用 `HookPolicy::ShouldAttach` 和
   `HookPolicy::ShouldPassThroughUntrackedSelectObject`，保存配置开关与返回值。
3. 分别选择已跟踪 HFONT、未跟踪 HFONT 和非字体 GDI 对象，记录 `SelectObject` 是否透传。
4. 以只有 DAT、`render.dll` 或相似目录的程序作为非目标样本。

| 身份 | 开关 | 对象状态 | 预期策略 |
| --- | --- | --- | --- |
| TinkerBell | 开 | 宽字符创建入口 | 使用适配器定义的挂接集合 |
| TinkerBell | 开 | 未跟踪 HFONT | 调用真实 `SelectObject` |
| TinkerBell | 开 | 已跟踪 HFONT | 进入通用字体模型 |
| TinkerBell | 关 | 任意 | 使用通用默认策略 |
| 非目标 | 任意 | 任意 | 使用通用默认策略 |

证据材料包含标记组合、策略返回值、对象跟踪状态和有限采样日志。记录格式见
[功能证据与复刻流程](../../../../../docs/reproduction.md)。

## 验证

覆盖强标记、标记对、相似 DAT/DLL 布局、模块标记缺失、宽字符创建策略、
未跟踪与已跟踪 `SelectObject`、功能开关和相似目录布局程序。
