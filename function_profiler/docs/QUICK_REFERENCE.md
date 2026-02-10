# 函数剖析指标快速参考表

## 命名规范

| 后缀 | 含义 | 示例 |
|------|------|------|
| `_static` | 静态分析得到的指令数量 | `inst_static`, `branch_static` |
| `_exec` | 运行时动态执行次数 | `inst_exec`, `branch_exec` |

---

## 一、执行统计 (A类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `call_exec` | 动态 | 函数调用执行次数 |
| `inst_exec` | 动态 | 总指令执行次数 |
| `inst_static` | 静态 | 静态指令数量 |

---

## 二、数据流特性 (B1类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `mem_read_exec` | 动态 | 内存读执行次数 |
| `mem_write_exec` | 动态 | 内存写执行次数 |
| `mem_inst_exec` | 动态 | 访存指令执行次数(不重复) |

---

## 三、内存访问模式 (B1.5类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `seq_read_exec` | 动态 | 连续读执行次数 |
| `stride_read_exec` | 动态 | 步长读执行次数 |
| `random_read_exec` | 动态 | 随机读执行次数 |
| `seq_write_exec` | 动态 | 连续写执行次数 |
| `stride_write_exec` | 动态 | 步长写执行次数 |
| `random_write_exec` | 动态 | 随机写执行次数 |

**分类阈值**:
- 连续访问: |stride| ≤ 64字节
- 步长访问: stride变化 ≤ 8字节
- 随机访问: 其他情况

---

## 四、计算特性 (B2类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `arith_static` | 静态 | 算术指令静态数量 |
| `logic_static` | 静态 | 逻辑指令静态数量 |
| `float_static` | 静态 | 浮点指令静态数量 |
| `simd_static` | 静态 | SIMD指令静态数量 |
| `pure_compute_static` | 静态 | 纯计算指令静态数量 |
| `data_movement_static` | 静态 | 数据移动指令静态数量 |
| `compare_static` | 静态 | 比较指令静态数量 |
| `stack_static` | 静态 | 栈操作指令静态数量 |
| `string_static` | 静态 | 字符串指令静态数量 |
| `nop_static` | 静态 | NOP指令静态数量 |
| `other_static` | 静态 | 其他指令静态数量 |
| `arith_exec` | 动态 | 算术指令执行次数 |
| `logic_exec` | 动态 | 逻辑指令执行次数 |
| `float_exec` | 动态 | 浮点指令执行次数 |
| `simd_exec` | 动态 | SIMD指令执行次数 |
| `compare_exec` | 动态 | 比较指令执行次数 |
| `stack_exec` | 动态 | 栈操作指令执行次数 |
| `string_exec` | 动态 | 字符串指令执行次数 |
| `nop_exec` | 动态 | NOP指令执行次数 |
| `other_exec` | 动态 | 其他指令执行次数 |

---

## 四点五、指令类型分布熵 (B3类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `inst_type_entropy_static` | 静态 | 静态指令类型分布熵 |
| `inst_type_entropy_exec` | 动态 | 动态指令类型分布熵 |

**熵值说明**:
- 公式：`H = -Σ(p_i * log2(p_i))`
- 熵值范围：0 ~ log2(N)，N为指令类型数
- 熵值=0：所有指令为同一类型
- 熵值越大：指令类型分布越均匀

---

## 五、控制流特性 (C类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `branch_static` | 静态 | 分支指令静态数量 |
| `branch_exec` | 动态 | 分支指令执行次数 |
| `loop_static` | 静态 | 循环静态数量 |
| `return_static` | 静态 | 返回点静态数量 |
| `call_static` | 静态 | 函数调用静态数量 |
| `call_other_exec` | 动态 | 调用其他函数执行次数 |
| `indirect_exec` | 动态 | 间接跳转执行次数 |

---

## 五点五、函数间调用图 (I类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `fan_in` | 动态 | 入度：有多少不同函数调用本函数 |
| `fan_out` | 动态 | 出度：本函数调用多少不同函数 |

**与 call_exec/call_other_exec 的区别**:
- `call_exec` / `call_other_exec` = **频率**（调用多少次）
- `fan_in` / `fan_out` = **耦合度**（与多少个不同函数有关系）

---

## 六、寄存器使用 (D类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `reg_read_exec` | 动态 | 寄存器读取执行次数 |
| `reg_write_exec` | 动态 | 寄存器写入执行次数 |
| `reg_read_static` | 静态 | 静态寄存器读操作数 |
| `reg_write_static` | 静态 | 静态寄存器写操作数 |
| `unique_reg_read` | 静态 | 使用的不同读寄存器数 |
| `unique_reg_write` | 静态 | 使用的不同写寄存器数 |

---

## 七、控制流细化 (E类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `branch_taken_exec` | 动态 | 分支跳转执行次数 |
| `branch_not_taken_exec` | 动态 | 分支未跳转执行次数 |
| `cond_branch_static` | 静态 | 条件分支静态数量 |
| `uncond_branch_static` | 静态 | 无条件跳转静态数量 |
| `loop_iter_total` | 动态 | 循环总迭代次数 |
| `call_depth_max` | 动态 | 最大调用深度 |
| `loop_depth_max` | 动态 | 最大循环嵌套深度 |

---

## 八、数据依赖 (F类) [可选]

需要 `-enable_dep` 参数启用。

| 指标 | 类型 | 说明 |
|------|------|------|
| `def_use_pairs` | 动态 | 定义-使用对总数 |
| `reg_dep_chain_max` | 动态 | 最长寄存器依赖链 |
| `mem_to_reg_exec` | 动态 | 内存→寄存器传递次数 |
| `reg_to_mem_exec` | 动态 | 寄存器→内存传递次数 |

---

## 九、生命周期 (G类) [可选]

需要 `-enable_lifetime` 参数启用。

| 指标 | 类型 | 说明 |
|------|------|------|
| `reg_lifetime_total` | 动态 | 寄存器值总存活指令数 |
| `dead_write_exec` | 动态 | 死写次数(写后未读即覆盖) |
| `first_use_dist_total` | 动态 | 定义到首次使用的总距离 |

---

## 十、圈复杂度 (H类)

| 指标 | 类型 | 说明 |
|------|------|------|
| `bbl_static` | 静态 | 静态基本块数量 (N_static) |
| `edge_static` | 静态 | 静态控制流边数量 (E_static) |
| `static_cyclomatic` | 静态 | 静态圈复杂度 = E_static - N_static + 2 |
| `bbl_exec` | 动态 | 基本块执行次数 |
| `unique_bbl_exec` | 动态 | 实际执行的唯一基本块数 (N_dynamic) |
| `unique_edge_exec` | 动态 | 实际执行的唯一边数 (E_dynamic) |
| `dynamic_cyclomatic` | 动态 | 动态圈复杂度 = E_dynamic - N_dynamic + 2 |

**说明**:
- 静态圈复杂度：基于静态分析的所有可能控制流路径
- 动态圈复杂度：基于实际执行的控制流路径
- 公式：`CC = E - N + 2`（E=边数，N=节点数）
- 最小值为1（顺序执行无分支）
- `dynamic_cyclomatic <= static_cyclomatic`（动态覆盖的路径 ≤ 静态所有路径）

---

## 十一、JSON输出结构

```json
{
  "function_name": "example",
  "execution_stats": {
    "call_exec": 10,
    "inst_exec": 1000,
    "inst_static": 50
  },
  "data_flow": {
    "mem_read_exec": 200,
    "mem_write_exec": 100,
    "mem_inst_exec": 250
  },
  "memory_access_pattern": {
    "seq_read_exec": 180,
    "stride_read_exec": 15,
    "random_read_exec": 5,
    "seq_write_exec": 90,
    "stride_write_exec": 8,
    "random_write_exec": 2
  },
  "compute_characteristics": {
    "arith_static": 10,
    "logic_static": 5,
    "float_static": 0,
    "simd_static": 0,
    "pure_compute_static": 12,
    "data_movement_static": 15,
    "compare_static": 4,
    "stack_static": 6,
    "string_static": 0,
    "nop_static": 1,
    "other_static": 2,
    "arith_exec": 100,
    "logic_exec": 50,
    "float_exec": 0,
    "simd_exec": 0,
    "compare_exec": 40,
    "stack_exec": 60,
    "string_exec": 0,
    "nop_exec": 10,
    "other_exec": 20
  },
  "instruction_entropy": {
    "inst_type_entropy_static": 2.8745,
    "inst_type_entropy_exec": 2.5632
  },
  "control_flow": {
    "branch_static": 8,
    "branch_exec": 50,
    "loop_static": 2,
    "return_static": 1,
    "call_static": 3,
    "call_other_exec": 25,
    "indirect_exec": 10
  },
  "call_graph": {
    "fan_in": 3,
    "fan_out": 5
  },
  "register_usage": {
    "reg_read_exec": 500,
    "reg_write_exec": 300,
    "reg_read_static": 80,
    "reg_write_static": 50,
    "unique_reg_read": 12,
    "unique_reg_write": 10
  },
  "control_flow_detail": {
    "branch_taken_exec": 30,
    "branch_not_taken_exec": 20,
    "cond_branch_static": 6,
    "uncond_branch_static": 2,
    "loop_iter_total": 100,
    "call_depth_max": 5,
    "loop_depth_max": 3
  },
  "data_dependency": {
    "def_use_pairs": 450,
    "reg_dep_chain_max": 15,
    "mem_to_reg_exec": 200,
    "reg_to_mem_exec": 100
  },
  "lifetime": {
    "reg_lifetime_total": 2500,
    "dead_write_exec": 10,
    "first_use_dist_total": 300
  },
  "cyclomatic_complexity": {
    "bbl_static": 12,
    "edge_static": 15,
    "static_cyclomatic": 5,
    "bbl_exec": 150,
    "unique_bbl_exec": 10,
    "unique_edge_exec": 14,
    "dynamic_cyclomatic": 6
  }
}
```

---

## 十二、使用示例

```bash
# 基本使用
pin -t function_profiler.so -o output.json -- ./program

# 启用所有可选指标
pin -t function_profiler.so -o output.json -enable_dep -enable_lifetime -- ./program

# 过滤低调用次数函数
pin -t function_profiler.so -o output.json -min_calls 5 -- ./program
```

```python
import json

with open('output.json') as f:
    data = json.load(f)

for func in data['functions']:
    name = func['function_name']
    inst_exec = func['execution_stats']['inst_exec']
    cc = func['cyclomatic_complexity']
    dynamic_cc = cc['dynamic_cyclomatic']
    unique_bbl = cc['unique_bbl_exec']
    print(f"{name}: 指令={inst_exec}, 动态圈复杂度={dynamic_cc}, 执行BBL数={unique_bbl}")
```

---

*快速参考表 v3.1*
