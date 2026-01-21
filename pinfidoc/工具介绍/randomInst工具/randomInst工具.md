# randomInst 工具

## 概述

`randomInst` 是一个用于随机选择目标程序中某条动态指令的工具。它在程序执行到指定的随机指令编号时，记录该指令的详细信息（程序计数器、寄存器、内存操作类型等），主要用于故障注入实验的目标指令选择。

## 源文件

- **文件路径**: `randomInst.cpp`
- **代码行数**: 282行
- **复杂度**: 中等

## 核心功能

### 基本功能
1. **随机指令定位**: 根据给定的随机数，定位到程序执行的第 N 条指令
2. **指令信息提取**: 记录指令的 PC、寄存器、内存操作类型
3. **批量生成**: 支持生成多个不同的随机指令信息文件
4. **智能过滤**: 自动处理无效寄存器和标志寄存器

### 输出信息
- **PC 地址**: 指令的程序计数器
- **寄存器**: 操作的寄存器名称
- **操作类型**: `reg:`（通用寄存器）、`mem:`（内存操作）、无效标记

## 工作原理

### 1. 指令计数
```cpp
static UINT64 allinst = 0;
VOID docount(VOID *ip, VOID *reg_name, UINT32 mflag, INS ins) {
    allinst++;
    // ...
}
```
每执行一条指令，`allinst` 递增。

### 2. 随机数匹配
```cpp
if (randInst.Value() >= allinst && randInst.Value() <= allinst+0) {
    // 找到目标指令
    // 记录信息到文件
}
```
当计数器达到指定的随机数时，记录该指令的信息。

### 3. 信息提取
```cpp
if (mflag == 1) {
    OutFile << "mem:" << (const char *)reg_name << endl;  // 内存操作
}
if (mflag == 0) {
    OutFile << "reg:" << (const char*)reg_name << endl;   // 寄存器操作
}
if (static_cast<int>(mflag) == -1) {
    OutFile << (const char*)reg_name << endl;             // 无效寄存器
}
OutFile << "pc:" << (unsigned long)ip << endl;
```

### 4. 输出格式
文件名：`instruction` 或 `instruction<N>`（N 为序列号）

内容示例：
```
reg:rax
pc:4198764
```
或
```
mem:rbp
pc:4200412
```

## 使用方法

### 编译
```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/randomInst.so
```

### 运行

#### 单次随机选择
```bash
# 选择第 1000 条指令
pin -t obj-intel64/randomInst.so -randinst 1000 -- ./target_program

# 输出文件: instruction
cat instruction
# reg:rcx
# pc:4198900
```

#### 批量生成
```bash
# 生成 10 个不同的随机指令文件
for i in {1..10}; do
    RAND=$((RANDOM % 1000000))
    pin -t obj-intel64/randomInst.so \
        -randinst $RAND \
        -FileNameSeq $i \
        -- ./target_program
done

# 输出文件: instruction1, instruction2, ..., instruction10
```

### 参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `-randinst` | UINT64 | 0 | 随机指令编号（第几条指令） |
| `-FileNameSeq` | UINT64 | 0 | 文件名序号（0=instruction，非0=instruction<N>） |

## 寄存器选择逻辑

### 内存操作指令
```cpp
if (INS_IsMemoryWrite(ins) || INS_IsMemoryRead(ins)) {
    REG reg = INS_MemoryBaseReg(ins);  // 优先基址寄存器
    if (!REG_valid(reg)) {
        reg = INS_MemoryIndexReg(ins);  // 其次索引寄存器
    }
    mflag = 1;  // 标记为内存操作
}
```

### 通用指令
```cpp
else {
    int numW = INS_MaxNumWRegs(ins), randW = 0;
    if (numW > 1)
        randW = rand() % numW;  // 随机选择一个写寄存器

    reg = INS_RegW(ins, randW);

    // 跳过标志寄存器
    if (numW > 1 && (reg == REG_RFLAGS || reg == REG_FLAGS || reg == REG_EFLAGS))
        randW = (randW + 1) % numW;

    if (numW > 1 && REG_valid(INS_RegW(ins, randW)))
        reg = INS_RegW(ins, randW);
    else
        reg = INS_RegW(ins, 0);

    // 检查有效性
    if (!REG_valid(reg)) {
        mflag = -1;  // 标记为无效
    }
}
```

## 应用场景

### 1. 故障注入目标选择
```bash
# 步骤1: 统计总指令数
pin -t obj-intel64/instcount_official.so -o count.out -- ./program
# 假设输出: Count 5000000

# 步骤2: 生成随机数（例如 Python）
python3 -c "import random; print(random.randint(0, 5000000))"
# 输出: 2341892

# 步骤3: 使用 randomInst 定位指令
pin -t obj-intel64/randomInst.so -randinst 2341892 -- ./program

# 步骤4: 查看结果
cat instruction
# mem:rsp
# pc:4200588

# 步骤5: 将此信息传递给 faultinjection 工具
```

### 2. 批量实验准备
```bash
# 生成 1000 个随机注入点
TOTAL_INST=$(grep "Count" count.out | awk '{print $2}')

for i in {1..1000}; do
    RAND=$(python3 -c "import random; print(random.randint(0, $TOTAL_INST))")
    pin -t obj-intel64/randomInst.so \
        -randinst $RAND \
        -FileNameSeq $i \
        -- ./program
done

# 得到 instruction1 ~ instruction1000
```

### 3. 指令抽样分析
随机抽取指令样本，分析程序的典型指令特征。

## 边界处理

### 随机数超出范围
```cpp
VOID Fini(INT32 code, VOID *v)
{
    if (find_flag == 0) {  // 未找到合适的指令
        cout << "allInst:\t" << allinst << endl;
        ofstream OutFile;
        OutFile.open(filename.c_str());
        OutFile << "pc:0" << endl;  // 输出 pc:0 表示未找到
        OutFile.close();
    }
}
```
如果给定的随机数大于总指令数，输出 `pc:0` 作为错误标记。

### 无效寄存器
```cpp
if (!REG_valid(reg)) {
    string *temp = new string("REGNOTVALID: inst " + INS_Disassemble(ins));
    reg_name = temp->c_str();
    mflag = -1;
}
```
标记为 `-1`，输出特殊消息。

### 标志寄存器
```cpp
if (reg == REG_RFLAGS || reg == REG_FLAGS || reg == REG_EFLAGS) {
    string *temp = new string("REGNOTVALID: inst " + INS_Disassemble(ins));
    reg_name = temp->c_str();
    mflag = -1;
}
```
跳过标志寄存器，避免故障注入时产生不可预测的控制流变化。

## 代码关键点

### 指令级插桩
```cpp
VOID CountInst(INS ins, VOID *v)
{
    // 为每条指令插入回调
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount,
                   IARG_INST_PTR,
                   IARG_PTR, reg_name,
                   IARG_UINT32, mflag,
                   IARG_ADDRINT, INS_Address(ins),
                   IARG_END);
}
```
每条指令都会调用 `docount`，因此性能开销较大。

### 保护机制
```cpp
if (find_flag == 0) {  // 保护不被重复写入
    ofstream OutFile;
    OutFile.open(filename.c_str());
    // ...
    OutFile.close();
    find_flag = 1;  // 标记已找到
}
```
防止同一个随机数被多次处理。

### 调试支持
代码中包含大量调试代码（被 `if (0)` 包裹），可以打开用于调试：
```cpp
if (0) {
    OPCODE opcode = INS_Opcode(ins);
    std::string opcodeStr = OPCODE_StringShort(opcode);
    std::ofstream logfile;
    logfile.open("pintool_mylog", std::ios_base::app);
    // ...
}
```

## 性能考虑

### 开销分析
- **每条指令插桩**: 性能开销较高（约 10-50倍）
- **回调频繁**: 每条指令都调用 `docount`
- **适用场景**: 适合短时间运行或小规模程序

### 优化建议
1. **提前终止**: 找到目标后调用 `PIN_Detach()` 停止插桩
2. **条件插桩**: 只在接近目标时插桩
3. **批处理**: 一次运行生成多个随机点

## 典型工作流

### 单目标故障注入
```bash
# 1. 统计指令数
pin -t obj-intel64/instcount_official.so -o count.out -- ./program

# 2. 选择随机点
RAND=$(python3 -c "import random; print(random.randint(0, $(grep Count count.out | awk '{print $2}')))")

# 3. 定位指令
pin -t obj-intel64/randomInst.so -randinst $RAND -- ./program

# 4. 解析结果
if grep -q "pc:0" instruction; then
    echo "随机数超出范围，重新选择"
else
    PC=$(grep "pc:" instruction | cut -d':' -f2)
    REG=$(grep -E "^(reg|mem):" instruction | cut -d':' -f2)
    echo "目标 PC: $PC, 寄存器: $REG"
fi
```

### 批量实验
```bash
#!/bin/bash
TOTAL=$(grep Count count.out | awk '{print $2}')

for i in {1..100}; do
    RAND=$((RANDOM % TOTAL))
    pin -t obj-intel64/randomInst.so \
        -randinst $RAND \
        -FileNameSeq $i \
        -- ./program

    # 检查有效性
    if grep -q "pc:0" instruction$i; then
        echo "Experiment $i: Invalid target"
        rm instruction$i
        ((i--))  # 重试
    else
        echo "Experiment $i: OK"
    fi
done
```

## 与其他工具配合

### 与 faultinjection 配合
```bash
# randomInst 生成目标
pin -t obj-intel64/randomInst.so -randinst 123456 -- ./program

# faultinjection 读取目标并注入
# （faultinjection 工具需要支持读取 instruction 文件）
pin -t obj-intel64/faultinjection.so \
    -fi_function instruction \
    -- ./program
```

### 与 findnextinst 配合
```bash
# randomInst 找到 PC
pin -t obj-intel64/randomInst.so -randinst 123456 -- ./program
PC=$(grep "pc:" instruction | cut -d':' -f2)

# findnextinst 查询详细信息
pin -t obj-intel64/findnextinst.so -pc $PC -- ./program
cat nextpc
```

## 调试技巧

### 开启日志
修改代码，将调试日志的条件改为 `if (1)`：
```cpp
if (1) {  // 原来是 if (0)
    std::ofstream logfile;
    logfile.open("pintool_mylog", std::ios_base::app);
    // ...
}
```

### 打印所有指令
临时修改匹配条件，打印每条指令：
```cpp
if (allinst % 1000 == 0) {  // 每 1000 条打印一次
    cout << "Inst #" << allinst << " at " << std::hex << ip << endl;
}
```

## 注意事项

1. **随机数范围**: 确保随机数不超过总指令数
2. **无效输出**: 检查 `pc:0`，表示未找到有效指令
3. **性能开销**: 对长时间运行的程序影响显著
4. **寄存器随机性**: 多个写寄存器时会随机选择

## 已知问题

1. **IP 值不匹配**: 注释中提到 IP 值和汇编代码中不匹配（可能是 PIE/ASLR 导致）
2. **操作码筛选**: 注释建议不要尝试筛选操作码

## 扩展建议

1. **条件筛选**: 支持仅选择特定类型的指令
2. **范围随机**: 支持在指定范围内随机选择
3. **热点权重**: 根据执行频率加权随机选择
4. **批处理模式**: 一次运行生成多个目标

## 相关文件

- 源代码: `/home/tongshiyu/pin/source/tools/pinfi/randomInst.cpp`
- 依赖: `utils.h`, `config_pintool.h`
- 输出: `instruction` 或 `instruction<N>`
