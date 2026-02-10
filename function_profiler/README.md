# Function Profiler - 函数维度剖析工具

## 概述

Function Profiler 是一个基于Intel Pin的函数级别执行特性剖析工具，用于学术研究中分析应用程序各函数的执行特征，帮助研究人员分析函数特征与软件弹性之间的关系。

## 指标分类

| 类别 | 名称 | 说明 |
|------|------|------|
| A | 执行统计 | 调用次数、指令执行次数 |
| B1 | 数据流 | 内存读写次数 |
| B1.5 | 内存访问模式 | 连续/步长/随机访问 |
| B2 | 计算特性 | 算术/逻辑/浮点/SIMD/比较/栈/字符串指令 |
| B3 | 指令类型熵 | 指令类型分布熵 |
| C | 控制流 | 分支/循环/调用 |
| D | 寄存器使用 | 寄存器读写统计 |
| E | 控制流细化 | 分支方向/循环迭代/调用深度/循环嵌套深度 |
| F | 数据依赖 | 定义-使用对 [可选] |
| G | 生命周期 | 寄存器存活/死写 [可选] |
| H | 动态圈复杂度 | 基本块/边/动态圈复杂度 |

**命名规范**: `_static` = 静态数量, `_exec` = 动态执行次数

## 编译

```bash
make obj-intel64/function_profiler/function_profiler.so
```

## 使用方法

```bash
# 基本使用
pin -t function_profiler.so -o output.json -- ./program

# 启用可选指标（F类/G类，有性能开销）
pin -t function_profiler.so -enable_dep -enable_lifetime -o output.json -- ./program

# 过滤低调用次数函数
pin -t function_profiler.so -min_calls 5 -o output.json -- ./program
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-o <file>` | 输出JSON文件路径 | `function_profile.json` |
| `-min_calls <n>` | 最小调用次数过滤 | `1` |
| `-enable_dep` | 启用F类数据依赖分析 | 关闭 |
| `-enable_lifetime` | 启用G类生命周期分析 | 关闭 |

## 文档

- `docs/QUICK_REFERENCE.md` - 指标快速参考表
- `docs/METRICS_GUIDE.md` - 详细指标说明与弹性关联

## 版本

- **版本**: 3.0
- **依赖**: Intel Pin, utils.h
