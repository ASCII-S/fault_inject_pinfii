# BBL级剖析指标原理与弹性关联说明

## 概述

本文档详细说明BBL Profiler工具收集的各项BBL级指标的**计算原理**、**技术实现**以及**与软件弹性(Resilience)的关联**。

**命名规范**：
- `_static` 后缀：静态分析得到的指令数量
- `_exec` 后缀：运行时动态执行次数
- `_count` 后缀：计数（静态或动态）

---

## 一、BBL与函数的区别

### 1.1 什么是BBL（基本块）

BBL（Basic Block）是程序控制流中的基本单元，具有以下特性：
- **单入口**：只能从头部进入
- **单出口**：只能从尾部退出
- **顺序执行**：内部没有分支，指令顺序执行

### 1.2 BBL vs 函数

| 维度 | 函数 | BBL |
|------|------|-----|
| 粒度 | 粗（包含多个BBL） | 细（单个控制流单元） |
| 内部控制流 | 有分支/循环/调用 | 无分支，顺序执行 |
| 故障定位精度 | 粗略定位到函数 | 精确定位到代码块 |
| 弹性分析价值 | 函数级弹性特征 | BBL级错误传播分析 |

### 1.3 为什么分析BBL维度

1. **更精确的故障定位**：知道哪个具体代码块出了问题
2. **控制流图分析**：可以分析BBL间的跳转关系和热路径
3. **错误传播分析**：通过边统计理解错误如何在BBL间传播
4. **循环分析**：识别循环头和循环体，分析循环对弹性的影响

---

## 二、基本属性指标 (A类)

### 2.1 bbl_addr / bbl_offset

**原理说明**:
- `bbl_addr`: BBL的绝对起始地址
- `bbl_offset`: 相对于主程序基址的偏移

**技术实现**:
```cpp
ADDRINT bbl_addr = BBL_Address(bbl);
ADDRINT bbl_offset = bbl_addr - g_main_img_low;
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 偏移值 | 用于定位故障注入点 |
| 地址范围 | 判断是否在主程序内 |

---

### 2.2 inst_static / bbl_size_bytes

**原理说明**:
- `inst_static`: BBL内的静态指令数量
- `bbl_size_bytes`: BBL的字节大小

**技术实现**:
```cpp
UINT32 inst_static = BBL_NumIns(bbl);
UINT32 bbl_size_bytes = BBL_Size(bbl);
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 指令数多 | 故障暴露点多，SDC风险可能更高 |
| 指令数少 | 单次故障影响范围有限 |

---

## 三、执行统计指标 (B类)

### 3.1 exec_count

**原理说明**:
- 统计BBL在程序执行过程中被执行的总次数
- 通过在BBL入口点插入计数回调实现

**技术实现**:
```cpp
BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBBLExec,
               IARG_PTR, profile,
               IARG_UINT32, inst_count,
               IARG_END);
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高执行次数 | 热点BBL，故障影响范围大，是防护重点 |
| 低执行次数 | 冷BBL，故障影响可能被屏蔽 |
| 执行次数为0 | 死代码，不需要关注 |

**研究假设**:
- 高频执行的BBL应该优先进行故障注入测试
- 热点BBL的错误更容易被放大

---

### 3.2 inst_exec

**原理说明**:
- BBL内所有指令的动态执行总次数
- `inst_exec = inst_static × exec_count`

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高inst_exec | 指令执行密集，故障暴露机会多 |

---

## 四、控制流特征指标 (C类)

### 4.1 succ_count (后继数量)

**原理说明**:
- 统计BBL可能的后继BBL数量
- 0: ret/syscall（后继不确定）
- 1: 无条件跳转或顺序执行
- 2: 条件分支（taken + fallthrough）

**技术实现**:
```cpp
UINT32 GetSuccessorCount(INS tail) {
    if (INS_IsRet(tail) || INS_IsSyscall(tail)) return 0;
    if (INS_IsBranch(tail) && INS_HasFallThrough(tail)) return 2;
    return 1;
}
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| succ_count=2 | 分支点，错误可能导致错误路径 |
| succ_count=1 | 线性执行，错误传播路径明确 |
| succ_count=0 | 出口点，错误影响函数返回值 |

---

### 4.2 is_loop_header (循环头标识)

**原理说明**:
- 通过检测回边识别循环头
- 回边定义：跳转目标地址 < 当前BBL地址

**技术实现**:
```cpp
// 检查是否是回边（循环）
if (target < bbl_addr) {
    g_bbl_profiles[target].is_loop_header = true;
}
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 是循环头 | 错误会被循环放大，SDC风险高 |
| 非循环头 | 错误影响范围有限 |

**研究假设**:
- 循环头BBL的故障影响显著大于普通BBL
- 循环体内的错误会随迭代次数累积

---

### 4.3 terminator_type (终结指令类型)

**原理说明**:
- 分类BBL尾部指令的类型
- 反映BBL如何结束执行

**取值说明**:
| 类型 | 说明 | 弹性影响 |
|------|------|---------|
| `fallthrough` | 顺序执行 | 错误传播路径明确 |
| `direct_branch` | 直接跳转 | 可能跳过错误检测点 |
| `indirect_branch` | 间接跳转 | 跳转目标不确定，风险高 |
| `call` | 函数调用 | 错误可能传播到被调函数 |
| `ret` | 函数返回 | 错误影响返回值 |
| `syscall` | 系统调用 | 可能导致程序崩溃 |

---

### 4.4 has_indirect_branch (间接跳转标识)

**原理说明**:
- 标识BBL是否包含间接控制流转移
- 间接跳转的目标地址在寄存器中，运行时才能确定

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 有间接跳转 | 故障可能导致跳转到错误地址，崩溃风险高 |
| 无间接跳转 | 控制流可预测 |

---

## 五、计算特征指标 (D类)

### 5.1 内存访问指标

**指标说明**:
- `mem_read_static/exec`: 内存读操作
- `mem_write_static/exec`: 内存写操作
- `mem_inst_static/exec`: 访存指令（不重复计数）

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高内存读 | 依赖外部数据，错误从内存传入 |
| 高内存写 | 错误持久化到内存，影响后续操作 |
| 读写比例失衡 | 可能存在数据流瓶颈 |

---

### 5.2 计算类型指标

**指标说明**:
- `arith_static/exec`: 算术运算（ADD/SUB/MUL/DIV等）
- `logic_static/exec`: 逻辑运算（AND/OR/XOR/SHL等）
- `float_static/exec`: 浮点运算
- `simd_static/exec`: SIMD向量运算

**弹性关联**:
| 指令类型 | 弹性影响 |
|---------|---------|
| 算术指令 | 数值错误，可能传播到后续计算 |
| 逻辑指令 | 位错误可能改变条件判断 |
| 浮点指令 | 精度错误，科学计算敏感 |
| SIMD指令 | 单次故障影响多个数据通道 |

---

### 5.3 pure_compute_static/exec (纯计算指令)

**原理说明**:
- 统计不涉及内存访问的纯计算指令
- 纯计算 = (算术 || 逻辑 || 浮点 || SIMD) && !内存访问

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高纯计算比例 | 计算密集型BBL，ALU错误敏感 |
| 低纯计算比例 | 访存密集型BBL，内存错误敏感 |

---

## 六、数据依赖指标 (E类)

### 6.1 live_in_count (外部输入寄存器数)

**原理说明**:
- 统计在BBL内**定义之前使用**的寄存器数量
- 这些寄存器的值来自BBL外部

**技术实现**:
```cpp
// 对于每条指令的读寄存器
for (UINT32 i = 0; i < num_rregs; i++) {
    REG reg = INS_RegR(ins, i);
    // 如果该寄存器还没有被定义过，则是外部输入
    if (profile.def_regs.find(reg) == profile.def_regs.end()) {
        profile.use_before_def.insert(reg);
    }
}
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高live_in | BBL对外部依赖强，错误从前驱传入 |
| 低live_in | BBL相对独立，错误来源有限 |

---

### 6.2 live_out_count (可能输出寄存器数)

**原理说明**:
- 统计在BBL内**最后定义**的寄存器数量
- 这些寄存器的值可能被后继BBL使用

**技术实现**:
```cpp
// 记录每条指令定义的寄存器
for (UINT32 i = 0; i < num_wregs; i++) {
    REG reg = INS_RegW(ins, i);
    profile.last_def_regs.insert(reg);  // 可能被后续覆盖
}
// 最后剩下的就是live_out候选
```

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高live_out | BBL错误可能传播到多个后继，SDC风险高 |
| 低live_out | 错误影响范围有限 |

**研究假设**:
- `live_out_count` 高的BBL应该是防护重点
- 这类BBL的错误更容易"逃逸"到程序其他部分

---

### 6.3 mem_to_reg_exec / reg_to_mem_exec

**原理说明**:
- `mem_to_reg_exec`: 内存加载到寄存器的次数（load操作）
- `reg_to_mem_exec`: 寄存器存储到内存的次数（store操作）

**弹性关联**:
| 特征 | 弹性影响 |
|------|---------|
| 高mem_to_reg | 内存错误容易传播到计算 |
| 高reg_to_mem | 计算错误容易持久化到内存 |

---

## 七、边统计指标 (F类)

### 7.1 边的基本结构

每条边记录：
- `from_bbl`: 源BBL地址
- `to_bbl`: 目标BBL地址
- `exec_count`: 边执行次数
- `edge_type`: 边类型

### 7.2 edge_type (边类型)

| 类型 | 说明 | 检测方式 |
|------|------|---------|
| `fallthrough` | 条件分支未跳转 | INS_HasFallThrough |
| `taken` | 分支跳转 | IARG_BRANCH_TAKEN |
| `indirect` | 间接跳转 | IARG_BRANCH_TARGET_ADDR |
| `call` | 函数调用 | INS_IsCall |
| `ret` | 函数返回 | INS_IsRet |

### 7.3 弹性关联

| 特征 | 弹性影响 |
|------|---------|
| 高频边 | 热路径，错误传播的主要通道 |
| 回边（目标<源） | 循环路径，错误被放大 |
| indirect边 | 动态发现，故障可能导致意外跳转 |

**研究应用**:
- 识别热路径进行针对性防护
- 分析错误传播路径
- 检测循环对弹性的影响

---

## 八、指标间的关联分析

### 8.1 BBL类型矩阵

```
                    高live_out
                         |
        错误传播型       |      关键BBL
        (易传播)         |    (高影响力)
                         |
    ─────────────────────┼─────────────────────
                         |
        低影响BBL        |      错误吸收型
        (可忽略)         |    (错误被覆盖)
                         |
                    低live_out

    低exec_count ←───────────────→ 高exec_count
```

### 8.2 弹性预测组合

| 组合特征 | 预期弹性行为 |
|---------|-------------|
| 高exec + is_loop_header | 循环热点，错误放大风险高 |
| 高live_out + 高exec | 关键BBL，SDC风险高 |
| terminator=indirect + has_indirect_branch | 控制流不可预测，崩溃风险高 |
| 高mem_write + 高exec | 频繁写内存，错误持久化风险高 |
| 低live_in + 低live_out | 相对独立，错误影响局部 |

---

## 九、学术研究应用

### 9.1 研究问题示例

- **RQ1**: 循环头BBL的故障影响是否显著大于普通BBL？
- **RQ2**: 高`live_out_count`的BBL是否SDC风险更高？
- **RQ3**: BBL执行频率与故障屏蔽率的关系？
- **RQ4**: 不同`terminator_type`的BBL弹性特征差异？
- **RQ5**: 热路径上的BBL故障影响范围如何？

### 9.2 数据收集流程

```
1. 运行bbl_profiler获取BBL特征
   ↓
2. 对每个BBL进行故障注入实验
   ↓
3. 记录故障结果：SDC / Crash / Masked / Detected
   ↓
4. 建立特征-结果数据集
   ↓
5. 统计分析/机器学习建模
```

### 9.3 分析方法建议

1. **相关性分析**: 计算各指标与故障结果的Pearson/Spearman相关系数
2. **分类分析**: 基于BBL类型（循环头/入口/出口）对比弹性差异
3. **聚类分析**: 基于多维指标对BBL进行聚类，识别弹性模式
4. **路径分析**: 利用边统计分析热路径和错误传播路径

---

## 十、指标汇总表

| 类别 | 指标名 | 类型 | 说明 |
|------|--------|------|------|
| A | bbl_addr | 静态 | BBL起始地址 |
| A | bbl_offset | 静态 | 相对偏移 |
| A | function_name | 静态 | 所属函数 |
| A | inst_static | 静态 | 静态指令数 |
| A | bbl_size_bytes | 静态 | 字节大小 |
| B | exec_count | 动态 | BBL执行次数 |
| B | inst_exec | 动态 | 指令执行次数 |
| C | succ_count | 静态 | 后继数量 |
| C | is_loop_header | 静态 | 循环头标识 |
| C | is_function_entry | 静态 | 函数入口标识 |
| C | is_function_exit | 静态 | 函数出口标识 |
| C | terminator_type | 静态 | 终结类型 |
| C | has_indirect_branch | 静态 | 间接跳转标识 |
| D | mem_read_static/exec | 两者 | 内存读 |
| D | mem_write_static/exec | 两者 | 内存写 |
| D | arith_static/exec | 两者 | 算术指令 |
| D | logic_static/exec | 两者 | 逻辑指令 |
| D | float_static/exec | 两者 | 浮点指令 |
| D | simd_static/exec | 两者 | SIMD指令 |
| D | data_movement_static/exec | 两者 | 数据移动 |
| D | pure_compute_static/exec | 两者 | 纯计算 |
| E | live_in_count | 静态 | 外部输入寄存器 |
| E | live_out_count | 静态 | 可能输出寄存器 |
| E | def_count | 静态 | 定义总数 |
| E | use_count | 静态 | 使用总数 |
| E | reg_read_exec | 动态 | 寄存器读 |
| E | reg_write_exec | 动态 | 寄存器写 |
| E | mem_to_reg_exec | 动态 | 内存→寄存器 |
| E | reg_to_mem_exec | 动态 | 寄存器→内存 |
| F | from_bbl | - | 源BBL |
| F | to_bbl | - | 目标BBL |
| F | exec_count | 动态 | 边执行次数 |
| F | edge_type | 静态 | 边类型 |

---

## 十一、参考文献

1. Reis, G. A., et al. "SWIFT: Software implemented fault tolerance." CGO 2005.
2. Hari, S. K. S., et al. "Relyzer: Exploiting application-level fault equivalence to analyze application resiliency to transient faults." ASPLOS 2012.
3. Thomas, A., Pattabiraman, K. "LLFI: An Intermediate Code Level Fault Injection Tool for Hardware Faults." QRS 2018.
4. Feng, S., et al. "Shoestring: Probabilistic soft error reliability on the cheap." ASPLOS 2010.

---

*文档版本: 1.0*
*命名规范: _static=静态数量, _exec=动态执行次数*
