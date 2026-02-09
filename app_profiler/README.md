# Application Profiler

基于 Intel Pin 的应用级特征剖析工具，用于分析程序的指令分布、数值敏感性、误差吸收能力和库调用特征。

## 快速开始

```bash
# 基本使用
pin -t obj-intel64/app_profiler.so -- ./your_program [args]

# 指定输出文件
pin -t obj-intel64/app_profiler.so -o result.json -- ./your_program

# 详细模式
pin -t obj-intel64/app_profiler.so -v -o result.json -- ./your_program
```

## 命令行参数

| 参数 | 默认值 | 描述 |
|------|--------|------|
| `-o <file>` | `app_profile.json` | 输出文件路径 |
| `-v` | `false` | 详细输出模式 |

## 剖析指标

### D类：指令类型分布

统计各类指令的静态数量和动态执行次数：

| 大类 | 细分 | 说明 |
|------|------|------|
| `int_arithmetic` | add, sub, mul, div | 整数算术运算 |
| `float` | - | 浮点运算（详见A类） |
| `memory` | load, store, stack | 内存访问 |
| `control_flow` | jmp, jcc, call, ret | 控制流 |
| `logic` | bitwise, shift | 逻辑/移位运算 |
| `mov` | - | 数据移动 |
| `simd` | sse, avx, avx512 | SIMD向量指令 |
| `other` | - | 其他指令 |

### A类：数值敏感性

| 指标 | 说明 |
|------|------|
| `float_inst_static/exec` | 浮点指令静态/动态数量 |
| `float_add_sub/mul/div/sqrt/fma/cmp/cvt_exec` | 各类浮点运算执行次数 |
| `single/double_precision_exec` | 单/双精度执行次数 |
| `x87_exec` | x87扩展精度执行次数 |
| `simd_float_exec` | SIMD浮点执行次数 |

> **敏感运算**：`div` 和 `sqrt` 对输入误差敏感，可能放大错误

### B类：误差吸收能力

| 指标 | 说明 |
|------|------|
| `cmp_inst_exec` | 比较指令（提供错误检测机会） |
| `test_inst_exec` | TEST指令（位测试） |
| `saturate_inst_exec` | 饱和运算（吸收溢出） |
| `minmax_inst_exec` | MIN/MAX（限制范围） |
| `abs_inst_exec` | 绝对值（消除符号错误） |
| `round_inst_exec` | 舍入指令 |

### C类：库调用统计

支持的库分类：`math`, `blas`, `lapack`, `memory`, `io`, `string`, `mpi`, `omp`, `pthread`

## 输出示例

```json
{
  "tool_info": { "name": "Application Profiler", "version": "2.0", "main_image": "test" },
  "instruction_distribution": {
    "total": { "static_count": 1234, "exec_count": 567890 },
    "by_category": {
      "int_arithmetic": { "static_count": 200, "exec_count": 50000 },
      "float": { "static_count": 100, "exec_count": 30000 },
      "memory": { "static_count": 300, "exec_count": 150000 },
      ...
    },
    "int_arithmetic_details": { "add": {...}, "sub": {...}, "mul": {...}, "div": {...} },
    "memory_details": { "load": {...}, "store": {...}, "stack": {...} },
    "control_flow_details": { "jmp": {...}, "jcc": {...}, "call": {...}, "ret": {...} },
    "logic_details": { "bitwise": {...}, "shift": {...} },
    "simd_details": { "sse": {...}, "avx": {...}, "avx512": {...} }
  },
  "numeric_sensitivity": {
    "float_inst_static": 100, "float_inst_exec": 30000,
    "operation_distribution": { "add_sub": 10000, "mul": 8000, "div": 100, ... },
    "precision_distribution": { "single": 5000, "double": 25000, ... }
  },
  "error_absorption": { "cmp_inst_exec": 5000, "test_inst_exec": 2000, ... },
  "library_calls": {
    "total_lib_calls": 500, "user_func_calls": 100,
    "by_category": { "math_lib": { "call_count": 50, "unique_funcs": 5, "functions": [...] }, ... }
  },
  "global_stats": { "total_inst_static": 1234, "total_inst_exec": 567890, "total_func_calls": 600 }
}
```

## 编译

```bash
cd /path/to/pinfi
make obj-intel64/app_profiler.so
```

## 文件结构

```
app_profiler/
├── app_profiler.cpp   # 主程序
├── app_profiler.h     # 数据结构定义
├── lib_classifier.h   # 库函数分类器
├── docs/metrics.md    # 详细指标文档
└── README.md
```

## 局限性

- 静态链接的库函数可能被识别为用户函数
- 内联函数无法被识别
- 需要符号信息以获得准确的函数名
