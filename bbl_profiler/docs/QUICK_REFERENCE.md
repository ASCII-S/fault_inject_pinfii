# BBL剖析指标快速参考表

## 命名规范

| 后缀 | 含义 | 示例 |
|------|------|------|
| `_static` | 静态分析得到的指令数量 | `inst_static`, `arith_static` |
| `_exec` | 运行时动态执行次数 | `exec_count`, `arith_exec` |
| `_count` | 计数（静态或动态） | `live_in_count`, `succ_count` |

---

## A类: 基本属性（静态）

| 指标 | 类型 | 说明 |
|------|------|------|
| `bbl_addr` | 静态 | BBL起始地址（绝对地址） |
| `bbl_offset` | 静态 | 相对主程序基址的偏移 |
| `function_name` | 静态 | 所属函数名 |
| `inst_static` | 静态 | BBL内静态指令数量 |
| `bbl_size_bytes` | 静态 | BBL大小（字节） |

---

## B类: 执行统计（动态）

| 指标 | 类型 | 说明 |
|------|------|------|
| `exec_count` | 动态 | BBL执行次数 |
| `inst_exec` | 动态 | 总指令执行次数 = inst_static × exec_count |

---

## C类: 控制流特征

| 指标 | 类型 | 说明 |
|------|------|------|
| `succ_count` | 静态 | 后继BBL数量（1=顺序/无条件，2=条件分支，0=ret/syscall） |
| `is_loop_header` | 静态 | 是否是循环头（有回边指向） |
| `is_function_entry` | 静态 | 是否是函数入口块 |
| `is_function_exit` | 静态 | 是否是函数出口块（含RET指令） |
| `terminator_type` | 静态 | 终结指令类型 |
| `has_indirect_branch` | 静态 | 是否含间接跳转 |

**terminator_type 取值**：
- `fallthrough`: 顺序执行到下一BBL
- `direct_branch`: 直接条件/无条件跳转
- `indirect_branch`: 间接跳转
- `call`: 函数调用
- `ret`: 函数返回
- `syscall`: 系统调用

---

## D类: 计算特征

### 静态指标

| 指标 | 类型 | 说明 |
|------|------|------|
| `mem_read_static` | 静态 | 内存读指令静态数量 |
| `mem_write_static` | 静态 | 内存写指令静态数量 |
| `mem_inst_static` | 静态 | 访存指令静态数量（不重复计数） |
| `arith_static` | 静态 | 算术指令静态数量（ADD/SUB/MUL/DIV等） |
| `logic_static` | 静态 | 逻辑指令静态数量（AND/OR/XOR/SHL等） |
| `float_static` | 静态 | 浮点指令静态数量 |
| `simd_static` | 静态 | SIMD指令静态数量（SSE/AVX等） |
| `data_movement_static` | 静态 | 数据移动指令静态数量（MOV/LEA等） |
| `pure_compute_static` | 静态 | 纯计算指令静态数量（不涉及内存） |

### 动态指标

| 指标 | 类型 | 说明 |
|------|------|------|
| `mem_read_exec` | 动态 | 内存读执行次数 |
| `mem_write_exec` | 动态 | 内存写执行次数 |
| `mem_inst_exec` | 动态 | 访存指令执行次数 |
| `arith_exec` | 动态 | 算术指令执行次数 |
| `logic_exec` | 动态 | 逻辑指令执行次数 |
| `float_exec` | 动态 | 浮点指令执行次数 |
| `simd_exec` | 动态 | SIMD指令执行次数 |
| `data_movement_exec` | 动态 | 数据移动指令执行次数 |
| `pure_compute_exec` | 动态 | 纯计算指令执行次数 |

---

## E类: 数据依赖特征（简化版）

### 静态指标

| 指标 | 类型 | 说明 |
|------|------|------|
| `live_in_count` | 静态 | 外部输入寄存器数（定义前使用的寄存器） |
| `live_out_count` | 静态 | 可能输出寄存器数（BBL内最后定义的寄存器） |
| `def_count` | 静态 | BBL内寄存器定义总数 |
| `use_count` | 静态 | BBL内寄存器使用总数 |
| `reg_read_static` | 静态 | 静态寄存器读操作数 |
| `reg_write_static` | 静态 | 静态寄存器写操作数 |

### 动态指标

| 指标 | 类型 | 说明 |
|------|------|------|
| `reg_read_exec` | 动态 | 寄存器读执行次数 |
| `reg_write_exec` | 动态 | 寄存器写执行次数 |
| `mem_to_reg_exec` | 动态 | 内存→寄存器传递次数（load类操作） |
| `reg_to_mem_exec` | 动态 | 寄存器→内存传递次数（store类操作） |

---

## F类: 边统计

| 指标 | 类型 | 说明 |
|------|------|------|
| `from_bbl` | - | 源BBL地址 |
| `to_bbl` | - | 目标BBL地址 |
| `exec_count` | 动态 | 边执行次数 |
| `edge_type` | 静态 | 边类型 |

**edge_type 取值**：
- `fallthrough`: 条件分支未跳转时的顺序执行
- `taken`: 条件分支跳转或无条件跳转
- `indirect`: 间接跳转（动态发现的边）
- `call`: 函数调用边
- `ret`: 函数返回边

---

## JSON输出结构

```json
{
  "tool_info": {
    "name": "BBL Profiler",
    "version": "1.0",
    "main_image": "program_name",
    "base_address": "0x400000"
  },
  "bbls": [
    {
      "bbl_addr": "0x401000",
      "bbl_offset": "0x1000",
      "function_name": "main",
      "basic_stats": {
        "inst_static": 5,
        "bbl_size_bytes": 20,
        "exec_count": 100,
        "inst_exec": 500
      },
      "control_flow": {
        "succ_count": 2,
        "is_loop_header": false,
        "is_function_entry": true,
        "is_function_exit": false,
        "terminator_type": "direct_branch",
        "has_indirect_branch": false
      },
      "compute": {
        "mem_read_static": 2,
        "mem_write_static": 1,
        "arith_static": 1,
        "mem_read_exec": 200,
        "mem_write_exec": 100,
        "arith_exec": 100,
        ...
      },
      "data_dependency": {
        "live_in_count": 3,
        "live_out_count": 2,
        "def_count": 4,
        "use_count": 6,
        "mem_to_reg_exec": 200,
        "reg_to_mem_exec": 100
      }
    }
  ],
  "edges": [
    {
      "from_bbl": "0x401000",
      "to_bbl": "0x401020",
      "exec_count": 50,
      "edge_type": "taken"
    }
  ],
  "statistics": {
    "total_bbls": 100,
    "total_edges": 150,
    "total_bbl_executions": 10000,
    "total_inst_executions": 50000
  }
}
```

---

## 使用示例

```bash
# 基本使用
pin -t bbl_profiler.so -o output.json -- ./program

# Python分析脚本示例
import json

with open('output.json') as f:
    data = json.load(f)

# 找出热点BBL（执行次数最多）
hot_bbls = sorted(data['bbls'],
                  key=lambda x: x['basic_stats']['exec_count'],
                  reverse=True)[:10]

# 找出循环头BBL
loop_headers = [bbl for bbl in data['bbls']
                if bbl['control_flow']['is_loop_header']]

# 分析热路径
hot_edges = sorted(data['edges'],
                   key=lambda x: x['exec_count'],
                   reverse=True)[:20]

# 计算访存密集度
for bbl in data['bbls']:
    mem_ratio = (bbl['compute']['mem_inst_static'] /
                 bbl['basic_stats']['inst_static'])
    bbl['mem_intensity'] = mem_ratio
```

---

*快速参考表 v1.0*
