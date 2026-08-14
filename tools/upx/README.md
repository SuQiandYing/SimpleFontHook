# UPX 构建工具

本目录固定提供 UPX 5.1.1 Win64，用于压缩 Win32 和 x64 Release DLL。Release 的
Post-Build Event 会先压缩并验证临时副本，成功后再覆盖标准输出目录中的 `winmm.dll`。
需要使用 PDB 调试时，可先对对应 DLL 执行 `upx -d winmm.dll`，或设置环境变量
`SIMPLEFONTHOOK_SKIP_PACK=1` 后重新构建。

- 来源：<https://github.com/upx/upx/releases/tag/v5.1.1>
- 发布包：`upx-5.1.1-win64.zip`
- 发布包 SHA-256：`FA5380BCA4C2718547AAA0134BC0D8A7FA27E102F0AC6371573D60D1C21D64DE`
- `upx.exe` SHA-256：`16A29ADBDF3B6FDE74C290205E24CCEB7BEF1A216941922AA73D2D808A699BBB`

授权条款保存在同目录的 `LICENSE` 与 `COPYING`。
