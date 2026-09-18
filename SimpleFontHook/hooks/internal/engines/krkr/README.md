# KiriKiri / TVP 适配器

## 职责

保留 `Font.mapPrerenderedFont` 与 `Font.unmapPrerenderedFont` 的原生注册，在确认
TVP 原生方法能力后，将预渲染映射调用转交给同一字体实例的解除映射方法，让后续
实时栅格化进入通用字体钩子。不修改游戏脚本、EXE 文件、TJS 成员名或 TFT 文件。

## 入口与依赖

- `krkr_paths.cppinc`：身份缓存、TVP 插件 ABI 绑定与原生方法分派。
- `KrkrPatchMapPrerenderedFontName`：保留既有安装入口名称，仅做只读身份检查。
- `KrkrMaybeWrapGetProcAddress`：由 `newGetProcAddress` 包装 `V2Link` 入口。
- `KrkrShouldHidePrerenderedFontW`：统一文件分派器的兼容入口，始终透传。
- 复用现有 Detours、`orgGetProcAddress`、配置和通用 GDI 字体模型，不引入 TJS SDK 库。

## 身份、能力与流程

1. 以主模块的 ANSI 或 Unicode `mapPrerenderedFont` 标记做缓存身份检查；路径、
   `.tft`、XP3 后缀不作为身份依据。
2. `V2Link` 在引擎脚本线程传入 TVP exporter。最多保存 16 个不同插件入口，各自使用
   独立 thunk，原参数、HRESULT 和嵌套插件调用保持不变；不在 `GetProcAddress` 中调用 TJS。
3. 通过官方导出签名查询全局对象与 Variant 操作，获取 `Font` 的 map/unmap 成员。
   `TJS_IGNOREPROP` 防止属性 getter 副作用；Variant 构造、清理、类型和对象查询均由引擎执行。
   x86 exporter 查询入口通过栈中立边界识别 `cdecl`（弹出 0 字节）或 `stdcall`
   （弹出 16 字节），随后引用计数、属性获取和原生分派使用对应虚接口约定。
   生成的 TVP 导出辅助函数及 `V2Link` 仍使用 `stdcall`，不随虚接口约定一起切换；
   x64 使用平台统一调用约定。未知栈清理量不继续绑定。
4. exporter 查询函数、所需函数及原生分派入口必须属于主模块，避免持有可卸载插件代码。
   使用引擎创建的临时原生方法核对 `FuncCall`，不把脚本函数或自定义分派器当作原生方法。
5. Detours 仅在能力全部成立后安装；共享 `FuncCall` 内按 map 方法对象指针精确过滤。
   开关开启且参数有效时调用原生 unmap 方法，传入原始字体实例、空参数和原结果指针。
   其他方法、非默认成员调用、无实例、参数不足与开关关闭均调用原始分派。
6. 不隐藏 TFT，松散字体和归档字体无需分别重定向。接口缺失、类尚未就绪、分派不匹配
   或 Detours 失败均保留引擎原行为；后续插件链接可重试，最多 32 次。

## 不变量与生命周期

- 不重命名 `mapPrerenderedFont`，不依赖脚本具有 `try/catch`，不匹配或改写 `unmap` 子串。
- map/unmap 对象在安装成功后持有引擎引用，随进程保留；失败路径释放临时引用。
- 不在字体热路径扫描模块、读写磁盘或分配资源；其他原生方法只增加一次指针判断。
- 插件槽锁只保护槽分配，不在持锁期间调用插件或引擎。
- 不在 `DLL_PROCESS_DETACH` 调用 TJS、等待线程或卸载运行时钩子。
- 字体、代码页与文字映射保持独立；不新增样本专用识别或修改游戏文件。

## 配置

- `EnableKrkrHook=1`：启用身份检查和运行时桥接。
- `KrkrDisablePrerenderedFonts=1`：已安装的分派在每次 map 调用时读取此开关；关闭时
  调用原 map，开启时调用 unmap。已有配置的声明、默认值、持久化保持不变。
- 通用字体设置继续控制实时渲染的字体名称和度量。

开关只影响之后的映射调用，不自动重放历史 `mappfont`，也不遍历所有字体实例。
游戏已映射的字体需由后续映射调用解除；从开启切到关闭不会自动恢复旧 TFT 映射。
没有插件链接、插件链接晚于字体映射、主模块之外的 TJS runtime、运行时重建 `Font` 类
以及绕开原生 map 方法的字体路径均不保证预渲染替换，保持透传而非破坏启动。
不经过通用 GDI 钩子的栅格器也不在本适配器的字体替换保证范围内。

## 证据与复刻

官方 ABI 依据：
[TJS 分派接口](https://github.com/krkrz/krkrz/blob/master/tjs2/tjsInterface.h)、
[TVP 导出桥](https://github.com/krkrz/krkrz/blob/master/base/win32/FuncStubs.cpp)、
[原生方法分派](https://github.com/krkrz/krkrz/blob/master/tjs2/tjsNative.cpp)、
[Font 的 map/unmap](https://github.com/krkrz/krkrz/blob/master/visual/LayerIntf.cpp)。

上述源码证明接口契约，不等于某个游戏版本已运行验证。版本特例与观察记录写入
[诊断文档](../../../../../docs/diagnostics.md)。

## 扩展与验证

| 场景 | 预期 |
| --- | --- |
| 有效 map、开关开启 | 原成员仍存在，原实例执行 unmap，结果由原生分派清理 |
| 开关关闭或非 map 调用 | 原始分派接收原参数 |
| 缺少能力、非目标程序、序号导出查询 | 不安装、不改名、不隐藏文件 |
| 多插件、重复查询、槽满、嵌套链接 | 原 V2Link 不串线，槽满透传 |
| TFT 松散文件或 XP3 内资源 | 不改变 Win32 文件可见性 |
| 参数不足、空实例 | 保留引擎校验及错误码 |
| x86 cdecl / stdcall 虚接口 | 查询后栈平衡，引用计数、PropGet 和 FuncCall 使用同一约定 |

扩展其他运行时前需单独验证导出、调用约定、对象寿命和初始化时机。C++ 变更检查
Win32/x64 Release；离线接口夹具只验证分派契约，不能替代游戏中的字体渲染验证。
