# 指令维度剖析工具 - 指标文档

本文档详细解释 Instruction Profiler 输出的各项指标的含义、取值范围和使用场景。

## 目录

- [A类：指令标识](#a类指令标识)
- [B类：指令分类](#b类指令分类)
- [C类：寄存器特征](#c类寄存器特征)
- [D类：访存特征](#d类访存特征)
- [E类：控制流特征](#e类控制流特征)
- [F类：故障敏感性](#f类故障敏感性)

---

## A类：指令标识

用于唯一标识和定位指令的基本属性。

### offset

- **类型**: string (十六进制)
- **说明**: 指令相对于主程序镜像基址的偏移量
- **用途**:
  - 崩溃分析时，通过 `崩溃地址 - 运行时基址` 计算 offset，匹配到对应指令
  - 与其他工具（如 unified_tracer）的输出进行关联
- **示例**: `"0x1234"`

### mnemonic

- **类型**: string
- **说明**: 指令助记符，即指令的操作码名称
- **用途**: 快速识别指令类型
- **示例**: `"MOV"`, `"ADD"`, `"JMP"`, `"CALL"`

### disasm

- **类型**: string
- **说明**: 完整的反汇编文本，包含操作数
- **用途**: 人工分析时查看指令的完整语义
- **示例**: `"mov rax, qword ptr [rbp-0x8]"`

### size

- **类型**: int
- **说明**: 指令的字节长度
- **取值范围**: 1-15 (x86-64 指令最长 15 字节)
- **用途**:
  - 计算下一条指令地址
  - 分析代码密度
- **示例**: `4`

---

## B类：指令分类

基于 XED (X86 Encoder Decoder) 的指令语义分类。

### category

- **类型**: string
- **说明**: XED 定义的指令类别
- **常见取值**:
  | 值 | 说明 |
  |----|------|
  | `BINARY` | 二进制算术运算 (ADD, SUB, MUL, DIV 等) |
  | `LOGICAL` | 逻辑运算 (AND, OR, XOR, TEST 等) |
  | `DATAXFER` | 数据传输 (MOV, MOVZX, MOVSX 等) |
  | `COND_BR` | 条件分支 (JE, JNE, JG 等) |
  | `UNCOND_BR` | 无条件分支 (JMP) |
  | `CALL` | 函数调用 |
  | `RET` | 函数返回 |
  | `PUSH` | 压栈 |
  | `POP` | 出栈 |
  | `SSE` | SSE 指令 |
  | `AVX` | AVX 指令 |
  | `AVX2` | AVX2 指令 |
  | `X87_ALU` | x87 浮点运算 |
  | `SHIFT` | 移位操作 |
  | `ROTATE` | 循环移位 |
  | `BITBYTE` | 位操作 |
  | `NOP` | 空操作 |
  | `MISC` | 其他 |

### is_arith

- **类型**: bool
- **说明**: 是否为算术运算指令
- **判断依据**: category 为 `BINARY`, `DECIMAL`, `AVX`, `AVX2`, `SSE`
- **典型指令**: ADD, SUB, MUL, IMUL, DIV, IDIV, INC, DEC
- **用途**: 识别计算密集型代码区域

### is_logic

- **类型**: bool
- **说明**: 是否为逻辑运算指令
- **判断依据**: category 为 `LOGICAL`, `BITBYTE`, `SHIFT`, `ROTATE`
- **典型指令**: AND, OR, XOR, NOT, TEST, SHL, SHR, SAR, ROL, ROR
- **用途**: 识别位操作密集型代码

### is_float

- **类型**: bool
- **说明**: 是否为浮点运算指令
- **判断依据**: category 为 `X87_ALU`, `FCMOV`, `SSE`
- **典型指令**: FADD, FSUB, FMUL, FDIV, ADDSS, MULSD
- **用途**: 识别浮点计算代码

### is_simd

- **类型**: bool
- **说明**: 是否为 SIMD (单指令多数据) 向量指令
- **判断依据**: category 为 `SSE`, `AVX`, `AVX2`, `AVX512`, `MMX`
- **典型指令**: MOVAPS, ADDPS, MULPD, VADDPS
- **用途**: 识别向量化代码，分析并行计算特征

### is_data_move

- **类型**: bool
- **说明**: 是否为数据移动指令
- **判断依据**: category 为 `DATAXFER`, `PUSH`, `POP`
- **典型指令**: MOV, MOVZX, MOVSX, LEA, PUSH, POP, XCHG
- **用途**: 分析数据流动模式

---

## C类：寄存器特征

描述指令对寄存器的读写操作，区分显式和隐式操作数。

### explicit_reg_read

- **类型**: array of string
- **说明**: 指令显式读取的寄存器列表
- **显式定义**: 在指令编码中明确指定的操作数
- **包含**:
  - 源操作数寄存器
  - 内存操作数的基址寄存器 (base)
  - 内存操作数的索引寄存器 (index)
- **示例**:
  - `mov rax, rbx` → `["rbx"]`
  - `mov rax, [rbp+rcx*4]` → `["rbp", "rcx"]`

### explicit_reg_write

- **类型**: array of string
- **说明**: 指令显式写入的寄存器列表
- **显式定义**: 在指令编码中明确指定的目标操作数
- **示例**:
  - `mov rax, rbx` → `["rax"]`
  - `add rax, rbx` → `["rax", "rflags"]`

### implicit_reg_read

- **类型**: array of string
- **说明**: 指令隐式读取的寄存器列表
- **隐式定义**: 指令语义固定使用，但不在操作数中显式编码
- **典型场景**:
  | 指令 | 隐式读寄存器 |
  |------|-------------|
  | `MUL rbx` | RAX |
  | `DIV rbx` | RAX, RDX |
  | `REP MOVSB` | RSI, RDI, RCX |
  | `PUSH rax` | RSP |
  | `RET` | RSP |
- **示例**: `["rax", "rdx"]`

### implicit_reg_write

- **类型**: array of string
- **说明**: 指令隐式写入的寄存器列表
- **典型场景**:
  | 指令 | 隐式写寄存器 |
  |------|-------------|
  | `MUL rbx` | RAX, RDX |
  | `DIV rbx` | RAX, RDX |
  | `CALL func` | RSP |
  | `RET` | RSP |
  | `CPUID` | RAX, RBX, RCX, RDX |
- **示例**: `["rax", "rdx"]`

### uses_flags

- **类型**: bool
- **说明**: 是否读取或写入标志寄存器 (RFLAGS/EFLAGS)
- **用途**:
  - 识别影响条件分支的指令
  - 分析数据依赖链
- **典型场景**:
  - 写 flags: ADD, SUB, CMP, TEST, AND, OR
  - 读 flags: JE, JNE, CMOVE, ADC, SBB

---

## D类：访存特征

描述指令的内存访问行为和寻址模式。

### is_mem_read

- **类型**: bool
- **说明**: 指令是否从内存读取数据
- **典型指令**: MOV reg, [mem], CMP reg, [mem], PUSH [mem]
- **用途**: 识别内存读操作，分析缓存行为

### is_mem_write

- **类型**: bool
- **说明**: 指令是否向内存写入数据
- **典型指令**: MOV [mem], reg, PUSH reg, ADD [mem], reg
- **用途**: 识别内存写操作，分析数据持久化点

### mem_operand_count

- **类型**: int
- **说明**: 内存操作数的数量
- **取值范围**: 0-2 (大多数指令 0 或 1)
- **特殊情况**:
  - `MOVSB` 等字符串指令有 2 个内存操作数
  - `PUSH [mem]` 有 2 个内存操作数 (读源 + 写栈)

### mem_access_mode

- **类型**: string
- **说明**: 内存寻址模式的静态分类
- **取值及含义**:

| 值 | 寻址形式 | 说明 | 典型场景 |
|----|----------|------|----------|
| `none` | - | 无内存访问 | 寄存器间操作 |
| `stack` | `[RSP+disp]` 或 `[RBP+disp]` | 栈访问 | 局部变量、函数参数 |
| `array` | `[base+index*scale+disp]` | 数组式访问 | 数组遍历、查表 |
| `pointer` | `[base+disp]` 或 `[base]` | 指针解引用 | 结构体访问、链表 |
| `rip_relative` | `[RIP+disp]` | RIP 相对寻址 | 全局变量、常量 |
| `absolute` | `[disp]` | 绝对地址 | 固定地址访问 (少见) |

**判断逻辑**:
```
if base == RIP:
    return "rip_relative"
elif base in {RSP, RBP} and index == INVALID:
    return "stack"
elif index != INVALID:
    return "array"
elif base != INVALID:
    return "pointer"
else:
    return "absolute"
```

---

## E类：控制流特征

描述指令对程序控制流的影响。

### is_branch

- **类型**: bool
- **说明**: 是否为分支指令 (改变程序执行流)
- **包含**: 条件跳转、无条件跳转
- **不包含**: CALL, RET (有专门字段)
- **典型指令**: JMP, JE, JNE, JG, JL, LOOP

### is_cond_branch

- **类型**: bool
- **说明**: 是否为条件分支指令
- **判断依据**: 是分支指令且有 fall-through 路径
- **典型指令**: JE, JNE, JG, JGE, JL, JLE, JA, JB, JO, JS
- **用途**: 识别分支预测敏感点

### is_call

- **类型**: bool
- **说明**: 是否为函数调用指令
- **典型指令**: CALL (直接/间接)
- **用途**: 识别函数边界，分析调用图

### is_ret

- **类型**: bool
- **说明**: 是否为函数返回指令
- **典型指令**: RET, RETN
- **用途**: 识别函数出口

### is_indirect

- **类型**: bool
- **说明**: 是否为间接控制流转移
- **包含**:
  - 间接跳转: `JMP rax`, `JMP [mem]`
  - 间接调用: `CALL rax`, `CALL [mem]`
  - 返回指令: `RET` (目标地址从栈读取)
- **用途**:
  - 识别控制流劫持风险点
  - 分析虚函数调用、函数指针

---

## F类：故障敏感性

描述指令在故障注入场景下的敏感性特征。

### is_crash_prone

- **类型**: bool
- **说明**: 是否为易崩溃指令
- **判断依据**: `crash_prone_type != "none"`
- **用途**: 快速筛选高风险指令

### crash_prone_type

- **类型**: string
- **说明**: 易崩溃的具体类型
- **取值及风险分析**:

| 值 | 说明 | 崩溃机制 | 风险等级 |
|----|------|----------|----------|
| `none` | 非易崩溃指令 | - | 低 |
| `mem_read` | 内存读操作 | 地址错误导致 SIGSEGV | 中 |
| `mem_write` | 内存写操作 | 地址错误导致 SIGSEGV，数据损坏 | 高 |
| `indirect_cf` | 间接控制流 | 跳转到非法地址导致 SIGILL/SIGSEGV | 高 |
| `div` | 除法指令 | 除零导致 SIGFPE | 高 |

**优先级说明**:
判断顺序为 `div > indirect_cf > mem_write > mem_read`，即一条指令只会被标记为一种类型。

**典型崩溃场景**:

1. **mem_read**:
   - 空指针解引用
   - 数组越界读
   - 释放后使用 (use-after-free)

2. **mem_write**:
   - 空指针写入
   - 缓冲区溢出
   - 栈破坏

3. **indirect_cf**:
   - 虚表指针损坏
   - 函数指针被覆盖
   - 返回地址被篡改

4. **div**:
   - 除数为零
   - 整数溢出 (IDIV)

---

## 使用示例

### 1. 通过 offset 匹配崩溃指令

```python
import json

# 加载剖析结果
with open('instruction_profile.json') as f:
    profile = json.load(f)

# 崩溃地址 (从 core dump 获取)
crash_addr = 0x401234
base_addr = int(profile['tool_info']['base_address'], 16)
crash_offset = hex(crash_addr - base_addr)

# 查找匹配指令
for inst in profile['instructions']:
    if inst['offset'] == crash_offset:
        print(f"崩溃指令: {inst['disasm']}")
        print(f"类型: {inst['crash_prone_type']}")
        print(f"访存模式: {inst['mem_access_mode']}")
        break
```

### 2. 统计指令类型分布

```python
from collections import Counter

categories = Counter(inst['category'] for inst in profile['instructions'])
print("指令类别分布:")
for cat, count in categories.most_common(10):
    print(f"  {cat}: {count}")
```

### 3. 筛选高风险指令

```python
high_risk = [
    inst for inst in profile['instructions']
    if inst['crash_prone_type'] in ('mem_write', 'indirect_cf', 'div')
]
print(f"高风险指令数量: {len(high_risk)}")
```

---

## 参考资料

- [Intel XED Documentation](https://intelxed.github.io/)
- [Intel Pin User Guide](https://software.intel.com/sites/landingpage/pintool/docs/98749/Pin/html/)
- [x86-64 Instruction Reference](https://www.felixcloutier.com/x86/)
