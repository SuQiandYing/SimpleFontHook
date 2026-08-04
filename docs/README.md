# 项目文档

`docs/` 维护跨模块事实：进程架构、配置契约、证据复刻、构建发布要求、诊断方法和架构决策。
源码目录中的 README 负责解释单个模块的职责与实现原理。这样的分工让稳定规则拥有唯一
权威位置，也让模块文档能够紧贴入口、状态和运行约束。

| 文档 | 内容 |
| --- | --- |
| [architecture.md](architecture.md) | 进程入口、模块接口、数据流、线程和缓存生命周期 |
| [configuration.md](configuration.md) | `FontHook.ini` 配置项、默认值和作用域 |
| [reproduction.md](reproduction.md) | 功能证据等级、样本固定、契约提取、复刻步骤和验证矩阵 |
| [build-and-release.md](build-and-release.md) | Win32/x64 编译、DLL 体积、导出与发布流程 |
| [diagnostics.md](diagnostics.md) | 日志、崩溃、卡顿和兼容问题定位 |
| [adr/README.md](adr/README.md) | 架构决策记录规则和模板 |
| [引擎适配器](../SimpleFontHook/hooks/internal/engines/README.md) | 全部引擎的职责、检测条件、配置和运行约束 |

模块级文档位于对应源码目录。跨模块规则以根目录 `CONTRIBUTING.md` 为准，模块
README 说明模块当前职责、入口、流程、配置与不变量。

## 阅读路径

1. 先读 [architecture.md](architecture.md)，建立代理加载、钩子聚合、配置版本和线程
   生命周期的整体模型。
2. 依据任务进入对应模块 README；引擎相关工作从
   [引擎适配器索引](../SimpleFontHook/hooks/internal/engines/README.md) 开始。
3. 需要确认功能来源或在另一份样本上复现时，按
   [reproduction.md](reproduction.md) 固定输入、记录证据并执行验证矩阵。
4. 涉及用户配置时同时核对 [configuration.md](configuration.md)，涉及构建产物时核对
   [build-and-release.md](build-and-release.md)。
5. 运行异常、兼容判断和日志语义统一参考 [diagnostics.md](diagnostics.md)。

## 内容边界

- README 描述可从当前源码验证的职责、接口、流程、配置和约束。
- `architecture.md` 保存跨模块数据流与生命周期，不复制单个引擎的资源细节。
- `reproduction.md` 定义证据等级和通用复刻流程；模块 README 保存具体符号、输入与预期输出。
- ADR 记录会长期约束多个模块的取舍，局部实现说明留在相应模块 README。
- 单个样本参数和一次性日志进入诊断记录或提交上下文，稳定契约进入对应模块 README。
