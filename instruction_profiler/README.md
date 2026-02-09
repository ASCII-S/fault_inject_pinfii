# Instruction Profiler - 指令维度剖析工具

## 功能概述

对应用程序的每条静态指令进行语义剖析，输出指令级别的特征指标。用于通过崩溃时的指令地址匹配到该指令的静态特征。

## 使用方法

```bash
# 编译
cd /path/to/pinfi
make obj-intel64/instruction_profiler/instruction_profiler.so

# 运行
$PIN_ROOT/pin -t obj-intel64/instruction_profiler/instruction_profiler.so [options] -- <application>
```

### 命令行选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `-o <file>` | `instruction_profile.json` | 输出文件名 |

### 示例

```bash
$PIN_ROOT/pin -t obj-intel64/instruction_profiler/instruction_profiler.so -o my_app_profile.json -- ./my_app
```

## 输出格式

输出为 JSON 格式，包含以下结构：

```json
{
  "tool_info": {
    "name": "Instruction Profiler",
    "version": "1.0",
    "main_image": "/path/to/app",
    "base_address": "0x400000"
  },
  "instructions": [
    {
      "offset": "0x1234",
      "mnemonic": "MOV",
      "disasm": "mov rax, [rbp-0x8]",
      "size": 4,
      "category": "DATAXFER",
      "is_arith": false,
      "is_logic": false,
      "is_float": false,
      "is_simd": false,
      "is_data_move": true,
      "explicit_reg_read": ["RBP"],
      "explicit_reg_write": ["RAX"],
      "implicit_reg_read": [],
      "implicit_reg_write": [],
      "uses_flags": false,
      "is_mem_read": true,
      "is_mem_write": false,
      "mem_operand_count": 1,
      "mem_access_mode": "stack",
      "is_branch": false,
      "is_cond_branch": false,
      "is_call": false,
      "is_ret": false,
      "is_indirect": false,
      "is_crash_prone": true,
      "crash_prone_type": "mem_read"
    }
  ],
  "statistics": {
    "total_instructions": 1000
  }
}
```

## 指标说明

### A类：指令标识

| 指标 | 类型 | 说明 |
|------|------|------|
| `offset` | string | 相对于镜像基址的偏移（用于地址匹配） |
| `mnemonic` | string | 指令助记符（如 MOV, ADD） |
| `disasm` | string | 完整反汇编文本 |
| `size` | int | 指令字节长度 |

### B类：指令分类

| 指标 | 类型 | 说明 |
|------|------|------|
| `category` | string | XED 指令类别 |
| `is_arith` | bool | 是否为算术指令 |
| `is_logic` | bool | 是否为逻辑指令 |
| `is_float` | bool | 是否为浮点指令 |
| `is_simd` | bool | 是否为 SIMD 指令 |
| `is_data_move` | bool | 是否为数据移动指令 |

### C类：寄存器特征

| 指标 | 类型 | 说明 |
|------|------|------|
| `explicit_reg_read` | array | 显式读寄存器列表 |
| `explicit_reg_write` | array | 显式写寄存器列表 |
| `implicit_reg_read` | array | 隐式读寄存器列表（如 DIV 隐式使用 RAX） |
| `implicit_reg_write` | array | 隐式写寄存器列表 |
| `uses_flags` | bool | 是否读/写标志寄存器 |

### D类：访存特征

| 指标 | 类型 | 说明 |
|------|------|------|
| `is_mem_read` | bool | 是否读内存 |
| `is_mem_write` | bool | 是否写内存 |
| `mem_operand_count` | int | 内存操作数数量 |
| `mem_access_mode` | string | 寻址模式 |

**mem_access_mode 取值**：

| 值 | 说明 |
|----|------|
| `none` | 无内存访问 |
| `stack` | 栈访问（RSP/RBP 基址） |
| `array` | 数组式访问（base + index * scale） |
| `pointer` | 指针解引用（base + disp） |
| `rip_relative` | RIP 相对寻址（全局变量） |
| `absolute` | 绝对地址 |

### E类：控制流特征

| 指标 | 类型 | 说明 |
|------|------|------|
| `is_branch` | bool | 是否为分支指令 |
| `is_cond_branch` | bool | 是否为条件分支 |
| `is_call` | bool | 是否为调用指令 |
| `is_ret` | bool | 是否为返回指令 |
| `is_indirect` | bool | 是否为间接跳转/调用 |

### F类：故障敏感性

| 指标 | 类型 | 说明 |
|------|------|------|
| `is_crash_prone` | bool | 是否为易崩溃指令 |
| `crash_prone_type` | string | 易崩溃类型 |

**crash_prone_type 取值**：

| 值 | 说明 |
|----|------|
| `none` | 非易崩溃指令 |
| `mem_read` | 内存读操作 |
| `mem_write` | 内存写操作 |
| `indirect_cf` | 间接控制流 |
| `div` | 除法指令 |

## 使用场景

1. **崩溃分析**：通过崩溃时的指令地址（offset）匹配到该指令的静态特征
2. **故障注入研究**：识别易崩溃指令，分析故障敏感性
3. **程序特征分析**：统计指令类型分布、访存模式等

## 注意事项

- 仅分析主程序的可执行段，不分析动态链接库
- 需要程序包含符号信息以获得完整的指令覆盖
- 输出的 offset 是相对于镜像基址的偏移，使用时需要加上运行时基址
