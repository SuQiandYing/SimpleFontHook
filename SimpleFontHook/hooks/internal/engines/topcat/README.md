# TopCat 字体适配

## 范围

TopCat 适配器仅针对已识别的 x86 主模块，处理其直接通过 GDI 导入表调用的
`CreateFontIndirectA` 和 `GetTextMetricsA`。它不解析文本编码，不修改 `TCD3` 资源，
也不接管游戏自己的 UI、字形贴图或绘制缓存。

字体创建请求转发给通用 `newCreateFontIndirectA`，由通用字体 Hook 应用当前字体名称、
字符集和度量配置。度量保护只作用于已确认的 TopCat `GetTextMetricsA` 调用返回点：
把异常宽字形归一化，并将宽度和高度限制在 560 像素以内。

## 安装条件

1. `EnableTopCatHook` 已启用。
2. 主模块与 `SET_FONT` 契约、根目录 `*.TCD` 的 `TCD3` 头满足 TopCat 身份策略；模块中的
   `TOPCAT` 标记作为补充证据。
3. 主模块是 x86 PE，且 `CreateFontIndirectA`、`GetTextMetricsA` 导入及其直接调用点均能
   在有界扫描中定位。
4. 两个 IAT 槽均成功替换；第二个槽写入失败时恢复第一个槽。

条件不满足时保留原始调用。适配器只改两个经确认的 IAT 槽，不改写游戏代码段，也不安装
私有函数 Detours。x64 主模块不启用此适配器。

## 字体切换边界

配置切换只影响之后经过字体创建 Hook 的请求。适配器不会枚举游戏窗口、清理 TopCat
内部缓存、重建旧 `HFONT`、清空字形贴图或请求游戏重绘。因此，已经创建并缓存的菜单、
按钮、对话框字体可以继续显示原样；这不表示 GDI 字体 Hook 没有生效。

`font-call decision=wrapper-hit` 表示一次 TopCat 字体创建请求到达适配器；
`font-created` 记录通用 Hook 创建后的实际字体。它们不能证明所有游戏 UI 都使用这条
GDI 路径。若相关界面仍未变化，应将其视为游戏 EXE 的渲染/缓存边界，不通过猜测缓存
地址或跨线程强制刷新来改变游戏状态。

## 配置与日志

| 配置键 | 默认值 | 说明 |
| --- | ---: | --- |
| `EnableTopCatHook` | `1` | 启用 TopCat x86 GDI 导入适配 |

重点查看 `[DEBUG][TopCat]`：

- `phase=identity`：身份判定证据。
- `phase=capability`：PE 架构、导入和直接调用点定位结果。
- `phase=install decision=active`：两个 IAT wrapper 已安装。
- `phase=font-call`：实际字体创建请求是否经过 wrapper，以及创建后的字体名。
- `phase=metrics`：TopCat 调用点度量是否被保护性归一化。

安装成功只说明目标 GDI 调用链已接入，不代表所有 UI 都通过 GDI 绘制，也不代表游戏会
在运行中重建已有界面缓存。
