# findnextinst 工具

## 概述

`findnextinst` 是一个指令信息查询工具，给定一个程序计数器（PC）地址，它可以查找该指令的详细信息，包括反汇编代码、下一条指令地址、写寄存器列表、内存操作类型、栈操作详情等。该工具主要用于辅助故障注入实验的目标分析。

## 源文件

- **文件路径**: `findnextinst.cpp`
- **代码行数**: 195行
- **复杂度**: 中等

## 核心功能

### 基本功能
1. **指令定位**: 根据 PC 地址精确定位指令
2. **反汇编输出**: 显示指令的汇编代码
3. **后继指令**: 查找下一条指令的地址
4. **寄存器分析**: 列出所有写寄存器
5. **内存操作**: 判断是否为内存读写指令
6. **栈操作分析**: 详细分析栈操作的基址、索引、位移、缩放

### 输出信息
- **thispc**: 当前指令的反汇编
- **nextpc**: 下一条指令的地址
- **regw0, regw1, ...**: 写寄存器列表
- **memory-load/write**: 内存操作类型
- **stackr/stackw**: 栈读写操作
- **base, index, displacement, scale**: 栈操作详情

## 工作原理

### 1. PC 匹配
```cpp
VOID CountInst(INS ins, VOID *v)
{
    stringstream ss;
    ss << INS_Address(ins);

    if (pc.Value() == ss.str()) {
        // 找到匹配的PC
        // 提取指令信息
    }
}
```

### 2. 指令信息提取
```cpp
ofstream OutFile;
OutFile.open("nextpc");
OutFile << "thispc:" << INS_Disassemble(ins) << endl;
OutFile << "nextpc:" << INS_NextAddress(ins) << endl;

// 写寄存器
int numW = INS_MaxNumWRegs(ins);
for(int i = 0; i < numW; ++i) {
    REG write_reg = INS_RegW(ins, i);
    if (REG_valid(write_reg))
        OutFile << "regw" << i << ":" << REG_StringShort(write_reg) << endl;
}

// 内存操作
if (INS_IsMemoryRead(ins))
    OutFile << "memory-load" << endl;
if (INS_IsMemoryWrite(ins))
    OutFile << "memory-write" << endl;
```

### 3. 栈操作分析
```cpp
if (INS_IsStackRead(ins) || INS_IsStackWrite(ins)) {
    if (REG_valid(INS_MemoryBaseReg(ins))) {
        OutFile << "base:" << REG_StringShort(INS_MemoryBaseReg(ins)) << endl;

        if (REG_valid(INS_MemoryIndexReg(ins)))
            OutFile << "index:" << REG_StringShort(INS_MemoryIndexReg(ins)) << endl;
        else
            OutFile << "index:null" << endl;

        OutFile << "displacement:" << INS_MemoryDisplacement(ins) << endl;
        OutFile << "scale:" << INS_MemoryScale(ins) << endl;
    }
}
else {
    OutFile << "nostack" << endl;
}
```

### 4. 栈帧分析
```cpp
ofstream spsize;
spsize.open("spsize");
RTN rtn = INS_Rtn(ins);
if (RTN_Valid(rtn)) {
    RTN_Open(rtn);
    bool meet_rbp = false;
    for (INS ins1 = RTN_InsHead(rtn); INS_Valid(ins1); ins1 = INS_Next(ins1)) {
        // 寻找 push %rbp
        if (OPCODE_StringShort(INS_Opcode(ins1)) == "PUSH") {
            if (meet_rbp) {
                spsize << INS_Disassemble(ins1) << std::endl;
            }
            if (REG_StringShort(INS_RegR(ins1, 0)) == "rbp") {
                meet_rbp = true;
            }
        }

        // 寻找 sub $xxx, %rsp
        if (OPCODE_StringShort(INS_Opcode(ins1)) == "SUB") {
            for (int i = 0; i < INS_MaxNumWRegs(ins1); i++) {
                if (REG_StringShort(INS_RegW(ins1, i)) == "rsp") {
                    spsize << INS_Disassemble(ins1);
                    break;
                }
            }
            break;
        }
    }
    spsize.close();
    RTN_Close(rtn);
}
```
分析函数的栈帧大小（通过查找 `push %rbp` 和 `sub $xxx, %rsp`）。

## 使用方法

### 编译
```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/findnextinst.so
```

### 运行

#### 基本用法
```bash
# 查询 PC=4198764 的指令信息
pin -t obj-intel64/findnextinst.so -pc 4198764 -- ./target_program

# 输出文件: nextpc 和 spsize
cat nextpc
cat spsize
```

#### 与 randomInst 配合
```bash
# 步骤1: 使用 randomInst 找到一个随机指令
pin -t obj-intel64/randomInst.so -randinst 123456 -- ./program
cat instruction
# reg:rax
# pc:4198764

# 步骤2: 使用 findnextinst 查询详细信息
PC=$(grep "pc:" instruction | cut -d':' -f2)
pin -t obj-intel64/findnextinst.so -pc $PC -- ./program

# 步骤3: 查看结果
cat nextpc
cat spsize
```

### 参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `-pc` | string | "pc" | 目标指令的 PC 地址（十进制字符串） |

## 输出文件格式

### nextpc
```
thispc:add %rax, %rbx
nextpc:4198780
regw0:rbx
regw1:rflags
memory-load
stackr:rsp
base:rsp
index:null
displacement:-8
scale:1
```

字段说明：
- **thispc**: 当前指令的反汇编
- **nextpc**: 下一条指令地址（十进制）
- **regwN**: 第 N 个写寄存器
- **memory-load**: 内存读操作
- **memory-write**: 内存写操作
- **stackr/stackw**: 栈读/写操作
- **base**: 基址寄存器
- **index**: 索引寄存器（null 表示无）
- **displacement**: 位移量
- **scale**: 缩放因子

### spsize
```
push %r12
push %r13
sub $0x20, %rsp
```

内容：
- 函数序言中的 push 指令（在 `push %rbp` 之后）
- 栈空间分配指令（`sub $xxx, %rsp`）

## 应用场景

### 1. 故障注入目标验证
```bash
# randomInst 找到目标
pin -t obj-intel64/randomInst.so -randinst 100000 -- ./program
PC=$(grep "pc:" instruction | cut -d':' -f2)

# findnextinst 验证目标
pin -t obj-intel64/findnextinst.so -pc $PC -- ./program

# 检查是否为合适的注入目标
if grep -q "memory-load" nextpc; then
    echo "目标是内存读指令"
fi

if grep -q "stackr" nextpc; then
    echo "目标涉及栈操作"
fi
```

### 2. 指令上下文分析
了解某条指令在程序中的上下文：
```bash
pin -t obj-intel64/findnextinst.so -pc 4198764 -- ./program
echo "当前指令: $(grep thispc nextpc | cut -d':' -f2-)"
echo "下一条指令地址: $(grep nextpc nextpc | cut -d':' -f2)"
```

### 3. 栈帧分析
分析函数的栈布局：
```bash
pin -t obj-intel64/findnextinst.so -pc 4198764 -- ./program
echo "=== 栈帧信息 ==="
cat spsize

# 计算栈大小
if grep -q "sub.*rsp" spsize; then
    SIZE=$(grep "sub.*rsp" spsize | grep -oP '\$0x\K[0-9a-f]+')
    echo "栈帧大小: $((16#$SIZE)) 字节"
fi
```

### 4. 寄存器依赖分析
分析指令的寄存器写操作：
```bash
pin -t obj-intel64/findnextinst.so -pc 4198764 -- ./program
echo "=== 写寄存器列表 ==="
grep "regw" nextpc
```

## 输出示例

### 示例1: 普通算术指令
```
thispc:add $0x8, %rax
nextpc:4198780
regw0:rax
regw1:rflags
nostack
```

### 示例2: 内存读取指令
```
thispc:mov (%rbp), %rax
nextpc:4198900
regw0:rax
memory-load
nostack
```

### 示例3: 栈操作指令
```
thispc:push %rbx
nextpc:4199012
regw0:rsp
memory-write
stackw:rsp
base:rsp
index:null
displacement:-8
scale:1
```

### 示例4: 复杂寻址
```
thispc:mov 0x10(%rbp,%rcx,4), %rax
nextpc:4199120
regw0:rax
memory-load
nostack
```

## 代码关键点

### PC 字符串比较
```cpp
stringstream ss;
ss << INS_Address(ins);
if (pc.Value() == ss.str()) {
    // 匹配成功
}
```
使用字符串比较而非数值比较，避免类型转换问题。

### 栈操作判断
```cpp
if (INS_IsStackRead(ins)) {
    if (INS_MemoryOperandIsRead(ins, 0))
        OutFile << "stackr:" << REG_StringShort(INS_MemoryBaseReg(ins)) << endl;
    // ...
}
```
Pin 提供专门的 API 判断栈操作。

### 函数边界识别
```cpp
RTN rtn = INS_Rtn(ins);
if (RTN_Valid(rtn)) {
    RTN_Open(rtn);
    for (INS ins1 = RTN_InsHead(rtn); INS_Valid(ins1); ins1 = INS_Next(ins1)) {
        // 遍历函数内所有指令
    }
    RTN_Close(rtn);
}
```
利用 Pin 的 Routine API 分析函数级信息。

## 内存寻址模式

Pin 支持的内存寻址格式：
```
EA = Base + Index * Scale + Displacement
```

示例：
```
mov 0x10(%rbp,%rcx,4), %rax
     ^     ^    ^   ^
     |     |    |   |
  Disp  Base Index Scale
```

输出：
```
base:rbp
index:rcx
displacement:16
scale:4
```

## 性能考虑

### 开销分析
- **指令级插桩**: 对每条指令都检查 PC，开销较高
- **单次查询**: 找到目标后不会提前退出，仍需运行完整程序
- **适用场景**: 短时间运行的程序

### 优化建议
1. **提前退出**: 找到目标后调用 `exit()` 或 `PIN_Detach()`
2. **地址范围过滤**: 只在目标地址附近插桩
3. **缓存结果**: 相同程序只需查询一次

## 典型工作流

### 完整查询流程
```bash
#!/bin/bash
TARGET_PC="4198764"
PROGRAM="./matmul"

# 清理旧文件
rm -f nextpc spsize

# 运行查询
pin -t obj-intel64/findnextinst.so -pc $TARGET_PC -- $PROGRAM

# 检查是否找到
if [ ! -f nextpc ]; then
    echo "错误: 未找到 PC=$TARGET_PC"
    exit 1
fi

# 显示结果
echo "=== 指令信息 ==="
echo "当前指令: $(grep thispc nextpc | cut -d':' -f2-)"
echo "下一条地址: $(grep nextpc nextpc | cut -d':' -f2)"

echo ""
echo "=== 写寄存器 ==="
grep "^regw" nextpc

echo ""
echo "=== 内存操作 ==="
if grep -q "memory-load" nextpc; then
    echo "- 内存读"
fi
if grep -q "memory-write" nextpc; then
    echo "- 内存写"
fi

echo ""
echo "=== 栈操作 ==="
if grep -q "stack" nextpc; then
    grep "stack\|base\|index\|displacement\|scale" nextpc
else
    echo "- 非栈操作"
fi

echo ""
echo "=== 栈帧信息 ==="
if [ -f spsize ] && [ -s spsize ]; then
    cat spsize
else
    echo "- 无栈帧信息"
fi
```

### 批量查询
```bash
# 从 randomInst 生成的多个文件中提取 PC 并查询
for i in {1..10}; do
    if [ -f instruction$i ]; then
        PC=$(grep "pc:" instruction$i | cut -d':' -f2)
        echo "=== 查询 instruction$i (PC=$PC) ==="
        pin -t obj-intel64/findnextinst.so -pc $PC -- ./program
        mv nextpc nextpc_$i
        mv spsize spsize_$i 2>/dev/null
    fi
done
```

## 局限性

1. **单次查询**: 每次只能查询一个 PC
2. **完整运行**: 必须运行完整个程序（即使已找到）
3. **PC 格式**: 只支持十进制字符串
4. **符号信息**: 不显示函数名和源代码行号

## 扩展建议

1. **批量模式**: 支持一次查询多个 PC
2. **提前退出**: 找到后立即退出
3. **符号信息**: 集成调试信息，显示函数名
4. **JSON 输出**: 输出结构化数据便于解析
5. **范围查询**: 支持查询 PC 范围内的所有指令

## 与其他工具配合

### randomInst + findnextinst
```bash
# 随机选择 → 查询详情
pin -t obj-intel64/randomInst.so -randinst 100000 -- ./program
PC=$(grep "pc:" instruction | cut -d':' -f2)
pin -t obj-intel64/findnextinst.so -pc $PC -- ./program
```

### faultinjection + findnextinst
```bash
# 注入后查询注入点详情
grep "inject place" activation.log  # 假设输出: 123456
# 重新运行找到对应 PC
# （需要修改工具记录 PC 信息）
```

### saveInstcategory + findnextinst
```bash
# 从保存的指令中随机选择一条查询
PC=$(awk -F',' 'NR==100 {print "0x"$3}' targets.csv | xargs printf "%d")
pin -t obj-intel64/findnextinst.so -pc $PC -- ./program
```

## 调试技巧

### 检查 PC 是否存在
```bash
# 反汇编程序
objdump -d ./program > disasm.txt

# 搜索 PC
TARGET_PC="4198764"
HEX_PC=$(printf "%x" $TARGET_PC)
grep "^[[:space:]]*$HEX_PC:" disasm.txt
```

### 验证输出
```bash
# 验证 nextpc 是否正确
CURRENT_PC=$(grep "pc:" instruction | cut -d':' -f2)
NEXT_PC=$(grep "nextpc:" nextpc | cut -d':' -f2)

# 在反汇编中查找
CURRENT_HEX=$(printf "%x" $CURRENT_PC)
NEXT_HEX=$(printf "%x" $NEXT_PC)

echo "当前指令:"
grep "^[[:space:]]*$CURRENT_HEX:" disasm.txt

echo "下一条指令:"
grep "^[[:space:]]*$NEXT_HEX:" disasm.txt
```

## 注意事项

1. **PC 格式**: 必须是十进制字符串（不是十六进制）
2. **动态地址**: 对于 PIE 程序，PC 地址会随运行变化
3. **文件覆盖**: 每次运行会覆盖 `nextpc` 和 `spsize` 文件
4. **空输出**: 如果 PC 不存在，文件不会创建

## 相关文件

- 源代码: `/home/tongshiyu/pin/source/tools/pinfi/findnextinst.cpp`
- 依赖: `utils.h`, `pin.H`
- 输出: `nextpc`, `spsize`
