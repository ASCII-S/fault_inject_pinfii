# saveInstcategory 工具

## 概述

`saveInstcategory` 是一个高级指令过滤和保存工具，可以根据用户指定的指令类型筛选指令，并保存详细的指令信息（包括地址、助记符、反汇编、寄存器等）。它支持静态分析和动态频率分析两种模式。

## 源文件

- **文件路径**: `saveInstcategory.cpp`
- **代码行数**: 536行
- **复杂度**: 高

## 核心功能

### 基本功能
1. **指令类型筛选**: 支持多种预定义的指令类型过滤
2. **详细信息保存**: 记录每条指令的完整信息
3. **双模式分析**: 支持静态和动态分析
4. **灵活配置**: 可选择是否仅统计内存读写指令

### 支持的指令类型

| 类型 | 说明 | 包含的指令 |
|------|------|-----------|
| **integer** | 整数运算 | ADD, SUB, IDIV, IMUL, SAR, SBB, SHL, SHR |
| **float** | 浮点运算 | ADDSD, MULSD, DIVSD, SUBSD, CVTSI2SD, SQRTSD 等 |
| **div** | 除法运算 | IDIV, DIVSD, DIVSS |
| **mov** | 数据传送 | MOV, MOVAPD, MOVSD_XMM, MOVSX, MOVZX 等 |
| **stack** | 栈操作 | PUSH, POP |
| **cmp** | 比较指令 | CMP, CMPSD_XMM |
| **call_ret** | 调用返回 | CALL_NEAR, RET_NEAR |
| **control_flow** | 控制流 | JMP, JZ, JNZ, JL, JG, JBE 等 |
| **data_transfer** | 数据传送 | LEA |
| **logical** | 逻辑运算 | AND, OR, XOR, NOT, ANDPD, PXOR 等 |
| **other** | 其他 | CDQ, CDQE, UCOMISD, UCOMISS 等 |
| **all** | 所有指令 | 不过滤 |

## 工作原理

### 1. 指令类型初始化
```cpp
void initialize_instruction_types() {
    type_conditions["integer"].insert("ADD");
    type_conditions["integer"].insert("SUB");
    // ...
    type_conditions["float"].insert("ADDSD");
    type_conditions["float"].insert("MULSD");
    // ...
}
```
在程序启动时建立指令类型映射表。

### 2. 指令检查
```cpp
bool check_instruction_type(INS ins) {
    std::string mnemonic = INS_Mnemonic(ins);
    std::string ins_type_value = ins_type.Value();

    if (type_conditions[ins_type_value].find(mnemonic) !=
        type_conditions[ins_type_value].end()) {
        if (only_memory.Value() == "1") {
            return INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);
        }
        return true;
    }
    return false;
}
```

### 3. 信息保存格式
```
args_regmm,args_reg,address,mnemonic,disassembly
```

示例：
```
rbx,,400abc,ADD,add %rax, %rbx
,rax,400c12,IMUL,imul $0x10, %rax
rsp,,400d45,PUSH,push %rbp
```

### 4. 动态分析模式
```cpp
if (da == 1)
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)INS_mnemonic_counter,
                  IARG_PTR, ins,
                  IARG_PTR, new std::string(mnemonic_count_key),
                  IARG_END);
```
运行时回调，统计每条指令的执行频率。

## 使用方法

### 编译
```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/saveInstcategory.so
```

### 运行

#### 静态分析模式（默认）
```bash
# 保存所有整数运算指令
pin -t obj-intel64/saveInstcategory.so \
    -ins_type integer \
    -inst_type_table_file integer_insts.csv \
    -- ./target_program

# 保存所有浮点运算指令（仅内存读写）
pin -t obj-intel64/saveInstcategory.so \
    -ins_type float \
    -only_memory 1 \
    -inst_type_table_file float_mem_insts.csv \
    -- ./target_program
```

#### 动态分析模式
```bash
# 统计整数指令的动态执行频率
pin -t obj-intel64/saveInstcategory.so \
    -ins_type integer \
    -dynamic_analyze 1 \
    -address_count_file integer_freq.csv \
    -- ./target_program
```

### 参数说明

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `-ins_type` | none | 指令类型 (integer/float/mov/stack/all等) |
| `-only_memory` | 1 | 是否仅保存内存读写指令 (0/1) |
| `-dynamic_analyze` | 0 | 是否启用动态分析 (0=静态, 1=动态) |
| `-inst_type_table_file` | none | 静态分析输出文件 |
| `-address_count_file` | none | 动态分析输出文件 |

## 输出格式详解

### 静态分析输出（inst_type_table_file）
CSV 格式，每行一条指令：
```csv
memory_reg,general_reg,address,mnemonic,disassembly
rbp,,0x400a1c,MOV,mov %rbp, %rsp
,rax,0x400a1f,ADD,add $0x8, %rax
rdi,,0x400b23,IMUL,imul (%rdi), %rcx
```

字段说明：
1. **memory_reg**: 内存操作相关寄存器（基址或索引寄存器）
2. **general_reg**: 通用寄存器（写寄存器）
3. **address**: 指令地址（十六进制）
4. **mnemonic**: 助记符
5. **disassembly**: 完整反汇编

### 动态分析输出（address_count_file）
按执行频率降序排列：
```csv
rbp,,0x400a1c,MOV,mov %rbp, %rsp,1523891
,rax,0x400a1f,ADD,add $0x8, %rax,892341
rdi,,0x400b23,IMUL,imul (%rdi), %rcx,234
```
最后一列为执行次数。

## 应用场景

### 1. 故障注入目标生成
```bash
# 生成所有整数运算指令（仅内存操作）
pin -t obj-intel64/saveInstcategory.so \
    -ins_type integer \
    -only_memory 1 \
    -inst_type_table_file targets.csv \
    -- ./program

# 从 CSV 中随机选择注入目标
# targets.csv 可作为故障注入的候选指令池
```

### 2. 热点指令分析
```bash
# 动态分析找出最频繁执行的浮点指令
pin -t obj-intel64/saveInstcategory.so \
    -ins_type float \
    -dynamic_analyze 1 \
    -address_count_file hotspots.csv \
    -- ./program

# 查看前10个热点
head -n 10 hotspots.csv
```

### 3. 指令覆盖率分析
```bash
# 静态：所有可能执行的除法指令
pin -t obj-intel64/saveInstcategory.so \
    -ins_type div \
    -inst_type_table_file div_static.csv \
    -- ./program

# 动态：实际执行的除法指令
pin -t obj-intel64/saveInstcategory.so \
    -ins_type div \
    -dynamic_analyze 1 \
    -address_count_file div_dynamic.csv \
    -- ./program

# 对比两个文件，查看覆盖率
```

### 4. 架构相关分析
识别程序对特定指令集的使用：
```bash
# 查找所有 SIMD 指令
pin -t obj-intel64/saveInstcategory.so \
    -ins_type float \
    -only_memory 0 \
    -inst_type_table_file simd.csv \
    -- ./program
```

## 高级特性

### 1. 寄存器信息提取
工具智能提取寄存器信息：
- **内存操作**: 提取基址寄存器或索引寄存器
- **通用操作**: 随机选择一个写寄存器（排除标志寄存器）
- **无效寄存器**: 自动跳过

```cpp
if (INS_IsMemoryWrite(ins) || INS_IsMemoryRead(ins)) {
    REG reg = INS_MemoryBaseReg(ins);
    if (!REG_valid(reg)) {
        reg = INS_MemoryIndexReg(ins);
    }
    mflag = 1;  // 标记为内存操作
}
else {
    int numW = INS_MaxNumWRegs(ins), randW = 0;
    if (numW > 1)
        randW = rand() % numW;
    reg = INS_RegW(ins, randW);
    // 跳过标志寄存器
    if (numW > 1 && (reg == REG_RFLAGS || reg == REG_FLAGS || reg == REG_EFLAGS))
        randW = (randW + 1) % numW;
}
```

### 2. 过滤配置
与 `instcategory` 类似，支持编译时配置：
- `NOBRANCHES`: 排除分支指令
- `NOSTACKFRAMEOP`: 排除栈操作
- `ONLYFP`: 仅浮点指令

### 3. 日志系统
```cpp
#define SaveMsgLog(msg)  if (logFile.is_open()) { \
    logFile << msg << std::endl; \
}
```
可选的日志输出，用于调试。

## 性能考虑

### 静态模式
- **开销**: 中等（约 2-5倍运行时间）
- **内存**: 与程序大小成正比
- **适用**: 快速生成候选指令列表

### 动态模式
- **开销**: 较高（约 5-20倍运行时间）
- **内存**: 需要维护频率计数器
- **适用**: 深入分析热点指令

## 代码关键点

### 指令类型映射
```cpp
std::unordered_map<std::string, std::set<std::string>> type_conditions;
```
使用 `unordered_map` 提升查找性能。

### 动态频率统计
```cpp
std::map<std::string, int> iteration_map;
```
Key 是指令的唯一标识（包含地址和反汇编），Value 是执行次数。

### 排序输出
```cpp
void SaveSortedMapToFile(const std::map<std::string, int>& input_map,
                          const std::string& output_filename) {
    std::vector<std::pair<std::string, int>> sorted_map(input_map.begin(),
                                                          input_map.end());
    std::sort(sorted_map.begin(), sorted_map.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;  // 按频率降序
              });
    // ...
}
```

## 典型工作流

### 故障注入准备
```bash
# 步骤1: 生成整数指令候选池
pin -t obj-intel64/saveInstcategory.so \
    -ins_type integer -only_memory 1 \
    -inst_type_table_file int_targets.csv -- ./program

# 步骤2: 统计指令总数
wc -l int_targets.csv

# 步骤3: 从中随机选择注入目标
# （可用 Python/shell 脚本处理）

# 步骤4: 使用 randomInst 或 faultinjection 执行注入
```

### 性能热点定位
```bash
# 找出执行最频繁的浮点指令
pin -t obj-intel64/saveInstcategory.so \
    -ins_type float -dynamic_analyze 1 \
    -address_count_file float_hotspots.csv -- ./program

# 分析结果
head -n 20 float_hotspots.csv
```

## 与其他工具对比

| 工具 | 粒度 | 输出内容 | 适用场景 |
|------|------|---------|---------|
| **instcategory** | 类别+操作码 | 列表 | 快速了解组成 |
| **saveInstcategory** | 单条指令 | 详细信息 | 深度分析/故障注入 |
| **randomInst** | 单条指令 | 一条 | 随机选择 |

## 扩展建议

1. **添加更多指令类型**: 扩展 `type_conditions` 映射
2. **输出格式选项**: 支持 JSON、XML 等格式
3. **可视化**: 生成指令分布图表
4. **并行分析**: 支持多线程提升性能

## 注意事项

1. **CSV 解析**: 输出包含逗号，需要正确解析 CSV
2. **内存使用**: 动态模式可能消耗大量内存
3. **过滤逻辑**: `only_memory` 和指令类型是 AND 关系
4. **寄存器随机性**: 多个写寄存器时会随机选择一个

## 相关文件

- 源代码: `/home/tongshiyu/pin/source/tools/pinfi/saveInstcategory.cpp`
- 依赖: `utils.h`, `pin.H`
- 配置: `config_pintool.h`
