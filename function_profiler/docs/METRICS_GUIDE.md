# 函数级剖析指标原理与弹性关联说明

## 概述

本文档详细说明Function Profiler工具收集的各项函数级指标的**计算原理**、**技术实现**以及**与软件弹性(Resilience)的关联**。

**命名规范**：
- `_static` 后缀：静态分析得到的指令数量
- `_exec` 后缀：运行时动态执行次数

---

## 一、执行统计类指标 (Execution Statistics)

### 1.1 call_exec (函数调用执行次数)

**原理说明**:
- 统计函数在程序执行过程中被调用的总次数
- 通过在函数入口点插入计数回调实现

**技术实现**:
```cpp
RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)FunctionEntry, ...);
```

---

### 1.2 inst_exec (总指令执行次数)

**原理说明**:
- 统计函数内所有指令的动态执行总次数
- 包括循环内重复执行的指令

**技术实现**:
```cpp
INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountInstruction, ...);
```

---

### 1.3 inst_static (静态指令数量)

**原理说明**:
- 函数内不重复的指令数量（代码规模）
- 在插桩时静态分析获得

---

## 二、数据流特性指标 (Data Flow Characteristics)

### 2.0 mem_read_static / mem_write_static / mem_inst_static (内存访问静态数量)

**原理说明**:
- `mem_read_static`: 函数内包含内存读操作的指令数量（静态）
- `mem_write_static`: 函数内包含内存写操作的指令数量（静态）
- `mem_inst_static`: 函数内涉及内存访问的指令数量（静态，不重复计数）

**技术实现**:
```cpp
// 在静态分析阶段计数
bool is_mem_read = INS_IsMemoryRead(ins);
bool is_mem_write = INS_IsMemoryWrite(ins);
if (is_mem_read) profile.mem_read_static++;
if (is_mem_write) profile.mem_write_static++;
if (is_mem_read || is_mem_write) profile.mem_inst_static++;
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高mem_inst_static比例 | 代码访存密集，内存错误敏感 |
| 静态vs动态对比 | 可识别热点访存指令（高执行频率） |

**研究假设**:
- 静态指标反映代码结构特征，动态指标反映运行时行为
- 结合两者可以计算"每静态指令平均执行次数"，识别热点

---

### 2.1 mem_read_exec / mem_write_exec (内存读写执行次数)

**原理说明**:
- 统计函数执行过程中的内存访问操作次数
- 通过检测 `INS_IsMemoryRead()` 和 `INS_IsMemoryWrite()` 实现
- **注意**：一条指令可能同时进行读和写（如RMW指令），此时会分别计入两个计数器

**技术实现**:
```cpp
if (INS_IsMemoryRead(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountMemoryRead, ...);
}
if (INS_IsMemoryWrite(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountMemoryWrite, ...);
}
```

**同时读写的指令示例**:
| 指令类型 | 示例 | 行为 |
|---------|------|------|
| RMW指令 | `ADD [mem], reg` | 先读内存，计算后写回 |
| 自增/自减 | `INC [mem]`, `DEC [mem]` | 先读，修改，再写 |
| 交换指令 | `XCHG [mem], reg` | 读写同时发生 |
| 原子操作 | `LOCK ADD [mem], 1` | 原子读-修改-写 |

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高内存读取 | 数据依赖强，错误易传播 |
| 高内存写入 | 状态修改多，错误易持久化 |
| 读写比例失衡 | 可能存在数据流瓶颈 |

**研究假设**:
- 内存写操作是错误持久化的关键点
- 高内存读取意味着更多的数据依赖，错误传播概率增加

---

### 2.2 mem_inst_exec (访存指令执行次数)

**原理说明**:
- 统计涉及内存访问的指令执行次数（不重复计数）
- 一条指令无论同时读写还是只读/只写，都只计数一次
- 用于准确统计"有多少条指令涉及内存访问"

**技术实现**:
```cpp
// 使用OR条件，确保同时读写的指令只计数一次
if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountMemoryInstExec, ...);
}
```

**与memory_reads/memory_writes的关系**:
```
memory_inst_exec_count <= memory_reads + memory_writes
```
- 当等号成立时：所有访存指令都是纯读或纯写
- 当小于时：存在同时读写的RMW指令

**实际数据示例**:
| 函数 | reads | writes | inst_exec | 说明 |
|------|-------|--------|-----------|------|
| multiply | 30 | 15 | 35 | 有10次RMW操作 |
| fibonacci | 973 | 707 | 1680 | 无RMW操作 |

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高inst_exec | 更多指令涉及内存，故障暴露点多 |
| inst_exec << reads+writes | RMW操作多，原子性故障风险 |

**研究假设**:
- RMW指令的故障影响更复杂（可能同时影响读取值和写入值）
- 该指标更准确反映"访存指令密度"

---

### 2.3 内存访问模式分析 (Memory Access Pattern)

**原理说明**:
通过计算相邻内存访问的**stride（步长）**来分类访问模式：
- **连续访问(Sequential)**: |stride| ≤ 64字节（一个缓存行范围内）
- **步长访问(Strided)**: stride稳定（如数组遍历，每次+4/+8字节）
- **随机访问(Random)**: stride变化大（指针/链表/递归栈访问）

**技术实现**:
```cpp
#define SEQUENTIAL_THRESHOLD 64      // 缓存行大小
#define STRIDE_VARIANCE_THRESHOLD 8  // stride变化阈值

VOID AnalyzeMemoryReadPattern(FunctionProfile* profile, ADDRINT addr) {
    if (profile->has_last_read) {
        INT64 stride = addr - profile->last_read_addr;
        INT64 abs_stride = stride < 0 ? -stride : stride;

        if (abs_stride <= SEQUENTIAL_THRESHOLD) {
            profile->sequential_reads++;      // 连续访问
        } else {
            INT64 stride_diff = abs(stride - profile->last_read_stride);
            if (stride_diff <= STRIDE_VARIANCE_THRESHOLD) {
                profile->strided_reads++;     // 步长访问
            } else {
                profile->random_reads++;      // 随机访问
            }
        }
        profile->last_read_stride = stride;
    }
    profile->last_read_addr = addr;
    profile->has_last_read = true;
}
```

**指标说明**:

| 指标 | 类型 | 说明 |
|------|------|------|
| `seq_read_exec` | 动态 | 连续读执行次数 |
| `stride_read_exec` | 动态 | 步长读执行次数 |
| `random_read_exec` | 动态 | 随机读执行次数 |
| `seq_write_exec` | 动态 | 连续写执行次数 |
| `stride_write_exec` | 动态 | 步长写执行次数 |
| `random_write_exec` | 动态 | 随机写执行次数 |

**弹性关联**:

| 访问模式 | 特征 | 弹性影响 |
|---------|------|---------|
| 连续访问 | 数组顺序遍历 | 故障影响局部，易预测，缓存友好 |
| 步长访问 | 结构体数组遍历 | 故障可能跨数据边界 |
| 随机访问 | 指针/链表/递归 | 故障传播不可预测，SDC风险高 |

**实际数据示例**:
| 函数 | seq_reads | random_reads | 解读 |
|------|-----------|--------------|------|
| multiply | 29 | 0 | 100%连续，数组遍历 |
| fibonacci | 972 | 0 | 读连续，但写有随机（递归栈跳跃） |

**研究假设**:
- 高`random_read_ratio`的函数，故障传播路径不可预测，SDC风险高
- 高`sequential_read_ratio`的函数，故障影响范围局部，更容易检测
- 递归函数的写访问通常有较高的随机比例（栈帧跳跃）

---

## 三、计算特性指标 (Compute Characteristics)

### 3.1 arith_static (算术指令静态数量)

**原理说明**:
- 统计算术运算指令：ADD, SUB, MUL, DIV, INC, DEC, NEG, ADC, SBB, IMUL, IDIV
- 反映函数的计算密集程度

---

### 3.2 logic_static (逻辑指令静态数量)

**原理说明**:
- 统计逻辑运算指令：AND, OR, XOR, NOT, SHL, SHR, SAL, SAR, ROL, ROR
- 包括位操作和移位操作

---

### 3.3 float_static (浮点指令静态数量)

**原理说明**:
- 统计浮点运算指令：x87指令(FADD, FMUL等)和SSE/AVX浮点指令

---

### 3.4 simd_static (SIMD/向量指令静态数量)

**原理说明**:
- 统计使用XMM/YMM/ZMM/MMX寄存器的向量指令
- 通过检测寄存器类型实现

---

### 3.5 arith_exec / logic_exec / float_exec / simd_exec (指令执行次数)

**原理说明**:
- 动态统计各类计算指令的执行次数
- `arith_exec`: 算术指令执行次数
- `logic_exec`: 逻辑指令执行次数（新增）
- `float_exec`: 浮点指令执行次数
- `simd_exec`: SIMD指令执行次数（新增）

**技术实现**:
```cpp
if (IsLogicInstruction(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountLogicExec, ...);
}
if (IsSIMDInstruction(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountSIMDExec, ...);
}
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高SIMD比例 | 单次故障影响多个数据元素 |
| 向量化计算 | 错误可能被并行传播 |
| 高逻辑运算 | 位翻转可能改变控制流逻辑 |

**研究假设**:
- SIMD指令的故障影响范围更大（一次影响多个数据通道）
- 逻辑指令的位错误可能导致条件判断失败
- 静态-动态比值可识别热点指令

---

### 3.6 pure_compute_static / pure_compute_exec (纯计算指令)

**原理说明**:
- 统计不涉及内存访问的纯计算指令
- 纯计算 = (算术 || 逻辑 || 浮点 || SIMD) && !内存访问
- 反映函数的纯计算强度

**技术实现**:
```cpp
bool IsPureComputeInstruction(INS ins) {
    if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        return false;
    }
    return IsArithmeticInstruction(ins) ||
           IsLogicInstruction(ins) ||
           IsFloatInstruction(ins) ||
           IsSIMDInstruction(ins);
}

// 静态分析
if (IsPureComputeInstruction(ins)) {
    profile.pure_compute_static++;
}

// 动态插桩
if (IsPureComputeInstruction(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountPureComputeExec, ...);
}
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高纯计算比例 | 计算密集型，ALU错误敏感 |
| 低纯计算比例 | 访存密集型，内存/缓存错误敏感 |
| 纯计算/访存比 | 反映弹性脆弱点类型 |

**研究假设**:
- 纯计算指令的错误不会立即传播到内存
- 高纯计算比例的函数对寄存器/ALU错误更敏感
- 可用于计算"计算强度" (pure_compute_exec / mem_inst_exec)

---

### 3.7 data_movement_static / data_movement_exec (数据移动指令)

**原理说明**:
- 统计数据移动指令：MOV, LEA, XCHG, PUSH, POP等
- 这类指令只搬运数据，不进行计算
- 反映函数的数据流复杂度

**技术实现**:
```cpp
bool IsDataMovementInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    if (mnemonic.find("MOV") != string::npos ||
        mnemonic.find("LEA") != string::npos ||
        mnemonic.find("XCHG") != string::npos ||
        mnemonic.find("PUSH") != string::npos ||
        mnemonic.find("POP") != string::npos) {
        return true;
    }
    return false;
}

// 静态分析
if (IsDataMovementInstruction(ins)) {
    profile.data_movement_static++;
}

// 动态插桩
if (IsDataMovementInstruction(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountDataMovementExec, ...);
}
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高数据移动比例 | 错误易传播，但不产生新错误 |
| MOV指令多 | 数据流复杂，错误传播路径多 |
| PUSH/POP多 | 栈操作频繁，栈错误风险高 |

**实际数据示例**:
```
函数A: data_movement_exec=500, pure_compute_exec=100
  → 数据搬运为主，计算少，错误传播风险高

函数B: data_movement_exec=50, pure_compute_exec=800
  → 计算密集型，数据搬运少，ALU错误风险高
```

**研究假设**:
- 高数据移动比例的函数充当"数据管道"角色
- MOV指令本身不产生新错误，但会传播现有错误
- data_movement_exec / inst_exec 可反映函数角色（计算型 vs 数据流型）

---

## 四、控制流特性指标 (Control Flow Characteristics)

### 4.1 branch_static / branch_exec (分支指令静态数量/执行次数)

**原理说明**:
- branch_static: 静态分支指令数量
- branch_exec: 分支指令动态执行次数

---

### 4.2 loop_static (循环静态数量)

**原理说明**:
- 通过检测回边(backward edge)识别循环
- 回边定义：分支目标地址 < 当前指令地址 且 在函数范围内

---

### 4.3 return_static (返回点静态数量)

**原理说明**:
- 统计函数内RET指令的数量
- 反映函数的出口复杂度

---

### 4.4 call_static (函数调用静态数量)

**原理说明**:
- 统计函数内CALL指令的数量
- 反映函数的依赖复杂度

---

### 4.5 call_other_exec (调用其他函数执行次数)

**原理说明**:
- 统计函数内CALL指令的动态执行次数
- 反映函数运行时对其他函数的实际依赖程度
- 与 `call_exec`（被调用次数）形成调用关系的两端

**技术实现**:
```cpp
VOID TrackCallDepthEnter(FunctionProfile* profile) {
    // 统计调用其他函数次数
    __sync_fetch_and_add(&(profile->call_other_exec), 1);
    // ...
}
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高调用次数 | 错误可能传播到被调用函数，影响范围扩大 |
| 低调用次数 | 函数相对独立，错误影响局部化 |
| call_other_exec >> call_static | 调用点被频繁执行（循环中调用） |

**与 call_exec 的区别**:
| 指标 | 含义 | 视角 |
|------|------|------|
| `call_exec` | 本函数被其他函数调用的次数 | 被调用者 |
| `call_other_exec` | 本函数调用其他函数的次数 | 调用者 |

---

### 4.6 indirect_exec (间接跳转执行次数)

**原理说明**:
- 统计间接跳转/调用指令的动态执行次数（目标地址在寄存器中）

---

### 4.7 细化指令类型（用于熵计算）

除了基本的算术、逻辑、浮点、SIMD指令外，还统计以下细化类型：

| 指标 | 说明 | 包含指令 |
|------|------|----------|
| `compare_static/exec` | 比较指令 | CMP, TEST, COMIS, UCOMIS |
| `stack_static/exec` | 栈操作指令 | PUSH, POP, ENTER, LEAVE |
| `string_static/exec` | 字符串指令 | MOVS, STOS, LODS, SCAS, CMPS, REP |
| `nop_static/exec` | NOP指令 | NOP |
| `other_static/exec` | 其他未分类指令 | 不属于以上任何类别的指令 |

---

### 4.8 指令类型分布熵 (B3类)

**原理说明**:
- 熵（Entropy）衡量指令类型分布的均匀程度
- 公式：`H = -Σ(p_i * log2(p_i))`，其中 p_i 是第 i 类指令的占比

**技术实现**:
```cpp
double ComputeInstructionEntropy(const vector<UINT64>& counts) {
    UINT64 total = 0;
    for (UINT64 c : counts) total += c;
    if (total == 0) return 0.0;

    double entropy = 0.0;
    for (UINT64 c : counts) {
        if (c > 0) {
            double p = (double)c / (double)total;
            entropy -= p * log2(p);
        }
    }
    return entropy;
}
```

**指标说明**:

| 指标 | 说明 |
|------|------|
| `inst_type_entropy_static` | 基于静态指令分布计算的熵 |
| `inst_type_entropy_exec` | 基于动态执行分布计算的熵 |

**熵值解释**:
| 熵值范围 | 含义 |
|----------|------|
| 0 | 所有指令为同一类型（完全单一） |
| 1.0 ~ 2.0 | 指令类型较为集中 |
| 2.0 ~ 3.0 | 指令类型分布中等 |
| > 3.0 | 指令类型分布均匀（多样性高） |

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 低熵（类型单一） | 错误影响可预测，但缺乏多样性保护 |
| 高熵（类型多样） | 错误可能影响多种操作，但有自然冗余 |
| 静态熵 ≈ 动态熵 | 执行路径覆盖均匀 |
| 静态熵 >> 动态熵 | 热点代码集中在特定类型指令 |

**研究假设**:
- 高熵函数可能具有更好的自然容错能力（指令多样性）
- 静态熵与动态熵的差异反映代码执行的热点特征

---

## 五、指标间的关联分析

### 5.1 计算密集度 vs 访存密集度

```
                    高访存密集度
                         |
        访存密集型       |       混合型
        (数据库操作)     |    (通用计算)
                         |
    ─────────────────────┼─────────────────────
                         |
        控制流密集型     |      计算密集型
        (解析器)         |    (科学计算)
                         |
                    低访存密集度

    低计算密集度 ←───────────────→ 高计算密集度
```

### 5.2 弹性预测模型（假设）

基于指标组合预测函数的弹性特征：

| 组合特征 | 预期弹性行为 |
|---------|-------------|
| 高compute_ratio + 低memory_access_ratio | SDC风险高，崩溃概率低 |
| 高memory_access_ratio + 高random_read_ratio | 故障传播不可预测 |
| 高sequential_read_ratio + 低branch_density | 访问局部性好，易预测 |
| 高loop_count + 高arithmetic_exec_count | 错误放大风险高 |

---

## 六、学术研究应用指南

### 6.1 数据收集流程

```
1. 运行function_profiler获取函数特征
   ↓
2. 对每个函数进行故障注入实验
   ↓
3. 记录故障结果：SDC / Crash / Masked / Detected
   ↓
4. 建立特征-结果数据集
   ↓
5. 统计分析/机器学习建模
```

### 6.2 推荐的分析方法

1. **相关性分析**: 计算各指标与修复成功率的Pearson/Spearman相关系数
2. **回归分析**: 建立多元回归模型预测弹性
3. **聚类分析**: 基于指标对函数进行聚类，识别弹性模式
4. **决策树/随机森林**: 识别影响弹性的关键特征

### 6.3 论文写作建议

**研究问题示例**:
- RQ1: 哪些函数特征与故障屏蔽成功率显著相关？
- RQ2: 计算密集型函数和访存密集型函数的弹性差异如何？
- RQ3: 能否基于静态特征预测函数的弹性？

**预期发现示例**:
- "random_read_ratio与SDC率呈正相关(r=0.68, p<0.01)"
- "memory_access_ratio高的函数崩溃率更高，但SDC率更低"
- "循环密集型函数的故障影响范围更大"

---

## 五点五、函数间调用图指标 (I类)

I类指标用于分析函数在调用图中的位置和耦合度，反映函数间的依赖关系。

### 5.5.1 fan_in (入度)

**原理说明**:
- 统计有多少个**不同的函数**调用了本函数
- 反映函数被依赖的程度（被多少函数使用）

**技术实现**:
```cpp
// 在CALL指令处记录调用边
VOID RecordCallEdge(FunctionProfile* caller_profile, ADDRINT callee_addr) {
    // 记录callee被caller调用
    g_function_profiles[callee_addr].callers_set.insert(caller_profile->start_addr);
}
// 最终: profile.fan_in = profile.callers_set.size();
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高fan_in | 被多个函数依赖，故障影响范围广 |
| 低fan_in | 被少数函数依赖，故障影响局部 |
| fan_in=0 | 入口函数或未被调用的函数 |

---

### 5.5.2 fan_out (出度)

**原理说明**:
- 统计本函数调用了多少个**不同的函数**
- 反映函数对其他函数的依赖程度

**技术实现**:
```cpp
// 在CALL指令处记录调用边
VOID RecordCallEdge(FunctionProfile* caller_profile, ADDRINT callee_addr) {
    // 记录caller调用了callee
    caller_profile->callees_set.insert(callee_addr);
}
// 最终: profile.fan_out = profile.callees_set.size();
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高fan_out | 依赖多个函数，易受其他函数故障影响 |
| 低fan_out | 依赖少，相对独立 |
| fan_out=0 | 叶子函数，不调用其他函数 |

---

### 5.5.3 fan_in/fan_out 与 call_exec/call_other_exec 的区别

| 指标 | 含义 | 统计维度 | 示例 |
|------|------|---------|------|
| `call_exec` | 本函数被调用的**总次数** | 频率 | 被调用100次 |
| `fan_in` | 有**多少个不同函数**调用了本函数 | 唯一调用者数 | 被3个不同函数调用 |
| `call_other_exec` | 本函数调用其他函数的**总次数** | 频率 | 调用了其他函数50次 |
| `fan_out` | 本函数调用了**多少个不同函数** | 唯一被调用者数 | 调用了5个不同函数 |

**具体示例**:
```
假设调用关系：
main() ──调用10次──> foo()
bar()  ──调用20次──> foo()
baz()  ──调用70次──> foo()

foo() ──调用30次──> helper1()
foo() ──调用20次──> helper2()

对于 foo() 函数：
- call_exec = 100 (被调用总次数: 10+20+70)
- fan_in = 3 (有3个不同函数调用它: main, bar, baz)
- call_other_exec = 50 (调用其他函数总次数: 30+20)
- fan_out = 2 (调用了2个不同函数: helper1, helper2)
```

**研究假设**:
- 高fan_in的函数是"核心函数"，故障影响范围大
- 高fan_out的函数是"协调函数"，易受级联故障影响
- fan_in × call_exec 可衡量函数的"影响力"
- fan_out × call_other_exec 可衡量函数的"脆弱性"

---

## 六、寄存器使用指标 (D类)

### 6.1 reg_read_exec / reg_write_exec (寄存器读写执行次数)

**原理说明**:
- 统计函数执行过程中寄存器的读取和写入操作次数
- 通过 `INS_MaxNumRRegs()` 和 `INS_MaxNumWRegs()` 获取每条指令的寄存器操作数

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高寄存器读取 | 错误传播机会多 |
| 高寄存器写入 | 故障注入点多 |

---

### 6.2 reg_read_static / reg_write_static (静态寄存器操作数)

**原理说明**:
- 统计函数内所有指令的寄存器读/写操作数总和
- 反映函数的寄存器使用密度

---

### 6.3 unique_reg_read / unique_reg_write (唯一寄存器数)

**原理说明**:
- 统计函数使用的不同通用寄存器数量
- 反映寄存器压力和数据流复杂度

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高唯一寄存器数 | 数据流复杂，错误传播路径多 |
| 低唯一寄存器数 | 寄存器复用多，错误可能被覆盖 |

---

## 七、控制流细化指标 (E类)

### 7.1 branch_taken_exec / branch_not_taken_exec (分支方向统计)

**原理说明**:
- 统计条件分支的跳转和未跳转执行次数
- 使用 Pin 的 `IARG_BRANCH_TAKEN` 获取分支方向

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| taken/not_taken比例失衡 | 路径覆盖不均，某些路径测试不足 |
| 比例接近1:1 | 路径敏感性高，故障可能改变执行路径 |

---

### 7.2 cond_branch_static / uncond_branch_static (分支类型统计)

**原理说明**:
- 条件分支：有fall-through路径的分支（如JE, JNE）
- 无条件分支：无fall-through路径的跳转（如JMP）

---

### 7.3 loop_iter_total (循环迭代总次数)

**原理说明**:
- 统计所有循环回边的执行次数
- 反映循环的实际迭代量

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高迭代次数 | 错误放大风险高 |
| 低迭代次数 | 错误影响范围有限 |

---

### 7.4 call_depth_max (最大调用深度)

**原理说明**:
- 跟踪函数内部的调用/返回，记录最大嵌套深度
- 对于递归函数，反映递归深度

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高调用深度 | 栈溢出风险，错误传播链长 |
| 递归函数 | 错误可能被递归放大 |

---

### 7.5 loop_depth_max (最大循环嵌套深度)

**原理说明**:
- 跟踪函数执行过程中循环的嵌套深度
- 通过监控回边的执行状态，维护当前活跃循环集合
- 当回边执行（循环继续）时，将循环加入活跃集合
- 当回边不执行（循环退出）时，将循环从活跃集合移除

**技术实现**:
```cpp
VOID TrackLoopExecution(FunctionProfile* profile, ADDRINT backedge_addr, BOOL taken) {
    if (taken) {
        // 回边执行，循环继续
        if (profile->active_loops.find(backedge_addr) == profile->active_loops.end()) {
            profile->active_loops.insert(backedge_addr);
            profile->current_loop_depth = profile->active_loops.size();
            if (profile->current_loop_depth > profile->loop_depth_max) {
                profile->loop_depth_max = profile->current_loop_depth;
            }
        }
    } else {
        // 回边未执行，循环退出
        profile->active_loops.erase(backedge_addr);
        profile->current_loop_depth = profile->active_loops.size();
    }
}
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高循环嵌套深度 | 错误放大效应更强，内层循环错误影响范围大 |
| 深层嵌套循环 | 控制流复杂，故障传播路径多 |
| 嵌套深度=1 | 简单循环结构，错误影响相对可控 |

**研究假设**:
- 嵌套循环中的错误会被多层循环放大
- 高嵌套深度的函数对控制流错误更敏感
- 结合 `loop_iter_total` 可评估总体循环复杂度

---

## 八、数据依赖指标 (F类) [可选]

需要 `-enable_dep` 参数启用，有一定性能开销。

### 8.1 def_use_pairs (定义-使用对总数)

**原理说明**:
- 统计寄存器的定义（写入）到使用（读取）的配对数量
- 只跟踪16个通用寄存器（RAX-R15）

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高def_use_pairs | 错误传播路径多 |
| 低def_use_pairs | 数据流简单，错误影响局部 |

---

### 8.2 reg_dep_chain_max (最长依赖链)

**原理说明**:
- 记录从寄存器定义到使用的最大指令距离
- 反映错误传播的最大深度

---

### 8.3 mem_to_reg_exec / reg_to_mem_exec (内存-寄存器传递)

**原理说明**:
- mem_to_reg: 从内存加载到寄存器的次数
- reg_to_mem: 从寄存器存储到内存的次数

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高mem_to_reg | 内存错误易传播到计算 |
| 高reg_to_mem | 计算错误易持久化到内存 |

---

## 九、生命周期指标 (G类) [可选]

需要 `-enable_lifetime` 参数启用，有一定性能开销。

### 9.1 reg_lifetime_total (寄存器值总存活指令数)

**原理说明**:
- 累计所有寄存器值从定义到使用的指令距离
- 反映SDC风险窗口的总大小

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高lifetime | SDC风险窗口大，错误有更多机会被使用 |
| 低lifetime | 值快速被使用或覆盖，错误影响时间短 |

---

### 9.2 dead_write_exec (死写次数)

**原理说明**:
- 统计寄存器被写入后未被读取就再次被覆盖的次数
- 这些写入的错误会被自然屏蔽

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高dead_write | 自然屏蔽机会多，部分故障无影响 |
| 低dead_write | 大多数写入都会被使用，故障影响大 |

---

### 9.3 first_use_dist_total (首次使用距离总和)

**原理说明**:
- 累计从寄存器定义到首次使用的指令距离
- 反映早期检测错误的机会

---

## 十、圈复杂度指标 (H类)

圈复杂度指标反映程序控制流的复杂程度，包括静态圈复杂度（基于所有可能路径）和动态圈复杂度（基于实际执行路径）。

### 10.1 bbl_static (静态基本块数量)

**原理说明**:
- 函数中静态分析得到的基本块（BBL）数量
- 基本块是没有分支的连续指令序列
- 通过识别跳转目标和控制流指令边界来划分

**技术实现**:
```cpp
// 识别BBL头：函数入口、跳转目标、控制流指令的下一条指令
set<ADDRINT> bbl_heads;
bbl_heads.insert(rtn_addr);  // 函数入口

for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
    if (INS_IsDirectControlFlow(ins)) {
        ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
        if (target >= profile.start_addr && target < profile.end_addr) {
            bbl_heads.insert(target);
        }
    }
    if (INS_IsControlFlow(ins)) {
        INS next = INS_Next(ins);
        if (INS_Valid(next)) {
            bbl_heads.insert(INS_Address(next));
        }
    }
}
profile.bbl_static = bbl_heads.size();
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高BBL数量 | 控制流复杂，故障传播路径多 |
| 低BBL数量 | 简单顺序执行，错误影响可预测 |

---

### 10.2 edge_static (静态控制流边数量)

**原理说明**:
- 函数中静态分析得到的控制流边数量
- 边类型包括：
  - **Fall-through边**：条件分支不跳转时的顺序执行
  - **跳转边**：分支指令跳转到目标地址
  - **顺序边**：被跳转目标分割的BBL之间的连接

**技术实现**:
```cpp
set<std::pair<ADDRINT, ADDRINT>> static_edges;
ADDRINT current_bbl_start = rtn_addr;

for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
    INS next_ins = INS_Next(ins);
    if (INS_Valid(next_ins)) {
        ADDRINT next_pc = INS_Address(next_ins);
        if (bbl_heads.find(next_pc) != bbl_heads.end()) {
            // 当前指令是BBL的最后一条
            if (INS_IsControlFlow(ins)) {
                // 添加跳转边
                if (INS_IsDirectControlFlow(ins)) {
                    ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
                    if (target >= profile.start_addr && target < profile.end_addr) {
                        static_edges.insert(std::make_pair(current_bbl_start, target));
                    }
                }
                // 添加fall-through边
                if (INS_HasFallThrough(ins)) {
                    static_edges.insert(std::make_pair(current_bbl_start, next_pc));
                }
            } else {
                // 顺序边
                static_edges.insert(std::make_pair(current_bbl_start, next_pc));
            }
            current_bbl_start = next_pc;
        }
    }
}
profile.edge_static = static_edges.size();
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高边数 | 控制流转移多，分支决策复杂 |
| 边数接近BBL数 | 线性执行为主，控制流简单 |

---

### 10.3 static_cyclomatic (静态圈复杂度)

**原理说明**:
- 基于静态分析的所有可能控制流路径计算
- 公式：`Static_CC = E - N + 2`
  - E = edge_static（静态边数）
  - N = bbl_static（静态节点数）
- 最小值为1（纯顺序执行）

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高静态CC | 代码结构复杂，测试难度大 |
| 低静态CC | 代码结构简单，易于测试和验证 |

---

### 10.4 bbl_exec (基本块执行次数)

**原理说明**:
- 运行时所有基本块的总执行次数
- 反映函数的动态执行热度

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高执行次数 | 热点代码，故障暴露概率高 |
| 低执行次数 | 冷路径，故障可能潜伏 |

---

### 10.5 unique_bbl_exec (唯一基本块执行数)

**原理说明**:
- 实际执行过的不同基本块数量（去重）
- 反映代码覆盖率：`覆盖率 = unique_bbl_exec / bbl_static`

**技术实现**:
```cpp
VOID TrackBBLExecution(FunctionProfile* profile, ADDRINT bbl_addr) {
    __sync_fetch_and_add(&(profile->bbl_exec), 1);

    PIN_GetLock(&g_lock, 1);
    profile->executed_bbls.insert(bbl_addr);
    PIN_ReleaseLock(&g_lock);
}
// 最终: profile.unique_bbl_exec = profile.executed_bbls.size();
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| unique_bbl_exec ≈ bbl_static | 高覆盖率，大部分路径被执行 |
| unique_bbl_exec << bbl_static | 低覆盖率，存在未测试路径 |

---

### 10.6 unique_edge_exec (唯一边执行数)

**原理说明**:
- 实际执行过的不同控制流边数量
- 边表示从一个BBL到另一个BBL的控制流转移

**技术实现**:
```cpp
VOID TrackBBLExecution(FunctionProfile* profile, ADDRINT bbl_addr) {
    // ...
    if (profile->last_bbl_addr != 0) {
        profile->executed_edges.insert(std::make_pair(profile->last_bbl_addr, bbl_addr));
    }
    profile->last_bbl_addr = bbl_addr;
}
// 最终: profile.unique_edge_exec = profile.executed_edges.size();
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高边数 | 控制流转移频繁，分支决策多 |
| 边数接近BBL数 | 线性执行为主 |

---

### 10.7 dynamic_cyclomatic (动态圈复杂度)

**原理说明**:
- 基于实际执行的控制流图计算圈复杂度
- 公式：`Dynamic_CC = E - N + 2`
  - E = unique_edge_exec（实际执行的唯一边数）
  - N = unique_bbl_exec（实际执行的唯一节点数）
- 最小值为1（纯顺序执行）

**与静态圈复杂度的区别**:
| 指标 | 静态圈复杂度 | 动态圈复杂度 |
|------|-------------|-------------|
| 计算时机 | 编译时/静态分析 | 运行时 |
| 数据来源 | 所有可能路径 | 实际执行路径 |
| 应用场景 | 代码质量评估 | 运行时行为分析 |

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高动态CC | 实际执行路径复杂，故障传播路径多 |
| 动态CC < 静态CC | 部分分支未执行，存在未测试路径 |
| 动态CC = 1 | 纯顺序执行，无分支决策 |

**研究假设**:
- 动态圈复杂度高的函数对控制流错误更敏感
- 动态CC与静态CC的差异反映测试覆盖情况
- 结合 `branch_taken_exec` 和 `branch_not_taken_exec` 可分析分支行为

---

## 十一、指标汇总表

| 类别 | 指标名 | 类型 | 说明 |
|------|--------|------|------|
| A | call_exec | 动态 | 函数调用执行次数 |
| A | inst_exec | 动态 | 总指令执行次数 |
| A | inst_static | 静态 | 静态指令数量 |
| B1 | mem_read_exec | 动态 | 内存读执行次数 |
| B1 | mem_write_exec | 动态 | 内存写执行次数 |
| B1 | mem_inst_exec | 动态 | 访存指令执行次数 |
| B1.5 | seq_read_exec | 动态 | 连续读执行次数 |
| B1.5 | stride_read_exec | 动态 | 步长读执行次数 |
| B1.5 | random_read_exec | 动态 | 随机读执行次数 |
| B1.5 | seq_write_exec | 动态 | 连续写执行次数 |
| B1.5 | stride_write_exec | 动态 | 步长写执行次数 |
| B1.5 | random_write_exec | 动态 | 随机写执行次数 |
| B2 | arith_static | 静态 | 算术指令静态数量 |
| B2 | logic_static | 静态 | 逻辑指令静态数量 |
| B2 | float_static | 静态 | 浮点指令静态数量 |
| B2 | simd_static | 静态 | SIMD指令静态数量 |
| B2 | pure_compute_static | 静态 | 纯计算指令静态数量 |
| B2 | data_movement_static | 静态 | 数据移动指令静态数量 |
| B2 | compare_static | 静态 | 比较指令静态数量 |
| B2 | stack_static | 静态 | 栈操作指令静态数量 |
| B2 | string_static | 静态 | 字符串指令静态数量 |
| B2 | nop_static | 静态 | NOP指令静态数量 |
| B2 | other_static | 静态 | 其他指令静态数量 |
| B2 | arith_exec | 动态 | 算术指令执行次数 |
| B2 | logic_exec | 动态 | 逻辑指令执行次数 |
| B2 | float_exec | 动态 | 浮点指令执行次数 |
| B2 | simd_exec | 动态 | SIMD指令执行次数 |
| B2 | compare_exec | 动态 | 比较指令执行次数 |
| B2 | stack_exec | 动态 | 栈操作指令执行次数 |
| B2 | string_exec | 动态 | 字符串指令执行次数 |
| B2 | nop_exec | 动态 | NOP指令执行次数 |
| B2 | other_exec | 动态 | 其他指令执行次数 |
| B3 | inst_type_entropy_static | 静态 | 静态指令类型分布熵 |
| B3 | inst_type_entropy_exec | 动态 | 动态指令类型分布熵 |
| C | branch_static | 静态 | 分支指令静态数量 |
| C | branch_exec | 动态 | 分支指令执行次数 |
| C | loop_static | 静态 | 循环静态数量 |
| C | return_static | 静态 | 返回点静态数量 |
| C | call_static | 静态 | 函数调用静态数量 |
| C | call_other_exec | 动态 | 调用其他函数执行次数 |
| C | indirect_exec | 动态 | 间接跳转执行次数 |
| I | fan_in | 动态 | 入度：有多少不同函数调用本函数 |
| I | fan_out | 动态 | 出度：本函数调用多少不同函数 |
| D | reg_read_exec | 动态 | 寄存器读取执行次数 |
| D | reg_write_exec | 动态 | 寄存器写入执行次数 |
| D | reg_read_static | 静态 | 静态寄存器读操作数 |
| D | reg_write_static | 静态 | 静态寄存器写操作数 |
| D | unique_reg_read | 静态 | 使用的不同读寄存器数 |
| D | unique_reg_write | 静态 | 使用的不同写寄存器数 |
| E | branch_taken_exec | 动态 | 分支跳转执行次数 |
| E | branch_not_taken_exec | 动态 | 分支未跳转执行次数 |
| E | cond_branch_static | 静态 | 条件分支静态数量 |
| E | uncond_branch_static | 静态 | 无条件跳转静态数量 |
| E | loop_iter_total | 动态 | 循环总迭代次数 |
| E | call_depth_max | 动态 | 最大调用深度 |
| E | loop_depth_max | 动态 | 最大循环嵌套深度 |
| F | def_use_pairs | 动态 | 定义-使用对总数 [可选] |
| F | reg_dep_chain_max | 动态 | 最长寄存器依赖链 [可选] |
| F | mem_to_reg_exec | 动态 | 内存→寄存器传递次数 [可选] |
| F | reg_to_mem_exec | 动态 | 寄存器→内存传递次数 [可选] |
| G | reg_lifetime_total | 动态 | 寄存器值总存活指令数 [可选] |
| G | dead_write_exec | 动态 | 死写次数 [可选] |
| G | first_use_dist_total | 动态 | 定义到首次使用的总距离 [可选] |
| H | bbl_static | 静态 | 静态基本块数量 (N_static) |
| H | edge_static | 静态 | 静态控制流边数量 (E_static) |
| H | static_cyclomatic | 静态 | 静态圈复杂度 (E_static - N_static + 2) |
| H | bbl_exec | 动态 | 基本块执行次数 |
| H | unique_bbl_exec | 动态 | 实际执行的唯一基本块数 (N_dynamic) |
| H | unique_edge_exec | 动态 | 实际执行的唯一边数 (E_dynamic) |
| H | dynamic_cyclomatic | 动态 | 动态圈复杂度 (E_dynamic - N_dynamic + 2) |

---

## 十二、参考文献

1. Reis, G. A., et al. "SWIFT: Software implemented fault tolerance." CGO 2005.
2. Li, G., et al. "Understanding error propagation in deep learning neural network (DNN) accelerators and applications." SC 2017.
3. Hari, S. K. S., et al. "Relyzer: Exploiting application-level fault equivalence to analyze application resiliency to transient faults." ASPLOS 2012.
4. Feng, S., et al. "Shoestring: Probabilistic soft error reliability on the cheap." ASPLOS 2010.

---

*文档版本: 3.2*
*命名规范: _static=静态数量, _exec=动态执行次数*
*最后更新: 2024*
