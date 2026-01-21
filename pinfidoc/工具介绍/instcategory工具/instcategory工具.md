# instcategory 工具

## 概述

`instcategory` 工具用于收集和分类程序执行过程中的指令类别（Category）和操作码（Opcode/Mnemonic）。它可以帮助开发者了解程序的指令组成和分布情况。

## 源文件

- **文件路径**: `instcategory.cpp`
- **代码行数**: 123行
- **复杂度**: 中等

## 核心功能

### 基本功能
- 按指令类别分组统计操作码
- 过滤掉分支指令（可配置）
- 输出每个类别下的所有唯一操作码
- 生成结构化的分类报告

### 配置选项
- `INCLUDEALLINST`: 包含所有指令
- `NOBRANCHES`: 排除分支和返回指令
- `NOSTACKFRAMEOP`: 排除栈帧操作指令
- `ONLYFP`: 仅包含浮点指令

## 工作原理

### 1. 指令分类
```cpp
VOID CountInst(INS ins, VOID *v)
{
    if (!isValidInst(ins))
        return;

    string cate = CATEGORY_StringShort(INS_Category(ins));
    if (category_opcode_map.find(cate) == category_opcode_map.end()) {
        category_opcode_map.insert(
            std::pair<string, std::set<string>* >(cate, new std::set<string>)
        );
    }
    category_opcode_map[cate]->insert(INS_Mnemonic(ins));
}
```

### 2. 数据结构
```cpp
static std::map<string, std::set<string>* > category_opcode_map;
```
- **外层 map**: 指令类别 → 操作码集合
- **内层 set**: 去重的操作码集合
- 自动按字母序排序

### 3. 过滤逻辑
```cpp
#ifdef NOBRANCHES
  if(INS_IsBranch(ins) || !INS_HasFallThrough(ins)) {
    return;  // 跳过分支和返回指令
  }
#endif
```

## 使用方法

### 编译
```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/instcategory.so
```

### 运行
```bash
# 基本用法（排除分支指令）
pin -t obj-intel64/instcategory.so -- /path/to/target_program

# 指定输出文件
pin -t obj-intel64/instcategory.so -o mycategory.txt -- /path/to/target_program
```

### 参数说明
- `-o <filename>`: 指定输出文件名（默认: `pin.instcategory.txt`）

### 输出示例
```
BINARY
    ADD
    AND
    CMP
    MOV
    OR
    SUB
    XOR
DATAXFER
    LEA
    MOV
    MOVSD_XMM
    MOVZX
FP
    ADDSD
    DIVSD
    MULSD
    SUBSD
LOGICAL
    AND
    OR
    XOR
...
```

## 指令类别说明

Pin 框架将指令分为多个类别，常见类别包括：

| 类别 | 说明 | 示例操作码 |
|------|------|-----------|
| **BINARY** | 二进制算术运算 | ADD, SUB, IMUL, IDIV |
| **DATAXFER** | 数据传送 | MOV, LEA, PUSH, POP |
| **LOGICAL** | 逻辑运算 | AND, OR, XOR, NOT |
| **FP** | 浮点运算 | ADDSD, MULSS, DIVPD |
| **SHIFT** | 位移操作 | SHL, SHR, SAR, ROR |
| **COND_BR** | 条件分支 | JZ, JNZ, JL, JG |
| **UNCOND_BR** | 无条件分支 | JMP, CALL, RET |
| **CONVERT** | 类型转换 | CVTSI2SD, CVTTSD2SI |
| **STRINGOP** | 字符串操作 | MOVS, CMPS, SCAS |

## 应用场景

### 1. 程序特征分析
了解程序的计算特征：
- 整数运算密集型
- 浮点运算密集型
- 内存访问密集型
- 控制流密集型

### 2. 故障注入目标选择
根据指令分布选择故障注入的目标类型：
```bash
# 分析程序指令组成
pin -t obj-intel64/instcategory.so -o category.txt -- ./target

# 查看结果，决定注入哪类指令
cat category.txt
```

### 3. 优化方向指导
通过分析指令类别，识别优化机会：
- 大量类型转换 → 考虑算法优化
- 频繁分支 → 考虑分支预测优化
- 大量浮点运算 → 考虑 SIMD 优化

### 4. 架构研究
研究不同架构下的指令使用模式

## 配置选项详解

### NOBRANCHES（默认启用）
```cpp
#define NOBRANCHES
```
- 过滤掉所有分支指令和没有 fallthrough 的指令
- 适合关注数据处理指令
- 减少输出噪音

### NOSTACKFRAMEOP
```cpp
#define NOSTACKFRAMEOP
```
- 过滤掉栈帧操作（PUSH, POP）
- 关注核心计算逻辑
- **注意**: 必须与 NOBRANCHES 配合使用

### ONLYFP
```cpp
#define ONLYFP
```
- 仅统计浮点指令
- 分析浮点密集型程序
- 评估 FPU 使用情况

### INCLUDEALLINST
```cpp
#define INCLUDEALLINST
```
- 包含所有指令，不进行过滤
- 完整的指令全景图
- 输出数据量较大

## 代码关键点

### 使用 set 自动去重
```cpp
category_opcode_map[cate]->insert(INS_Mnemonic(ins));
```
- `std::set` 自动去重
- 同一类别下相同操作码只记录一次
- 自动按字母序排序

### 动态创建集合
```cpp
if (category_opcode_map.find(cate) == category_opcode_map.end()) {
    category_opcode_map.insert(
        std::pair<string, std::set<string>* >(cate, new std::set<string>)
    );
}
```
- 首次遇到新类别时创建对应的 set
- 避免预先定义所有可能的类别

### 输出格式化
```cpp
for (std::map<string, std::set<string>* >::iterator it = category_opcode_map.begin();
     it != category_opcode_map.end(); ++it) {
    OutFile << it->first << std::endl;  // 类别名
    for (std::set<string>::iterator it2 = it->second->begin();
         it2 != it->second->end(); ++it2) {
        OutFile << "\t" << *it2 << endl;  // 操作码（缩进）
    }
}
```

## 与其他工具的区别

| 工具 | 统计内容 | 输出详细度 | 适用场景 |
|------|---------|-----------|---------|
| **instcount** | 指令总数 | 一个数字 | 快速了解程序规模 |
| **instcategory** | 类别和操作码 | 分类列表 | 了解指令组成 |
| **saveInstcategory** | 特定类型指令详情 | 每条指令 | 深入分析特定类型 |

## 扩展建议

### 添加频率统计
可以修改代码，记录每个操作码出现的次数：
```cpp
std::map<string, std::map<string, int>> category_opcode_count;
```

### 添加百分比统计
计算每个类别占总指令的百分比

### 导出为 JSON/CSV
便于后续数据分析和可视化

## 典型输出分析

### 示例1: 计算密集型程序
```
BINARY
    ADD
    IMUL
    SUB
FP
    ADDSD
    MULSD
    DIVSD
```
→ 大量算术运算，适合 SIMD 优化

### 示例2: 内存密集型程序
```
DATAXFER
    LEA
    MOV
    MOVSD_XMM
    MOVZX
```
→ 大量数据传送，可能有缓存优化空间

## 相关文件

- 源代码: `/home/tongshiyu/pin/source/tools/pinfi/instcategory.cpp`
- 依赖: `utils.h` (提供 `isValidInst` 等辅助函数)

## 注意事项

1. **内存使用**: 对于大型程序，可能会收集大量不同的操作码
2. **不统计频率**: 只记录是否出现，不记录出现次数
3. **静态 vs 动态**: 只统计实际执行的指令，不包括未执行的代码路径

## 故障注入工作流中的位置

```
1. [instcount] 统计总指令数
2. [instcategory] 分析指令组成  ← 当前工具
3. [saveInstcategory] 保存目标指令详情
4. [randomInst] 随机选择注入点
5. [faultinjection] 执行故障注入
```
