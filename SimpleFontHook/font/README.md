# 字体表修改模块

`font/` 负责解析和修改 TTF、OTF 与 TTC 的 SFNT 数据。该模块不安装钩子、不读取
游戏文本，也不决定代码页。

## 模块定位

部分引擎直接读取字体文件或 `GetFontData` 返回的 SFNT 字节，字体名称替换和 `HFONT`
替换无法影响这条路径。本模块把字符集能力、垂直度量、字体族名和 `cmap` 别名表达为
结构化字体表修改，让 GDI、FreeType、HarfBuzz、Unity 与 Chromium 看到同一份字体能力。

把字节级修改集中在这里，是为了让钩子和引擎适配器只决定“何时、对哪份字体应用哪种
配置”，而不各自实现偏移、字节序和校验和处理。

## 公共接口

接口定义在 `font_patcher.h` 的 `FontPatcher` 命名空间中。

| 接口 | 职责 |
| --- | --- |
| `IsFontFile` | 验证独立 SFNT 的基本文件头 |
| `IsFontCollection` | 判断是否为 TTC |
| `NormalizeGdiFontData` | 规范化 GDI 导出字体中相对 TTC 的偏移 |
| `FindFontCollectionFaceIndex` | 按字体名称查找 TTC 中的字体面索引 |
| `ExtractFontFromCollectionByName` | 按字体名称从 TTC 重建独立字体 |
| `PatchOS2CodePageRange*` | 修改 `OS/2` 代码页能力位 |
| `PatchVerticalMetrics` | 修改 `hhea` 与 `OS/2` 垂直度量 |
| `PatchNameTableFamily` | 重建 `name` 表并设置唯一字体族名 |
| `PatchCmapAliases` | 重建 Unicode `cmap` 字符到字形的别名 |

## 数据约定

- SFNT 整数使用大端序，统一通过模块内的 `ReadU16BE`、`ReadU32BE`、
  `WriteU16BE` 和 `WriteU32BE` 访问。
- 所有偏移和长度在解引用前进行范围检查，同时防止加法和乘法溢出。
- 原始指针重载只能原地修改已有表；需要增长或重建字体时使用 `std::vector<BYTE>`。
- TTC 中每个字体面都可能拥有独立表目录，不能假设第一个字体面代表整个集合。
- 从 GDI 获取的字体数据可能保留相对原 TTC 的偏移，使用前先调用
  `NormalizeGdiFontData`。

## 实现原理

字体修改以“解析、选择字体面、重建目标表、修正目录、重算校验和”为主线：

1. 先识别独立 SFNT 或 TTC，并为每个候选字体面验证表目录。
2. TTC 按 `name` 表记录给字体名称打分，选出目标字体面后重建为独立 SFNT。
3. 固定长度字段可以原地写入；`name`、`cmap` 等可变长度表在临时缓冲区重建。
4. 表尺寸变化时重新排列对齐后的表数据，并同步目录中的偏移与长度。
5. 提交结果前更新目标表校验和与 `head.checkSumAdjustment`。

临时缓冲区使失败路径保留原始输入；完整重建使所有消费者获得自洽的表目录，而不是
依赖某个渲染库对损坏偏移或校验和的容忍行为。

## 校验和

修改或重建字体表后必须：

1. 更新目录项中的表校验和。
2. 将 `head.checkSumAdjustment` 临时置零。
3. 重新计算整个字体的校验和并写入 `checkSumAdjustment`。
4. 对 TTC 中每个被修改字体面分别处理其表目录。

不得只修改表内容而跳过校验和。部分 GDI 路径可能容忍错误字体，但 FreeType、
HarfBuzz、Unity 或 Chromium 可能直接拒绝加载。

## 修改流程

字体表补丁按以下顺序实现：

1. 验证 SFNT/TTC 头和目标表存在性。
2. 明确该操作能否原地完成；需要增长时重建数据并修正目录偏移。
3. 对所有长度、计数、偏移和对齐执行有界检查。
4. 只在内容实际变化时返回成功修改状态。
5. 更新表校验和和 `checkSumAdjustment`。
6. 为独立字体、TTC、多字体面、缺失目标表和损坏输入建立验证样本。

## 不变量

- 本模块不调用字体钩子，也不依赖引擎适配状态。
- `cmap` 别名只改变 Unicode 到字形的映射，不改变输入文本编码。
- `OS/2` 代码页能力位只影响字体能力声明，不代表文本字节流采用该代码页。
- 失败时保持输入数据可继续使用；需要重建的操作应先在临时缓冲区完成。
