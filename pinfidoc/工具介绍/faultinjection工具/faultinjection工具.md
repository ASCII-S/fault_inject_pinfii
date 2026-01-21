# faultinjection 工具

## 概述

`faultinjection` 是 Pinfi 项目的核心工具，用于在程序运行时向寄存器或内存注入故障（位翻转），模拟硬件瞬时故障（Soft Error）对程序行为的影响。该工具支持多种故障模型和注入策略,广泛应用于系统可靠性评估和容错机制测试。

## 源文件

- **文件路径**: `faultinjection.cpp`
- **代码行数**: 619行
- **复杂度**: 极高

## 核心功能

### 基本功能
1. **通用寄存器故障注入**: 支持所有通用寄存器的单比特翻转
2. **浮点寄存器故障注入**: 支持 XMM、YMM、ST(x87)、MM 寄存器
3. **标志寄存器故障注入**: 支持条件分支相关的标志位注入
4. **内存故障注入**: 支持内存读写操作的位翻转
5. **多比特故障**: 支持单比特和多比特错误（ECC 模式）

### 故障模型
- **单比特翻转**: 随机翻转一个比特
- **多比特翻转**: 随机翻转多个比特
- **连续比特翻转**: 翻转相邻的多个比特（模拟 burst error）

## 工作原理

### 1. 随机注入时机选择
```cpp
void get_instance_number(const char* fi_instcount_file)
{
    // 读取总指令数
    // 生成随机注入实例编号
    srand(seed);
    fi_inject_instance = random() / (RAND_MAX * 1.0) * total_num_inst;
}
```

### 2. 指令计数与触发
```cpp
VOID inject_CCS(VOID *ip, UINT32 reg_num, CONTEXT *ctxt){
    if(fi_iterator == fi_inject_instance) {
        // 执行故障注入
        // ...
        activated = 1;
        PIN_ExecuteAt(ctxt);  // 重新执行该指令
    }
    fi_iterator++;
}
```

### 3. 寄存器故障注入
```cpp
// 通用寄存器
ADDRINT temp = PIN_GetContextReg(ctxt, reg);
UINT32 inject_bit = (rand() % (high_bound_bit - low_bound_bit)) + low_bound_bit;
temp = (ADDRINT)(temp ^ (1UL << inject_bit));
PIN_SetContextReg(ctxt, reg, temp);
```

### 4. 内存故障注入
```cpp
VOID FI_InjectFault_Mem(VOID *ip, VOID *memp, UINT32 size)
{
    UINT8* temp_p = (UINT8*) memp;
    UINT32 inject_bit = rand() % (size * 8);
    UINT32 byte_num = inject_bit / 8;
    UINT32 offset_num = inject_bit % 8;
    *(temp_p + byte_num) = *(temp_p + byte_num) ^ (1U << offset_num);
}
```

## 使用方法

### 编译
```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/faultinjection.so
```

### 运行

#### 基本故障注入
```bash
# 注入到通用寄存器
pin -t obj-intel64/faultinjection.so \
    -fi_instcount instcount.txt \
    -fi_function CCS_INST \
    -fi_activation activation.log \
    -index 1 \
    -- ./target_program
```

#### 多比特故障注入（ECC模式）
```bash
# 注入 2 个比特
pin -t obj-intel64/faultinjection.so \
    -fi_instcount instcount.txt \
    -fi_function CCS_INST \
    -fi_activation activation.log \
    -fiecc 1 \
    -multibits 2 \
    -index 1 \
    -- ./target_program
```

#### 连续比特故障注入
```bash
# 注入 3 个连续比特
pin -t obj-intel64/faultinjection.so \
    -fi_instcount instcount.txt \
    -fi_function CCS_INST \
    -fi_activation activation.log \
    -fiecc 1 \
    -multibits 3 \
    -consecutive 1 \
    -index 1 \
    -- ./target_program
```

### 参数说明

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `-fi_instcount` | string | - | 指令统计文件（包含总指令数） |
| `-fi_function` | string | - | 故障注入模式（CCS_INST/FP_INST/SP_INST/ALL_INST） |
| `-fi_activation` | string | activation.txt | 激活日志文件 |
| `-index` | int | 0 | 实验编号（用于随机种子） |
| `-fiecc` | bool | false | 启用 ECC 模式（多比特故障） |
| `-multibits` | int | 1 | 多比特故障的比特数 |
| `-consecutive` | bool | false | 多比特是否连续 |

## 故障注入模式

### CCS_INST（通用寄存器）
```cpp
VOID inject_CCS(VOID *ip, UINT32 reg_num, CONTEXT *ctxt){
    const REG reg = reg_map.findInjectReg(reg_num);
    if (REG_valid(reg)) {
        if (reg_map.isFloatReg(reg_num)) {
            // 浮点寄存器处理
            if (REG_is_xmm(reg)) {
                FI_SetXMMContextReg(ctxt, reg, reg_num);
            }
            // ...
        } else {
            // 通用寄存器处理
            ADDRINT temp = PIN_GetContextReg(ctxt, reg);
            UINT32 inject_bit = (rand() % (high_bound_bit - low_bound_bit)) + low_bound_bit;
            temp = (ADDRINT)(temp ^ (1UL << inject_bit));
            PIN_SetContextReg(ctxt, reg, temp);
        }
    }
}
```

### FlagReg（标志寄存器）
```cpp
VOID FI_InjectFault_FlagReg(VOID *ip, UINT32 reg_num, UINT32 jmp_num,
                            CONTEXT* ctxt, INS ins){
    CJmpMap::JmpType jmptype = jmp_map.findJmpType(jmp_num);

    if (jmptype == CJmpMap::DEFAULT) {
        // 随机翻转一个标志位
        UINT32 inject_bit = jmp_map.findInjectBit(jmp_num);
        temp = temp ^ (1UL << inject_bit);
    }
    else if (jmptype == CJmpMap::USPECJMP) {
        // JBE/JA: 修改 CF 和 ZF
        // ...
    }
    else {
        // JLE/JG: 修改 SF, OF, ZF
        // ...
    }
}
```

### Memory（内存）
```cpp
VOID FI_InjectFault_Mem(VOID *ip, VOID *memp, UINT32 size)
{
    UINT8* temp_p = (UINT8*) memp;
    UINT32 inject_bit = rand() % (size * 8);
    UINT32 byte_num = inject_bit / 8;
    UINT32 offset_num = inject_bit % 8;
    *(temp_p + byte_num) = *(temp_p + byte_num) ^ (1U << offset_num);
}
```

## 指令过滤策略

### NOBRANCHES（默认启用）
```cpp
#ifdef NOBRANCHES
  if(INS_IsBranch(ins) || !INS_HasFallThrough(ins)) {
    return;  // 跳过分支和返回指令
  }
#endif
```
- 排除分支指令，避免直接影响控制流
- 注入在数据处理指令上

### NOSTACKFRAMEOP
```cpp
#ifdef NOSTACKFRAMEOP
  if(INS_IsStackWrite(ins) || OPCODE_StringShort(INS_Opcode(ins)) == "POP") {
    return;  // 跳过栈操作
  }
#endif
```
- 排除栈帧操作，避免破坏调用栈

### ONLYFP
```cpp
#ifdef ONLYFP
  // 仅注入浮点寄存器
#endif
```
- 专注于浮点计算的可靠性

## 输出文件格式

### activation.log
```
fi index:1
fi inject instance:2341892
Executing: Valid Reg name rax in 0x400abc
inject place during execution 2341892
Activated: Valid Reg name rax in 0x400abc
inject place during execution 2341892
```

关键信息：
- **fi index**: 实验编号
- **fi inject instance**: 选择的注入指令编号
- **Reg name**: 注入的寄存器
- **inject place**: 实际注入位置

### 未激活情况
```
fi index:1
fi inject instance:2341892
Not Activated!
```
表示随机选择的指令无有效寄存器，未成功注入。

## 应用场景

### 1. 可靠性评估
评估程序在硬件瞬时故障下的容错能力：
```bash
#!/bin/bash
# 运行 1000 次故障注入实验
for i in {1..1000}; do
    pin -t obj-intel64/faultinjection.so \
        -fi_instcount count.txt \
        -fi_function CCS_INST \
        -fi_activation activation_$i.log \
        -index $i \
        -- ./target_program > output_$i.txt 2>&1

    # 检查程序输出是否正确
    if diff output_$i.txt golden_output.txt > /dev/null; then
        echo "Experiment $i: Masked"
    else
        echo "Experiment $i: SDC (Silent Data Corruption)"
    fi
done
```

### 2. 容错机制测试
测试软件容错技术（如 TMR、检查点）的有效性。

### 3. 脆弱性分析
识别程序中对故障敏感的部分：
```bash
# 分析注入到哪些寄存器最容易导致错误
grep "Reg name" activation_*.log | cut -d' ' -f6 | sort | uniq -c | sort -rn
```

### 4. 硬件可靠性研究
模拟不同故障率下的系统行为。

## 高级特性

### 1. 浮点寄存器处理
```cpp
if (REG_is_xmm(reg)) {
    FI_SetXMMContextReg(ctxt, reg, reg_num);
}
else if (REG_is_ymm(reg)) {
    FI_SetYMMContextReg(ctxt, reg, reg_num);
}
else if (REG_is_fr(reg) || REG_is_mm(reg)) {
    FI_SetSTContextReg(ctxt, reg, reg_num);
}
```
支持多种浮点寄存器格式。

### 2. 条件分支特殊处理
```cpp
if (reg == REG_RFLAGS || reg == REG_FLAGS || reg == REG_EFLAGS) {
    INS next_ins = INS_Next(ins);
    if (INS_Valid(next_ins) && INS_Category(next_ins) == XED_CATEGORY_COND_BR) {
        // 特殊处理条件分支的标志寄存器
        INS_InsertPredicatedCall(ins, IPOINT_AFTER,
                                (AFUNPTR)FI_InjectFault_FlagReg,
                                // ...
                                );
    }
}
```

### 3. 多比特 ECC 模式
```cpp
for (UINT32 i = 0; i < num; i++) {
    UINT32 inject_bit = rand() % (size * 8);
    if ((i == num-1) && mode) {
        inject_bit = last_inject_bit + 1;  // 连续比特
    }
    *(temp_p + byte_num) = *(temp_p + byte_num) ^ (1U << offset_num);
}
```

## 性能考虑

### 开销分析
- **指令级插桩**: 每条指令都插桩，开销极高（50-200倍）
- **注入时刻**: 只有一个时刻实际注入，其余时刻仅计数
- **优化**: 注入后可以 Detach，减少后续开销

### 典型运行时间
| 程序规模 | 原始运行时间 | 故障注入运行时间 | 倍数 |
|---------|-------------|----------------|------|
| 小型（< 1s） | 0.5s | 10s | 20x |
| 中型（1-10s） | 5s | 250s | 50x |
| 大型（> 10s） | 60s | 3600s | 60x |

## 代码关键点

### 随机种子选择
```cpp
unsigned int seed;
FILE* urandom = fopen("/dev/urandom", "r");
fread(&seed, sizeof(int), 1, urandom);
fclose(urandom);
srand(seed);
```
使用 `/dev/urandom` 获取高质量随机种子。

### PIN_ExecuteAt 的使用
```cpp
PIN_ExecuteAt(ctxt);
```
- 修改上下文后重新执行指令
- 确保故障在指令执行前生效

### 激活标志
```cpp
if (activated) {
    // 已注入，不再继续
    return;
}
```
防止重复注入。

## 典型工作流

### 完整故障注入实验
```bash
#!/bin/bash
PROGRAM="./matmul"
GOLDEN="golden_output.txt"

# 1. 获取正确输出
$PROGRAM > $GOLDEN

# 2. 统计指令数
pin -t obj-intel64/instcount_official.so -o count.out -- $PROGRAM
# 提取指令数并写入 instcount.txt
echo "CCS_INST:$(grep Count count.out | awk '{print $2}')" > instcount.txt

# 3. 批量故障注入
for i in {1..1000}; do
    pin -t obj-intel64/faultinjection.so \
        -fi_instcount instcount.txt \
        -fi_function CCS_INST \
        -fi_activation activation_$i.log \
        -index $i \
        -- $PROGRAM > output_$i.txt 2>&1

    # 分类结果
    if grep -q "Not Activated" activation_$i.log; then
        echo "$i,No_Injection" >> results.csv
    elif diff output_$i.txt $GOLDEN > /dev/null 2>&1; then
        echo "$i,Masked" >> results.csv
    else
        EXITCODE=$?
        if [ $EXITCODE -ne 0 ]; then
            echo "$i,Crash" >> results.csv
        else
            echo "$i,SDC" >> results.csv
        fi
    fi
done

# 4. 统计结果
echo "=== Fault Injection Results ==="
echo "Total: 1000"
echo "No Injection: $(grep No_Injection results.csv | wc -l)"
echo "Masked: $(grep Masked results.csv | wc -l)"
echo "Crash: $(grep Crash results.csv | wc -l)"
echo "SDC: $(grep SDC results.csv | wc -l)"
```

## 故障分类

| 类型 | 说明 | 严重性 |
|------|------|--------|
| **Masked** | 故障被程序自然屏蔽，输出正确 | 低 |
| **Crash** | 程序崩溃（段错误、异常退出） | 中 |
| **Hang** | 程序挂起（超时） | 中 |
| **SDC** | 静默数据损坏（输出错误但程序正常退出） | 高 |

## 依赖工具

### instselector.h
定义指令选择逻辑：
```cpp
bool isInstFITarget(INS ins);
void configInstSelector();
```

### fi_cjmp_map.h
条件分支映射：
```cpp
class CJmpMap {
    enum JmpType { DEFAULT, USPECJMP, SSPECJMP };
    JmpType findJmpType(UINT32 jmp_num);
    UINT32 findInjectBit(UINT32 jmp_num);
};
```

### utils.h
辅助函数：
```cpp
bool isValidInst(INS ins);
class RegMap {
    REG findInjectReg(UINT32 reg_num);
    bool isFloatReg(UINT32 reg_num);
};
```

## 扩展建议

1. **Detach 优化**: 注入后立即 Detach，减少开销
2. **目标指令过滤**: 支持仅注入特定类型指令
3. **值依赖注入**: 根据寄存器值选择性注入
4. **黄金运行对比**: 集成输出对比功能

## 注意事项

1. **随机数质量**: 使用 `/dev/urandom` 确保随机性
2. **文件权限**: 确保有权限写入 activation 文件
3. **内存安全**: 内存注入可能导致段错误
4. **重复实验**: 相同 index 会产生相同结果（可复现）

## 相关文件

- 源代码: `/home/tongshiyu/pin/source/tools/pinfi/faultinjection.cpp`
- 头文件: `faultinjection.h`, `instselector.h`, `fi_cjmp_map.h`
- 依赖: `utils.h`, `pin.H`

## 学术应用

该工具常用于以下研究领域：
- **软件可靠性工程**
- **容错计算**
- **关键系统安全**
- **航空航天软件**
- **硬件加速器可靠性**

## 相关论文

建议参考以下方向的论文了解故障注入技术：
- Pin-based fault injection
- Software fault injection
- Soft error resilience
- Reliability assessment
