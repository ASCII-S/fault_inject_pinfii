# Crashprone Tracer

基于 Intel Pin 的易崩溃指令识别与数据流溯源工具。

## 功能

在程序动态执行过程中：
1. **识别易崩溃指令** - 可能导致程序崩溃的指令
2. **提取崩溃寄存器** - 只提取可能导致崩溃的寄存器（非所有读寄存器）
3. **数据流溯源** - 对每个崩溃寄存器独立进行后向数据流溯源

## 易崩溃指令类型

| 类型 | 说明 | 崩溃寄存器 |
|------|------|-----------|
| `mem_write` | 内存写 | base, index 寄存器 |
| `mem_read` | 内存读 | base, index 寄存器 |
| `index_access` | 带索引的内存访问 | base, index 寄存器 |
| `indirect_cf` | 间接跳转/调用 | 目标地址寄存器 |
| `div` | 除法指令 | 除数寄存器 |

## 编译

```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/crashprone_tracer/unified_tracer.so
```

## 使用

```bash
pin -t obj-intel64/crashprone_tracer/unified_tracer.so [options] -- <program> [args]
```

### 参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-o <file>` | 输出 JSON 文件路径 | `unified_trace.json` |
| `-depth <n>` | 最大溯源深度 | `5` |
| `-min_exec <n>` | 最小执行次数过滤 | `2` |

### 示例

```bash
# 基本用法
pin -t obj-intel64/crashprone_tracer/unified_tracer.so -o result.json -- ./myprogram

# 指定溯源深度
pin -t obj-intel64/crashprone_tracer/unified_tracer.so -o result.json -depth 3 -- ./myprogram arg1 arg2
```

## 输出格式

```json
{
  "config": {
    "img_name": "/path/to/program",
    "img_base_addr": "0x400000",
    "max_depth": 5,
    "min_exec_count": 2,
    "window_size": 10000
  },
  "crashprone_insts": [
    {
      "offset": "0x1234",
      "disasm": "mov rdi, qword ptr [rax+rbx*8]",
      "type": "index_access",
      "exec_count": 100,
      "crash_regs": ["rax", "rbx"],
      "register_traces": {
        "rax": [
          {"offset": "0x1200", "disasm": "mov rax, [rdi]", "depth": 1, "hit_count": 50}
        ],
        "rbx": [
          {"offset": "0x1210", "disasm": "mov rbx, rsi", "depth": 1, "hit_count": 100}
        ]
      }
    }
  ],
  "statistics": {
    "total_crashprone_insts": 89,
    "insts_with_traces": 50,
    "total_source_entries": 112,
    "total_instructions_executed": 40390
  }
}
```

### 字段说明

#### Config 部分
- `img_name`: 主程序镜像路径
- `img_base_addr`: 主镜像基地址（十六进制字符串）
- `max_depth`: 最大溯源深度
- `min_exec_count`: 最小执行次数过滤阈值
- `window_size`: 环形缓冲区大小

#### 易崩溃指令部分
- `offset`: 指令相对于主镜像基址的偏移
- `disasm`: 指令反汇编
- `type`: 易崩溃类型
- `exec_count`: 执行次数
- `crash_regs`: 可能导致崩溃的寄存器列表
- `register_traces`: 每个崩溃寄存器的溯源链
  - `depth`: 溯源深度（1=直接来源）
  - `hit_count`: 命中次数

#### 计算绝对地址
在进行故障注入时，需要使用指令的绝对地址：

```
绝对地址 = config.img_base_addr + crashprone_inst.offset
例如: 0x400000 + 0x1234 = 0x401234
```

## 技术原理

### 崩溃寄存器提取

只溯源可能导致崩溃的寄存器，而非所有读寄存器：

```
mov [rax+rbx*4], rcx
    ↑    ↑       ↑
   base index   不溯源(只是数据源)
```

### 数据流溯源

使用环形缓冲区 + Shadow Registers + BFS 算法：

1. **环形缓冲区** (10000条): 记录最近执行的动态指令
2. **Shadow Registers**: 映射每个寄存器到最后写入它的动态指令
3. **BFS回溯**: 从崩溃寄存器出发，逐层回溯找到源指令

## 文件结构

```
crashprone_tracer/
├── unified_tracer.h    # 数据结构定义
├── unified_tracer.cpp  # 核心实现
└── README.md           # 本文档
```
