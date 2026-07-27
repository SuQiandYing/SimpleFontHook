# 编译与发布

## 环境

- Visual Studio 2022
- MSVC v143 C++ 工具集
- Windows 10 SDK
- 仓库根目录中的 `detours.h`、`detours.lib` 和 `detours_x64.lib`

解决方案包含 Debug/Release 与 Win32/x64 四组配置，最终 DLL 名固定为 `winmm.dll`。

## 编译命令

直接使用仓库脚本：

```bat
build_x32.bat
build_x64.bat
```

脚本默认执行增量 `Build`，保留 Release LTCG 生成的 IPDB/IOBJ。需要清理全部中间产物时，
显式使用全量入口：

```bat
build_x32.bat rebuild
build_x64.bat rebuild
```

也可以在 Visual Studio Developer Command Prompt 中执行：

```bat
msbuild SimpleFontHook.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /v:minimal
msbuild SimpleFontHook.sln /t:Build /p:Configuration=Release /p:Platform=x64 /v:minimal
```

`Rebuild` 会先删除 IPDB/IOBJ，因此全量入口可能输出
`Previous IPDB not found, fall back to full compilation.`。这是清理后的预期链接过程；日常
编译使用默认 `Build`，链接器即可复用上一轮的 LTCG 状态。

## 产物

| 架构 | DLL | 调试符号 |
| --- | --- | --- |
| Win32 | `Release/winmm.dll` | `Release/winmm.pdb` |
| x64 | `x64/Release/winmm.dll` | `x64/Release/winmm.pdb` |

部署时只需要匹配架构的 DLL。PDB 用于定位崩溃地址，不应替代对应构建的源码和配置记录。

## Release 优化

项目的 Release 配置保持以下体积优化：

- 全程序优化和函数级链接。
- `/Os` 体积优先、`/Gw` 全局数据优化和字符串池。
- 关闭 RTTI。
- `/OPT:REF` 删除未引用符号。
- `/OPT:ICF` 折叠相同 COMDAT。
- 增量 LTCG 在普通 `Build` 中复用 IPDB/IOBJ，全量 `Rebuild` 用于主动清理中间状态。

新增第三方库、大型静态映射、模板实例或调试字符串时，分别记录 Win32 和 x64 DLL
体积变化。不要为了缩小 DLL 删除必要的错误处理、边界检查或代理导出。

## 代理导出检查

`winmm.dll` 同时承担系统 DLL 代理职责。发布前使用 `dumpbin` 检查导出：

```bat
dumpbin /nologo /exports Release\winmm.dll
dumpbin /nologo /exports x64\Release\winmm.dll
```

比较修改前后的导出名称、序号和转发目标。普通字体功能修改不应改变代理导出表。

## 发布检查清单

1. Win32 与 x64 Release 均编译成功。
2. 构建日志没有编译或链接错误。
3. 代理导出名称与序号保持兼容。
4. 记录两个 DLL 的文件大小和 SHA-256。
5. 默认配置关闭详细调试日志和高频采样。
6. 代码修改对应的 README、配置说明和架构说明已经同步。
7. 部署前备份目标目录中已有的 `winmm.dll` 和 `FontHook.ini`。
8. 只把匹配目标进程架构的 DLL 放入目标目录。

## 回滚

回滚时先退出目标程序，再恢复原 `winmm.dll` 和 `FontHook.ini`。不要在进程已加载 DLL
时覆盖文件，也不要混用不同构建的 DLL、PDB 和配置快照。
