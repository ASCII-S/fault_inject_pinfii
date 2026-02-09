# App Profiler 指标文档

## 概述

`app_profiler` 是一个基于 Intel Pin 的应用维度算法特性剖析工具，用于分析应用程序的算法特性，为故障注入实验提供指导。

---

## 指标分类

### D类：指令类型分布指标

用于分析程序中各类指令的静态数量和动态执行分布。

#### 指令大类分布

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 整数算术指令静态数量 | `int_arith_inst_static` | UINT32 | 整数算术指令的静态数量 |
| 整数算术指令执行次数 | `int_arith_inst_exec` | UINT64 | 整数算术指令的动态执行次数 |
| 浮点指令静态数量 | `float_inst_static` | UINT32 | 浮点指令的静态数量 |
| 浮点指令执行次数 | `float_inst_exec` | UINT64 | 浮点指令的动态执行次数 |
| 内存访问指令静态数量 | `mem_inst_static` | UINT32 | 内存访问指令的静态数量 |
| 内存访问指令执行次数 | `mem_inst_exec` | UINT64 | 内存访问指令的动态执行次数 |
| 控制流指令静态数量 | `ctrl_inst_static` | UINT32 | 控制流指令的静态数量 |
| 控制流指令执行次数 | `ctrl_inst_exec` | UINT64 | 控制流指令的动态执行次数 |
| 逻辑运算指令静态数量 | `logic_inst_static` | UINT32 | 逻辑运算指令的静态数量 |
| 逻辑运算指令执行次数 | `logic_inst_exec` | UINT64 | 逻辑运算指令的动态执行次数 |
| 数据移动指令静态数量 | `mov_inst_static` | UINT32 | 数据移动指令的静态数量 |
| 数据移动指令执行次数 | `mov_inst_exec` | UINT64 | 数据移动指令的动态执行次数 |
| SIMD指令静态数量 | `simd_inst_static` | UINT32 | SIMD向量指令的静态数量 |
| SIMD指令执行次数 | `simd_inst_exec` | UINT64 | SIMD向量指令的动态执行次数 |
| 其他指令静态数量 | `other_inst_static` | UINT32 | 其他指令的静态数量 |
| 其他指令执行次数 | `other_inst_exec` | UINT64 | 其他指令的动态执行次数 |

#### 指令类型枚举 (`InstCategory`)

```cpp
enum InstCategory {
    INST_INT_ARITH,      // 整数算术运算 (ADD, SUB, MUL, DIV, INC, DEC, NEG, IMUL, IDIV)
    INST_FLOAT,          // 浮点运算 (所有浮点指令)
    INST_MEMORY,         // 内存访问 (LOAD, STORE, LEA, PUSH, POP)
    INST_CONTROL,        // 控制流 (JMP, Jcc, CALL, RET, LOOP)
    INST_LOGIC,          // 逻辑运算 (AND, OR, XOR, NOT, SHL, SHR, SAR, ROL, ROR)
    INST_MOV,            // 数据移动 (MOV, MOVSX, MOVZX, XCHG, CMOVcc)
    INST_SIMD,           // SIMD向量 (SSE, AVX, AVX2, AVX-512)
    INST_OTHER,          // 其他指令 (NOP, CPUID, RDTSC, etc.)
    INST_CATEGORY_COUNT
};
```

#### 整数算术指令细分

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 整数加法静态数量 | `int_add_static` | UINT32 | ADD/INC 指令静态数量 |
| 整数加法执行次数 | `int_add_exec` | UINT64 | ADD/INC 指令执行次数 |
| 整数减法静态数量 | `int_sub_static` | UINT32 | SUB/DEC/NEG 指令静态数量 |
| 整数减法执行次数 | `int_sub_exec` | UINT64 | SUB/DEC/NEG 指令执行次数 |
| 整数乘法静态数量 | `int_mul_static` | UINT32 | MUL/IMUL 指令静态数量 |
| 整数乘法执行次数 | `int_mul_exec` | UINT64 | MUL/IMUL 指令执行次数 |
| 整数除法静态数量 | `int_div_static` | UINT32 | DIV/IDIV 指令静态数量 |
| 整数除法执行次数 | `int_div_exec` | UINT64 | DIV/IDIV 指令执行次数 |

#### 内存访问指令细分

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 内存读取静态数量 | `mem_load_static` | UINT32 | LOAD 指令静态数量 |
| 内存读取执行次数 | `mem_load_exec` | UINT64 | LOAD 指令执行次数 |
| 内存写入静态数量 | `mem_store_static` | UINT32 | STORE 指令静态数量 |
| 内存写入执行次数 | `mem_store_exec` | UINT64 | STORE 指令执行次数 |
| 栈操作静态数量 | `stack_op_static` | UINT32 | PUSH/POP 指令静态数量 |
| 栈操作执行次数 | `stack_op_exec` | UINT64 | PUSH/POP 指令执行次数 |

#### 控制流指令细分

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 无条件跳转静态数量 | `jmp_static` | UINT32 | JMP 指令静态数量 |
| 无条件跳转执行次数 | `jmp_exec` | UINT64 | JMP 指令执行次数 |
| 条件跳转静态数量 | `jcc_static` | UINT32 | Jcc 条件跳转指令静态数量 |
| 条件跳转执行次数 | `jcc_exec` | UINT64 | Jcc 条件跳转指令执行次数 |
| 函数调用静态数量 | `call_static` | UINT32 | CALL 指令静态数量 |
| 函数调用执行次数 | `call_exec` | UINT64 | CALL 指令执行次数 |
| 函数返回静态数量 | `ret_static` | UINT32 | RET 指令静态数量 |
| 函数返回执行次数 | `ret_exec` | UINT64 | RET 指令执行次数 |

#### 逻辑运算指令细分

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 位运算静态数量 | `bitwise_static` | UINT32 | AND/OR/XOR/NOT 指令静态数量 |
| 位运算执行次数 | `bitwise_exec` | UINT64 | AND/OR/XOR/NOT 指令执行次数 |
| 移位运算静态数量 | `shift_static` | UINT32 | SHL/SHR/SAR/ROL/ROR 指令静态数量 |
| 移位运算执行次数 | `shift_exec` | UINT64 | SHL/SHR/SAR/ROL/ROR 指令执行次数 |

#### SIMD指令细分

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| SSE指令静态数量 | `sse_inst_static` | UINT32 | SSE 指令静态数量 |
| SSE指令执行次数 | `sse_inst_exec` | UINT64 | SSE 指令执行次数 |
| AVX指令静态数量 | `avx_inst_static` | UINT32 | AVX/AVX2 指令静态数量 |
| AVX指令执行次数 | `avx_inst_exec` | UINT64 | AVX/AVX2 指令执行次数 |
| AVX-512指令静态数量 | `avx512_inst_static` | UINT32 | AVX-512 指令静态数量 |
| AVX-512指令执行次数 | `avx512_inst_exec` | UINT64 | AVX-512 指令执行次数 |

---

### A类：数值敏感性指标

用于评估应用程序对数值精度的敏感程度。

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 浮点指令静态数量 | `float_inst_static` | UINT32 | 程序中浮点指令的静态数量（去重） |
| 浮点指令执行次数 | `float_inst_exec` | UINT64 | 浮点指令的动态执行总次数 |
| 加减运算执行次数 | `float_add_sub_exec` | UINT64 | 浮点加法/减法指令执行次数 |
| 乘法运算执行次数 | `float_mul_exec` | UINT64 | 浮点乘法指令执行次数 |
| 除法运算执行次数 | `float_div_exec` | UINT64 | 浮点除法指令执行次数（**敏感运算**） |
| 开方运算执行次数 | `float_sqrt_exec` | UINT64 | 浮点开方指令执行次数（**敏感运算**） |
| FMA运算执行次数 | `float_fma_exec` | UINT64 | 融合乘加指令执行次数 |
| 浮点比较执行次数 | `float_cmp_exec` | UINT64 | 浮点比较指令执行次数 |
| 精度转换执行次数 | `float_cvt_exec` | UINT64 | 浮点精度转换指令执行次数 |
| 单精度执行次数 | `single_precision_exec` | UINT64 | 单精度(float)浮点指令执行次数 |
| 双精度执行次数 | `double_precision_exec` | UINT64 | 双精度(double)浮点指令执行次数 |
| x87扩展精度执行次数 | `x87_exec` | UINT64 | x87扩展精度浮点指令执行次数 |
| SIMD浮点执行次数 | `simd_float_exec` | UINT64 | SIMD向量浮点指令执行次数 |

#### 浮点运算类型枚举 (`FloatOpType`)

```cpp
enum FloatOpType {
    FLOAT_ADD_SUB,   // 加减运算
    FLOAT_MUL,       // 乘法运算
    FLOAT_DIV,       // 除法运算（敏感运算）
    FLOAT_SQRT,      // 开方运算（敏感运算）
    FLOAT_FMA,       // 融合乘加
    FLOAT_CMP,       // 浮点比较
    FLOAT_CVT,       // 精度转换
    FLOAT_OTHER      // 其他浮点运算
};
```

#### 浮点精度类型枚举 (`FloatPrecision`)

```cpp
enum FloatPrecision {
    PREC_SINGLE,     // 单精度 (float, SS/PS后缀)
    PREC_DOUBLE,     // 双精度 (double, SD/PD后缀)
    PREC_EXTENDED,   // 扩展精度 (x87)
    PREC_UNKNOWN     // 未知精度
};
```

---

### B类：误差吸收能力指标

用于评估应用程序对误差的容忍和吸收能力。

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 比较指令执行次数 | `cmp_inst_exec` | UINT64 | CMP指令执行次数 |
| TEST指令执行次数 | `test_inst_exec` | UINT64 | TEST指令执行次数 |
| 饱和运算执行次数 | `saturate_inst_exec` | UINT64 | 饱和运算指令执行次数 |
| MIN/MAX指令执行次数 | `minmax_inst_exec` | UINT64 | 最小值/最大值指令执行次数 |
| 绝对值指令执行次数 | `abs_inst_exec` | UINT64 | 绝对值指令执行次数 |
| 舍入指令执行次数 | `round_inst_exec` | UINT64 | 舍入指令执行次数 |

---

### C类：外部库调用指标

用于分析应用程序对外部库的依赖情况。

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 外部库调用总次数 | `total_lib_calls` | UINT64 | 所有外部库函数调用总次数 |
| 用户函数调用次数 | `user_func_calls` | UINT64 | 用户自定义函数调用次数 |
| 分类统计 | `lib_stats[LIB_CATEGORY_COUNT]` | LibCallStats[] | 各类库的调用统计 |

#### 库函数分类枚举 (`LibCategory`)

| 枚举值 | 名称 | 描述 | 示例函数 |
|--------|------|------|----------|
| `LIB_MATH` | math_lib | 数学库 | sin, cos, exp, log, pow, sqrt |
| `LIB_BLAS` | blas_lib | BLAS库 | dgemm, daxpy, ddot |
| `LIB_LAPACK` | lapack_lib | LAPACK库 | dgetrf, dgetrs, dpotrf |
| `LIB_MEMORY` | memory_lib | 内存管理 | malloc, free, memcpy, memset |
| `LIB_IO` | io_lib | IO库 | printf, scanf, fopen, fread |
| `LIB_STRING` | string_lib | 字符串库 | strlen, strcpy, strcmp |
| `LIB_MPI` | mpi_lib | MPI库 | MPI_Send, MPI_Recv, MPI_Bcast |
| `LIB_OMP` | omp_lib | OpenMP库 | omp_get_thread_num |
| `LIB_PTHREAD` | pthread_lib | Pthread库 | pthread_create, pthread_mutex |
| `LIB_OTHER` | other_lib | 其他库函数 | - |
| `LIB_USER` | user_func | 用户函数 | 非库函数 |

#### 库调用统计结构 (`LibCallStats`)

```cpp
struct LibCallStats {
    UINT64 call_count;           // 调用次数
    set<string> unique_funcs;    // 不同函数集合
};
```

---

### 全局统计指标

| 指标名称 | 变量名 | 类型 | 描述 |
|---------|--------|------|------|
| 总指令执行次数 | `total_inst_exec` | UINT64 | 程序执行的总指令数 |
| 总指令静态数量 | `total_inst_static` | UINT32 | 程序中指令的静态数量 |
| 总函数调用次数 | `total_func_calls` | UINT64 | 程序执行的总函数调用数 |
| 主程序名称 | `main_image_name` | string | 被分析程序的名称 |

---

## 派生指标（可计算）

基于原始指标可以计算以下派生指标：

### 指令分布派生指标

| 派生指标 | 计算公式 | 意义 |
|---------|---------|------|
| 整数算术指令占比 | `int_arith_inst_exec / total_inst_exec` | 整数计算密集度 |
| 浮点指令占比 | `float_inst_exec / total_inst_exec` | 浮点计算密集度 |
| 内存访问指令占比 | `mem_inst_exec / total_inst_exec` | 内存访问密集度 |
| 控制流指令占比 | `ctrl_inst_exec / total_inst_exec` | 控制流复杂度 |
| SIMD指令占比 | `simd_inst_exec / total_inst_exec` | 向量化程度 |
| 指令动态/静态比 | `total_inst_exec / total_inst_static` | 平均指令执行频率 |

### 数值敏感性派生指标

| 派生指标 | 计算公式 | 意义 |
|---------|---------|------|
| 敏感运算占比 | `(float_div_exec + float_sqrt_exec) / float_inst_exec` | 敏感运算在浮点运算中的比例 |
| 单精度占比 | `single_precision_exec / float_inst_exec` | 单精度运算比例 |
| 双精度占比 | `double_precision_exec / float_inst_exec` | 双精度运算比例 |

### 库调用派生指标

| 派生指标 | 计算公式 | 意义 |
|---------|---------|------|
| 库调用密度 | `total_lib_calls / total_inst_exec` | 每条指令的平均库调用频率 |
| 数学库调用占比 | `lib_stats[LIB_MATH].call_count / total_lib_calls` | 数学库调用比例 |

---

## 数据结构

### 指令分布统计结构 (`InstDistribution`)

```cpp
struct InstDistribution {
    UINT32 static_count;     // 静态数量
    UINT64 exec_count;       // 动态执行次数

    InstDistribution() : static_count(0), exec_count(0) {}
};
```

### 镜像信息 (`ImageInfo`)

用于判断地址是否属于库。

```cpp
struct ImageInfo {
    ADDRINT low_addr;            // 起始地址
    ADDRINT high_addr;           // 结束地址
    string name;                 // 镜像名称
    bool is_main_executable;     // 是否为主程序
};
```

### 应用剖析数据 (`AppProfile`)

包含所有剖析指标的主数据结构，详见上述各类指标。

---

## 输出格式

剖析结果以 JSON 格式输出到 `app_profile.json`（默认），包含以下结构：

```json
{
  "tool_info": {
    "name": "Application Profiler",
    "version": "2.0",
    "main_image": "程序名称"
  },

  "instruction_distribution": {
    "total": {
      "static_count": 静态数量,
      "exec_count": 执行次数
    },
    "by_category": {
      "int_arithmetic": { "static_count": 数量, "exec_count": 次数 },
      "float": { "static_count": 数量, "exec_count": 次数 },
      "memory": { "static_count": 数量, "exec_count": 次数 },
      "control_flow": { "static_count": 数量, "exec_count": 次数 },
      "logic": { "static_count": 数量, "exec_count": 次数 },
      "mov": { "static_count": 数量, "exec_count": 次数 },
      "simd": { "static_count": 数量, "exec_count": 次数 },
      "other": { "static_count": 数量, "exec_count": 次数 }
    },
    "int_arithmetic_details": {
      "add": { "static_count": 数量, "exec_count": 次数 },
      "sub": { "static_count": 数量, "exec_count": 次数 },
      "mul": { "static_count": 数量, "exec_count": 次数 },
      "div": { "static_count": 数量, "exec_count": 次数 }
    },
    "memory_details": {
      "load": { "static_count": 数量, "exec_count": 次数 },
      "store": { "static_count": 数量, "exec_count": 次数 },
      "stack": { "static_count": 数量, "exec_count": 次数 }
    },
    "control_flow_details": {
      "jmp": { "static_count": 数量, "exec_count": 次数 },
      "jcc": { "static_count": 数量, "exec_count": 次数 },
      "call": { "static_count": 数量, "exec_count": 次数 },
      "ret": { "static_count": 数量, "exec_count": 次数 }
    },
    "logic_details": {
      "bitwise": { "static_count": 数量, "exec_count": 次数 },
      "shift": { "static_count": 数量, "exec_count": 次数 }
    },
    "simd_details": {
      "sse": { "static_count": 数量, "exec_count": 次数 },
      "avx": { "static_count": 数量, "exec_count": 次数 },
      "avx512": { "static_count": 数量, "exec_count": 次数 }
    }
  },

  "numeric_sensitivity": {
    "float_inst_static": 静态数量,
    "float_inst_exec": 执行次数,
    "operation_distribution": {
      "add_sub": 加减次数,
      "mul": 乘法次数,
      "div": 除法次数,
      "sqrt": 开方次数,
      "fma": FMA次数,
      "cmp": 比较次数,
      "cvt": 转换次数
    },
    "precision_distribution": {
      "single": 单精度次数,
      "double": 双精度次数,
      "x87": x87次数,
      "simd": SIMD次数
    }
  },

  "error_absorption": {
    "cmp_inst_exec": 比较指令次数,
    "test_inst_exec": TEST指令次数,
    "saturate_inst_exec": 饱和运算次数,
    "minmax_inst_exec": MIN/MAX次数,
    "abs_inst_exec": 绝对值次数,
    "round_inst_exec": 舍入次数
  },

  "library_calls": {
    "total_lib_calls": 库调用总次数,
    "user_func_calls": 用户函数调用次数,
    "by_category": {
      "math_lib": {
        "call_count": 次数,
        "unique_funcs": 不同函数数量,
        "functions": ["函数列表"]
      },
      ...
    }
  },

  "global_stats": {
    "total_inst_static": 总指令静态数量,
    "total_inst_exec": 总指令执行次数,
    "total_func_calls": 总函数调用次数
  }
}
```

---

## 使用场景

1. **故障注入目标选择**：通过指令类型分布确定注入目标类型
2. **敏感性评估**：通过敏感运算占比评估程序对精度误差的敏感程度
3. **容错能力分析**：通过误差吸收指标评估程序的容错能力
4. **库依赖分析**：了解程序对外部库的依赖情况
5. **性能特征分析**：通过指令分布了解程序的计算特征（计算密集/内存密集/控制流密集）
6. **向量化评估**：通过SIMD指令占比评估程序的向量化程度
