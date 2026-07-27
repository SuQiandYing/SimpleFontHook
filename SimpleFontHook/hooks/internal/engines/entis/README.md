# EntisGLS 适配器

## 职责

本目录负责 EntisGLS 的位图字体注册、`SGLWindowsFont` 字形栅格化和
`SGLReferenceFont` 字体代理。适配器将引擎内部字体对象连接到通用 HFONT 替换路径，
并支持配置版本驱动的字体刷新。

## 为什么需要这层适配

EntisGLS 可以从 BMF 位图字体创建内部对象，并通过 `SGLWindowsFont` 和
`SGLReferenceFont` 自己管理 HDC、HFONT、样式与字形栅格。通用字体创建钩子能够影响
单个 Win32 调用，却无法把位图字体分支、引用对象和内部生命周期组织成一致状态。

适配器把 BMF 注册导向 Windows 字体实现，为引用字体建立代理对象，并把替换 HFONT 的
选择范围限制在一次引擎栅格调用内。这样既覆盖引擎内部对象，又保持原对象拥有的样式和
析构时序。

## 检测条件

主模块需要同时提供字体库存证据和位图加载器证据：

- 导出 `SGLFont::m_pFontStock` 对应符号。
- 导出 `SGLBitmapFontLoader::m_RuntimeClass`，或映像中存在对应的运行时类名。

完整运行时对象适配面向 Win32 MSVC 布局；其他架构保留统一调用入口。

## 文件结构

- `entis_font.cppinc`：聚合入口。
- `entis_state.cppinc`：PE 映像视图、检测、内存范围检查和日志。
- `entis_bitmap_font.cppinc`：位图字体注册函数定位与入口处理。
- `entis_runtime.cppinc`：RTTI/vtable 解析、Windows 字体代理、字形栅格钩子和配置通知。

## 实现原理

1. 检测模块同时验证字体库存符号与位图加载器类证据。
2. 位图注册模块定位 BMF 注册入口，使匹配请求进入 `SGLWindowsFont` 路径。
3. RTTI 与 vtable 解析器定位 `SetStyle`、析构和字形栅格方法，并校验所有函数地址。
4. `SGLReferenceFont` 首次使用时获得对应 Windows 字体代理，代理同步样式并按原对象键控。
5. 字形栅格入口保存 HDC 状态、选择当前替换 HFONT、调用引擎方法，然后恢复原状态。
6. 配置通知发布版本，渲染线程在受控入口观察并应用当前字体状态。

## 功能

- 定位位图字体注册入口，使 BMF 请求进入 `SGLWindowsFont` 字体路径。
- 通过 RTTI 解析 `SGLWindowsFont` 与 `SGLReferenceFont` 的析构、`SetStyle` 和
  字形栅格方法。
- 为引用字体对象创建对应 Windows 字体代理，并同步样式和生命周期。
- 在字形栅格调用范围内选择当前替换 HFONT，调用完成后恢复 HDC 状态。
- 使用 `ConfigVersion` 记录渲染线程观察到的字体配置。

## 设计理由

- BMF 分支在注册阶段转向 Windows 字体，比解析和重建专有位图格式具有更小的数据面。
- 代理对象按原引用字体地址管理，使样式同步和析构释放具有明确所有者。
- HDC 修改只存在于单次栅格调用，防止引擎的其他绘制对象继承临时字体状态。
- RTTI、vtable 和对象内存逐层校验，能力缺失时保留引擎原生字体路径。
- 完整对象布局面向 Win32 MSVC ABI，架构判断位于能力阶段而非引擎身份阶段。

## 配置

- `EnableEntisHook`
- `EntisDisableBitmapFonts`
- `EntisRefreshFontOnSwitch`
- 通用字体名称、字符集和度量配置

## 运行约束

- PE 节、RTTI、vtable、函数地址和对象内存都经过范围与保护属性校验。
- 引用字体代理按原对象地址管理，并在对应析构路径释放。
- HDC 字体替换限定在单次栅格调用范围内。
- Detours 挂接使用统一安装事务，运行时准备先于事务提交。
- 共享代理表由互斥锁保护，持锁范围只覆盖容器访问。

## 验证

覆盖位图字体、Windows 字体、引用字体、RTTI 导出路径、特征扫描路径、功能开关、字体
热切换、对象析构、Win32 Release 和非 Entis 程序。
