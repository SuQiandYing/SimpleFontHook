# 诊断与问题定位

## 日志文件

运行日志写入目标程序目录中的 `FontHook.trace.log`。每个进程会话第一次写入时重建
该文件，日志行包含时间、PID、TID 和模块标签。

基础安装、配置和引擎探测日志始终可能出现。详细 API 采样、异常记录和卡顿监视由
`EnableDebugLog` 控制。

## 建议的临时调试配置

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

## 日志标签

| 标签 | 含义 |
| --- | --- |
| `[TRACE]` | 安装、配置版本和 API 采样 |
| `[DEBUG][TextSub]` | 文字映射输入和输出 |
| `[DEBUG][BGI]` | BGI 检测、缓存和度量适配 |
| `[DEBUG][UnityMono]` | Unity Mono 运行时绑定和对象应用 |
| `[DEBUG][CatSystemUnity]` | Unity IL2CPP/TMP 运行时适配 |
| `[RenPy]` | 字体源解析、Python 虚拟文件回调和缓存刷新 |
| `[Tyrano]` | ASAR 检测、Web 字体重定向和桥接 |
| `[DIAG][crash]` | 首次机会异常的有限采样 |
| `[DIAG][hang-watchdog]` | 长时间阶段和窗口无响应探测 |

## 定位流程

1. 使用关闭所有可选兼容功能的配置确认基础加载是否正常。
2. 只启用字体名称替换，区分字体问题与编码、度量或引擎缓存问题。
3. 分别启用字符集伪装、代码页重定向和文字映射，不同时改变多个变量。
4. 查看进程启动阶段的引擎探测结果，确认没有误判。
5. 对照首次错误输出前后的 API 样本，而不是只分析最终截图。
6. 记录目标程序架构、DLL SHA-256、配置文件和最短复现步骤。

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

## 日志提交内容

提供问题材料时包含：目标程序架构、复现步骤、`FontHook.ini`、对应构建哈希、完整的
单次 `FontHook.trace.log` 和错误画面。删除与问题无关的多次旧日志，避免混合不同配置
版本的记录。
