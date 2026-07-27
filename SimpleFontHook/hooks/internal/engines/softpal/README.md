# Softpal 适配器

## 职责

本目录负责 Softpal 的默认位图字体选项和 `Pal.dll` 字体类型。适配器把引擎默认选项
映射到系统字体分支，使通用 GDI 字体钩子能够提供当前选择的字体。

## 为什么需要这层适配

Softpal 在进入 GDI 字体创建前通过 Pal 字体类型和 `DEFAULT_FONT.DAT` 选择位图或系统字体。
默认类型 `4` 会让文本使用引擎位图资源，通用 GDI 钩子只能看到旁路调用。字体切换因此
需要先让引擎选择系统字体类型，再由通用模型替换实际字体对象。

适配器在 Pal 导出边界规范化类型，并在文件分派器中控制默认位图字体资源的只读可见性。
两条路径都只负责分支选择，字体度量、字符集和字体数据仍由通用模块提供。

## 身份与能力

Softpal 身份由 `PalFontBegin`、`PalFontSetType`、`PalFontGetType` 三个运行时导出确认。
主模块包含完整导出名称契约时可提前确认；`Pal.dll` 加载后使用真实导出表完成最终绑定。
动态模块尚未加载时保留导出重试路径。

PAC 后缀、`Pal.dll` 文件名和 `DEFAULT_FONT.DAT` 只参与候选定位与功能路由，不构成独立
身份条件。

## 文件入口

- `softpal_default_font.cppinc`：引擎检测、Pal 字体类型处理、默认字体文件分类和策略接口。
- `SoftpalInstallPalFontHooks`：安装 `PalFontBegin` 与 `PalFontSetType` Detours。
- `engine_file_dispatch.cppinc`：处理 `DEFAULT_FONT.DAT` 文件可见性。
- 通用字体创建和字体数据钩子：查询 Softpal 的自然宽度与数据替换策略。

## 实现原理

1. 主模块标记或已加载 `Pal.dll` 的真实导出表确认完整字体契约。
2. `SoftpalInstallPalFontHooks` 在导出可用后挂接 `PalFontBegin` 与 `PalFontSetType`。
3. 字体会话开始或类型变化时读取当前类型，把默认值 `4` 规范化为系统字体值 `1`。
4. 游戏根目录 `DEFAULT_FONT.DAT` 的只读存在性由统一文件分派器按配置发布。
5. 系统字体分支进入 GDI 后，通用钩子按 Softpal 策略使用自然替换宽度和字体数据。
6. 动态模块尚未加载时保留可重试状态，由模块加载路径再次尝试绑定导出。

## 功能

- 解析 `PalFontBegin`、`PalFontSetType`、`PalFontGetType` 导出。
- 将 Softpal 默认字体类型 `4` 映射为系统字体类型 `1`。
- 在字体会话开始和字体类型变化时规范化当前字体类型。
- 为游戏根目录内的 `DEFAULT_FONT.DAT` 只读请求提供位图字体开关行为。
- 使用替换字体的自然宽度和字体数据查询结果参与通用 GDI 路径。

## 设计理由

- Pal 字体类型是引擎公开契约，修改该边界比解析专有 DAT 位图格式更稳定。
- 只映射值 `4`，其他用户选择与引擎字体类型保持原语义。
- 身份要求完整导出集合；`Pal.dll` 文件名、PAC 后缀和 DAT 文件名只用于候选定位。
- 导出绑定随模块加载重试，避免在 DLL 尚未进入进程时形成永久否定结果。
- Pal 调用异常被限制在适配器边界，返回值继续遵循引擎函数契约。

## 配置

- `EnableSoftpalHook`
- `SoftpalDisableDefaultFontDat`
- `SoftpalForceDefaultOptionToSystemFont`
- 通用字体名称、度量和字符集配置

## 运行约束

- Pal 导出函数完整解析后提交 Detours 事务。
- Pal 调用使用结构化异常保护，并保留引擎函数的返回值语义。
- `DEFAULT_FONT.DAT` 分类限定在游戏根目录。
- 字体类型映射只处理值 `4`，其他选项直接传递。
- 调试日志使用固定采样上限。

## 验证

覆盖 Pal 启动前后加载、默认字体类型、其他字体类型、`DEFAULT_FONT.DAT`、自然宽度、
字体数据查询、功能开关、导出缺失、共享 PAC 后缀和同名 DLL 的非 Softpal 程序。
