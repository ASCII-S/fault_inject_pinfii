# targeted_faultinjection - 定向故障注入工具

## 概述

`targeted_faultinjection.so` 是一个基于 Intel Pin 的精确故障注入工具，用于在指定的程序计数器（PC）地址、指定的寄存器、指定的执行次数时进行比特翻转注错。

与随机故障注入工具（如 `faultinjection.so`）不同，此工具提供精确的控制：
- **指定 PC 地址**：只在特定指令位置注错
- **指定寄存器**：只对指定的寄存器注错
- **指定执行次数**：在第 K 次执行时注错（从 1 开始）
- **指定比特位**：可精确控制翻转哪一位，或随机选择

## 主要功能

1. **精确目标匹配**：根据绝对地址定位目标指令
2. **执行计数**：统计目标指令的执行次数
3. **定时注错**：在第 K 次执行后对目标寄存器进行比特翻转
4. **详细信息输出**：输出完整的注错信息供后续分析或 GDB 修复使用
5. **支持多种寄存器类型**：
   - 通用寄存器（rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8-r15）
   - XMM 寄存器（xmm0-xmm15）
   - YMM 寄存器（ymm0-ymm15）

## 编译

```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/targeted_fi/targeted_faultinjection.so
```

## 命令行参数

| 参数 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `-target_pc` | UINT64 | 是 | 0 | 目标指令的绝对地址（十六进制） |
| `-target_reg` | string | 是 | "" | 目标寄存器名称（如 rax, xmm0） |
| `-target_kth` | UINT64 | 否 | 1 | 在第 K 次执行时注错（从 1 开始） |
| `-inject_bit` | INT32 | 否 | -1 | 要翻转的比特位置（-1=随机） |
| `-high_bit_only` | BOOL | 否 | 0 | 是否只在高位注错（1=是，0=否） |
| `-o` | string | 否 | inject_info.txt | 注错信息输出文件路径 |

## 使用示例

### 示例 1: 基本用法 - 指定比特位注错

```bash
# 在 0x402a16 处的 rsi 寄存器第 1 次执行时，翻转第 0 位
/home/tongshiyu/pin/pin \
    -t obj-intel64/targeted_fi/targeted_faultinjection.so \
    -target_pc 0x402a16 \
    -target_reg rsi \
    -target_kth 1 \
    -inject_bit 0 \
    -o inject_info.txt \
    -- /bin/ls /tmp
```

### 示例 2: 随机比特位注错

```bash
# 在第 100 次执行时，对 rax 随机翻转一个比特位
/home/tongshiyu/pin/pin \
    -t obj-intel64/targeted_fi/targeted_faultinjection.so \
    -target_pc 0x401234 \
    -target_reg rax \
    -target_kth 100 \
    -inject_bit -1 \
    -o inject_info.txt \
    -- ./myprogram arg1 arg2
```

### 示例 3: 只在高位注错

```bash
# 对 64 位寄存器 rdx 只在高 32 位随机注错
/home/tongshiyu/pin/pin \
    -t obj-intel64/targeted_fi/targeted_faultinjection.so \
    -target_pc 0x405678 \
    -target_reg rdx \
    -target_kth 50 \
    -inject_bit -1 \
    -high_bit_only 1 \
    -o inject_info.txt \
    -- ./myprogram
```

### 示例 4: XMM 寄存器注错

```bash
# 对 XMM0 寄存器注错（128 位）
/home/tongshiyu/pin/pin \
    -t obj-intel64/targeted_fi/targeted_faultinjection.so \
    -target_pc 0x403000 \
    -target_reg xmm0 \
    -target_kth 1 \
    -inject_bit 64 \
    -o inject_info.txt \
    -- ./fp_program
```

### 示例 5: 配合 GDB 调试

```bash
# 使用 -appdebug 让 GDB 连接
/home/tongshiyu/pin/pin \
    -appdebug -debug_port 12345 \
    -t obj-intel64/targeted_fi/targeted_faultinjection.so \
    -target_pc 0x4019c8 \
    -target_reg rax \
    -target_kth 459 \
    -inject_bit 31 \
    -o inject_info.txt \
    -- ./myprogram

# 在另一个终端中：
gdb -ex "target remote :12345" ./myprogram
```

## 输出文件格式

注错成功后，会生成一个文本文件（由 `-o` 参数指定），包含以下信息：

```
inject_pc: 0x402a16                      # 注错指令地址
inject_inst: mov rdi, qword ptr [rsi]   # 注错指令反汇编
inject_reg: rsi                          # 注错寄存器
inject_kth: 1                            # 第几次执行时注错
dynamic_ins_count: 123456                # 注错时的动态指令数
original_value: 0x7ffff6317728           # 原始寄存器值
injected_value: 0x7ffff6317729           # 注错后寄存器值
inject_bit: 0                            # 翻转的比特位
next_pc: 0x402a19                        # 下一条指令地址
regw_list: rdi                           # 该指令写的寄存器列表
stackw: no                               # 是否栈写操作
base: rsi                                # 内存操作基址寄存器
index: none                              # 内存操作索引寄存器
displacement: 0                          # 内存操作偏移
scale: 0                                 # 内存操作缩放因子
```

## 工作流程

```
1. Pin 启动程序
   ↓
2. Instruction() 遍历所有指令
   ├── 检查指令地址是否匹配 target_pc
   └── 如果匹配：
       ├── 保存指令信息（反汇编、下一条指令地址等）
       ├── 提取内存操作信息（base、index、displacement、scale）
       └── 插入 Analyze_TargetInst() 回调（IPOINT_AFTER）
   ↓
3. 程序执行
   ├── 每次执行到 target_pc 时，Analyze_TargetInst() 被调用
   ├── 执行计数 (exec_count++)
   └── 当 exec_count == target_kth 时：
       ├── 确定注入比特位置（指定或随机）
       ├── 读取寄存器原值
       ├── 执行比特翻转
       │   ├── 通用寄存器：PIN_GetContextReg() / PIN_SetContextReg()
       │   ├── XMM 寄存器：PIN_GetContextFPState() / PIN_SetContextFPState()
       │   └── YMM 寄存器：PIN_GetContextFPState() / PIN_SetContextFPState()
       ├── 写入注错信息文件
       └── PIN_ExecuteAt(ctxt) 继续执行
   ↓
4. 程序继续运行（可能崩溃或正常退出）
   ↓
5. Fini() 输出统计信息
```

## 寄存器名称

### 通用寄存器（64位）
- `rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi`, `rbp`, `rsp`
- `r8`, `r9`, `r10`, `r11`, `r12`, `r13`, `r14`, `r15`

### 通用寄存器（32位，自动归一化到64位）
- `eax`, `ebx`, `ecx`, `edx`, `esi`, `edi`, `ebp`, `esp`

### 通用寄存器（16位，自动归一化到64位）
- `ax`, `bx`, `cx`, `dx`

### XMM 寄存器（128位）
- `xmm0`, `xmm1`, `xmm2`, ..., `xmm15`

### YMM 寄存器（256位）
- `ymm0`, `ymm1`, `ymm2`, ..., `ymm15`

## 比特位说明

不同寄存器的比特位范围：
- **8位寄存器**：0-7
- **16位寄存器**：0-15
- **32位寄存器**：0-31
- **64位寄存器**：0-63
- **XMM 寄存器（128位）**：0-127
- **YMM 寄存器（256位）**：0-255

## 常见用法

### 获取目标 PC 地址

使用 `crashprone_tracer/unified_tracer.so` 识别易崩溃指令：

```bash
# 1. 运行溯源工具
/home/tongshiyu/pin/pin \
    -t obj-intel64/crashprone_tracer/unified_tracer.so \
    -o trace.json \
    -depth 3 \
    -- ./myprogram

# 2. 查看结果（需要安装 jq）
jq '.crashprone_insts[0] | {offset, disasm, crash_regs}' trace.json

# 输出示例：
# {
#   "offset": "0x2a16",
#   "disasm": "mov rdi, qword ptr [rsi]",
#   "crash_regs": ["rsi"]
# }

# 3. 计算绝对地址
# 绝对地址 = 镜像基址 + offset
# 如果镜像基址是 0x400000，则：
# target_pc = 0x400000 + 0x2a16 = 0x402a16
```

### 批量注错

对同一指令的多个寄存器进行注错：

```bash
# 对 rax 注错
pin -t obj-intel64/targeted_fi/targeted_faultinjection.so \
    -target_pc 0x401234 -target_reg rax -target_kth 1 \
    -inject_bit -1 -o inject_rax.txt -- ./program

# 对 rbx 注错
pin -t obj-intel64/targeted_fi/targeted_faultinjection.so \
    -target_pc 0x401234 -target_reg rbx -target_kth 1 \
    -inject_bit -1 -o inject_rbx.txt -- ./program
```

## 注意事项

1. **绝对地址 vs 偏移**：
   - `-target_pc` 参数使用绝对地址（运行时地址）
   - `crashprone_tracer` 输出的是偏移地址（offset）
   - 需要加上镜像基址：`target_pc = img_base + offset`

2. **执行次数从 1 开始**：
   - `-target_kth 1` 表示第 1 次执行
   - `-target_kth 100` 表示第 100 次执行

3. **比特位范围**：
   - 如果 `-inject_bit` 超出寄存器位宽，会自动调整到最大值
   - 例如对 64 位寄存器指定 `-inject_bit 100`，会自动调整为 63

4. **随机种子**：
   - 随机比特位使用 `time(NULL)` 作为种子
   - 每次运行结果可能不同

5. **IPOINT_AFTER**：
   - 注错发生在指令执行**之后**
   - 如果需要在指令执行前注错，需要修改源码

6. **性能影响**：
   - Pin 本身有性能开销（通常 5-10 倍慢）
   - 本工具只插桩一条指令，额外开销很小

## 错误处理

### 未找到目标指令

```
[targeted_fi] 程序退出，目标指令共执行 0 次
[警告] 未执行注错！目标指令执行次数 0 < 目标次数 1
```

**原因**：
- 目标 PC 地址不正确
- 程序未执行到该地址（可能因为条件分支跳过）

**解决**：
- 检查 PC 地址是否正确（使用 `objdump -d` 或 GDB 验证）
- 确认镜像基址

### 执行次数不足

```
[targeted_fi] 程序退出，目标指令共执行 50 次
[警告] 未执行注错！目标指令执行次数 50 < 目标次数 100
```

**原因**：
- 指令实际执行次数少于 `-target_kth` 参数

**解决**：
- 降低 `-target_kth` 值
- 检查程序逻辑

### 无法识别寄存器

```
[错误] 无法识别寄存器名称: rxx
```

**原因**：
- 寄存器名称拼写错误或不支持

**解决**：
- 检查寄存器名称拼写（区分大小写）
- 参考"寄存器名称"章节

## 与其他工具集成

### 与 crashprone_tracer 配合使用

完整工作流程：

```bash
# 1. 识别易崩溃指令
pin -t obj-intel64/crashprone_tracer/unified_tracer.so \
    -o trace.json -depth 5 -- ./program

# 2. 提取目标信息（Python/jq）
# 假设得到：offset=0x2a16, reg=rsi, img_base=0x400000

# 3. 执行注错
pin -t obj-intel64/targeted_fi/targeted_faultinjection.so \
    -target_pc 0x402a16 \
    -target_reg rsi \
    -target_kth 1 \
    -inject_bit -1 \
    -o inject_info.txt \
    -- ./program

# 4. 分析结果
cat inject_info.txt
```

### 与 Python 脚本集成

参见 `PYTHON_API.md` 了解如何在 Python 中调用此工具。

## 技术细节

### Pin API 使用

| API | 用途 |
|-----|------|
| `INS_AddInstrumentFunction()` | 注册指令插桩函数 |
| `INS_Address(ins)` | 获取指令地址 |
| `INS_Disassemble(ins)` | 获取指令反汇编 |
| `INS_InsertCall()` | 插入分析回调 |
| `PIN_GetContextReg(ctxt, reg)` | 读取通用寄存器值 |
| `PIN_SetContextReg(ctxt, reg, val)` | 写入通用寄存器值 |
| `PIN_GetContextFPState(ctxt, fpstate)` | 读取浮点状态 |
| `PIN_SetContextFPState(ctxt, fpstate)` | 写入浮点状态 |
| `PIN_ExecuteAt(ctxt)` | 从修改后的上下文继续执行 |

### 实现要点

1. **单指令插桩**：只对匹配的指令插桩，性能开销小
2. **IPOINT_AFTER**：在指令执行后注错，确保原始语义正确执行
3. **PIN_ExecuteAt()**：注错后立即继续执行，不影响后续指令
4. **比特翻转**：使用 XOR 操作 (`value ^ (1UL << bit)`)

## 常见问题 (FAQ)

**Q: 如何确定镜像基址？**

A: 运行 `unified_tracer.so` 时，会输出主镜像地址：
```
[UnifiedTracer] 主镜像: /bin/ls (0x400000 - 0x41da63)
                                   ^^^^^^^^ 这是基址
```

**Q: 可以同时对多个寄存器注错吗？**

A: 不可以。每次运行只能对一个寄存器注错。如需对多个寄存器注错，需要多次运行工具。

**Q: 注错后程序一定会崩溃吗？**

A: 不一定。注错效果取决于：
- 比特位位置（高位影响更大）
- 寄存器用途（寻址寄存器更容易崩溃）
- 程序逻辑（有些错误可能被容忍）

**Q: 如何调试注错后的程序？**

A: 使用 `-appdebug` 参数配合 GDB：
```bash
# 终端1：启动 Pin
pin -appdebug -debug_port 12345 -t targeted_faultinjection.so ... -- ./program

# 终端2：连接 GDB
gdb -ex "target remote :12345" ./program
```

**Q: 支持 AVX-512 寄存器（zmm）吗？**

A: 当前版本不支持。只支持 XMM（128位）和 YMM（256位）。

## 参考资料

- [Intel Pin 官方文档](https://software.intel.com/sites/landingpage/pintool/docs/98437/Pin/html/)
- [Pin API Reference](https://software.intel.com/sites/landingpage/pintool/docs/98437/Pin/html/group__API__REF.html)
- `crashprone_tracer/README.md` - 易崩溃指令识别工具
- `PYTHON_API.md` - Python 接口文档
