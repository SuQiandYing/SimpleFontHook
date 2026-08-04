# Unity 适配器

## 职责

Unity 的 TextMeshPro 和 `UI.Text` 不经过 GDI 字体创建路径。本目录通过 Mono 或
IL2CPP 运行时接口创建替换字体资源，并在 Unity 窗口线程上应用到托管对象。

## 运行时适配覆盖范围

Unity 文本组件保存的是托管 `Font`、TMP 字体资源、材质和图集，不是 Win32 `HFONT`。
字体资源还受 GC、Unity 对象生命周期、TMP 版本差异和主线程限制约束。文件重定向或 GDI
钩子不会改变已存活组件持有的对象，也不会自动保证图集、材质和字符表属于同一字体版本。

适配器通过 Mono 或 IL2CPP 元数据确认能力，在运行时构建 Unity 能识别的字体资源，并把
应用动作调度到 Unity 窗口线程。所有跨调用持有的托管对象由运行时句柄固定。

## 两条运行时路径

| 路径 | 聚合入口 | 正向检测 |
| --- | --- | --- |
| Mono | `unity_mono.cppinc` | 存在 `UnityPlayer.dll`，不存在 `GameAssembly.dll`，并存在受支持的 Mono 运行时 DLL |
| IL2CPP | `unity_tmp.cppinc` | 同时存在 `UnityPlayer.dll` 与 `GameAssembly.dll`，随后解析必需 IL2CPP 导出和 TMP 类型 |

UnityPlayer 与 Mono/IL2CPP 运行时组合负责框架身份，最终能力仍需成功解析运行时导出、
程序集、类型和方法。资源后缀与游戏数据文件名不参与身份判断，探测结果在进程生命
周期内缓存。

## 文件结构

### Mono

- `unity_mono_state.cppinc`：Mono API、托管句柄、版本和工作线程状态。
- `unity_mono_runtime.cppinc`：运行时导出、程序集、类型和方法绑定。
- `unity_mono_source.cppinc`：源字体读取和 Unity `Font` 创建。
- `unity_mono_legacy*.cppinc`：TMP 静态字形、图集和字体资源构建。
- `unity_mono_modern.cppinc`：TMP 动态字体资源路径。
- `unity_mono_apply.cppinc`：TMP 与 `UI.Text` 对象应用。
- `unity_mono_jit.cppinc`：窄范围 JIT/运行时调用钩子。
- `unity_mono_window.cppinc`：窗口消息分派、重试和退出。

### IL2CPP

- `unity_il2cpp_state.cppinc`：IL2CPP API、绑定、托管根和版本状态。
- `unity_il2cpp_runtime.cppinc`：导出解析、类型与方法绑定。
- `unity_il2cpp_font_asset.cppinc`：Unity `Font`、TMP 字体资源和材质构建。
- `unity_il2cpp_apply.cppinc`：遍历并应用到存活的 TMP 文本对象。
- `unity_il2cpp_window.cppinc`：Unity 线程消息分派、应用线程和退出。

## 资源构建原理

### Mono

1. 绑定 Mono 导出、domain、程序集、类型、字段和方法，并按签名候选适配运行时差异。
2. 从系统或本地字体文件创建 Unity `Font`，以 GC handle 固定来源对象。
3. 现代 TMP 使用动态字体资源和运行时字符填充；字符集合在会话内单调增长，减少图集
   反复重排造成的 UV 失效。
4. 静态 TMP 路径构建静态字形表、Alpha8 图集、材质和 kerning 数据，再调用其定义刷新入口。
5. JIT/运行时窄钩子在文本对象赋值前提供当前资源，常规对象扫描作为能力回退。

### IL2CPP

1. `UnityPlayer.dll` 与 `GameAssembly.dll` 确认运行时组合，IL2CPP 导出和类查询确认能力。
2. 通过运行时 API 创建 Unity `Font`、TMP 字体资源与基础材质，并固定托管根。
3. 在 Unity 线程枚举存活 TMP 对象，应用字体资源、材质和必要的 fallback 列表。
4. 方法入口只依赖已验证的 `MethodInfo` 稳定前缀，其他对象布局通过运行时 API 访问。

## 应用流程

1. 检测目标运行时并安装必要的消息或运行时钩子。
2. 解析 Mono/IL2CPP API 和 TMP/Unity 类型。
3. 根据 `Config::ConfigVersion` 和当前字体创建替换字体资源。
4. 使用 GC 句柄固定需要跨调用保存的托管对象。
5. 将应用请求投递到 Unity 窗口线程。
6. 将当前资源应用到存活对象，并把替换字体加入必要的 TMP fallback 列表。
7. 配置版本变化后创建当前版本资源并重新应用，被当前版本取代的资源按运行时生命周期释放。

## 设计约束依据

- Mono 与 IL2CPP 分为独立状态机，因为导出、元数据、调用 ABI 和对象访问方式不同。
- 字体资源创建与对象应用分开，准备失败时存活组件继续保留原字体。
- Unity 线程消息承担对象修改；工作线程只等待、重试和投递，避免跨线程调用引擎 API。
- GC handle 为跨消息和跨帧对象提供稳定根，本地指针只在单次运行时调用范围使用。
- 持有本地互斥锁时不进入托管运行时，防止回调重入形成锁顺序反转。
- 配置版本贯穿来源字体、图集、材质和应用记录，过期资源不会覆盖当前选择。

## 配置

适配器要求以下条件同时成立：

- `EnableCatSystemUnityHook=1`
- `EnableFontHook=1`
- `EnableFaceNameReplace=1`
- `ForcedFontNameW` 或 `ForcedFontNameA` 非空

## 线程与 GC 不变量

- 工作线程只负责等待、重试和投递，不直接执行要求 Unity 主线程的对象修改。
- 调用托管运行时前确保当前线程已经附加到对应 domain。
- 跨 GC 周期保存的对象必须由 Mono/IL2CPP GC handle 固定。
- 持有本地互斥锁时不得调用 Mono、IL2CPP 或托管方法。
- 退出时先发布停止信号；不得在 `DLL_PROCESS_DETACH` 中等待线程。
- 运行时或字体资源尚未就绪时延迟应用，不得用空资源覆盖原字体。

## 扩展

Unity 版本兼容通过方法签名候选或受控 fallback 扩展，类型和方法解析使用运行时元数据。

## 证据与复刻

1. 固定 UnityPlayer、GameAssembly、Mono/IL2CPP 运行时 DLL 和目标程序集哈希，记录进程
   架构及 `UnityPlayer.dll`/`GameAssembly.dll` 组合。
2. 分别保存 Mono 导出、IL2CPP 导出、程序集、类型、字段和方法签名解析结果；缺少任一必需
   入口时标记能力回退，不以文件后缀确认身份。
3. 建立含 `UI.Text`、静态 TMP、动态 TMP 和存活对象的最小场景，记录创建字体资源、材质、
   图集、fallback 列表、Unity 窗口线程 ID 和 GC handle。
4. 对配置切换执行准备、投递、主线程应用和对象再次枚举，比较 `ConfigVersion` 与资源哈希；
   工作线程不得直接调用托管对象。
5. 分别运行 Mono、IL2CPP、未安装 TMP、运行时导出缺失、字体源缺失和非 Unity 样本。

| 场景 | 预期结果 |
| --- | --- |
| Mono 组合 + API/程序集/类型完整 | Mono 资源创建与 Unity 线程应用 |
| IL2CPP 组合 + 导出/类型完整 | IL2CPP 资源创建与 TMP 对象应用 |
| UnityPlayer 存在但运行时能力缺失 | 保留通用路径，不修改托管对象 |
| 工作线程准备未完成 | 延迟投递，不以空资源覆盖对象 |
| 配置切换 | 当前版本资源应用，GC handle 与线程边界可追踪 |
| 非 Unity 进程 | 不安装 Unity 运行时适配 |

证据目录包含 `[DEBUG][UnityMono]`、`[DEBUG][CatSystemUnity]` 日志、运行时导出摘要、
资源哈希、线程/句柄记录和非目标样本。通用模板见
[功能证据与复刻流程](../../../../../docs/reproduction.md)。

## 验证

覆盖 Mono、IL2CPP、未安装 TMP、运行时导出缺失、功能关闭、热切换和进程退出。
