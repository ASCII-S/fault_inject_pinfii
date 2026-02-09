# BBL Profiler - BBL维度剖析工具

## 概述

BBL Profiler 是一个基于Intel Pin的BBL（基本块）级别执行特性剖析工具，用于学术研究中分析应用程序各BBL的执行特征，帮助研究人员分析BBL特征与软件弹性/可修复性之间的关系。

## 指标分类

| 类别 | 名称 | 说明 |
|------|------|------|
| A | 基本属性 | BBL地址、所属函数、指令数量、字节大小 |
| B | 执行统计 | 执行次数、指令执行次数 |
| C | 控制流特征 | 后继数量、循环头、函数入口/出口、终结类型 |
| D | 计算特征 | 内存/算术/逻辑/浮点/SIMD指令统计 |
| E | 数据依赖 | live_in/out、寄存器def/use、内存↔寄存器传递 |
| F | 边统计 | BBL间转移关系和执行次数 |

**命名规范**: `_static` = 静态数量, `_exec` = 动态执行次数

## 编译

```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/bbl_profiler/bbl_profiler.so
```

## 使用方法

```bash
# 基本使用
pin -t obj-intel64/bbl_profiler/bbl_profiler.so -o output.json -- ./program

# 自定义输出文件
pin -t obj-intel64/bbl_profiler/bbl_profiler.so -o bbl_analysis.json -- ./program
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-o <file>` | 输出JSON文件路径 | `bbl_profile.json` |

## 输出格式

工具输出JSON格式的BBL剖析数据，包含以下部分：

```json
{
  "tool_info": { /* 工具和程序信息 */ },
  "bbls": [
    {
      "bbl_addr": "0x401000",
      "function_name": "main",
      "basic_stats": { /* B类指标 */ },
      "control_flow": { /* C类指标 */ },
      "compute": { /* D类指标 */ },
      "data_dependency": { /* E类指标 */ }
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
  "statistics": { /* 统计摘要 */ }
}
```

## 核心功能

### 1. BBL级别细粒度分析
- 比函数维度更精细，每个BBL都是单入口单出口的代码序列
- 无内部分支，执行路径明确
- 便于精确定位故障影响范围

### 2. 控制流图边统计
- 记录BBL间的转移关系和执行频率
- 自动识别循环头（通过回边检测）
- 支持间接跳转的动态发现

### 3. 数据依赖分析（简化版）
- `live_in_count`: 外部输入寄存器数（定义前使用）
- `live_out_count`: 可能输出寄存器数（最后定义）
- 反映BBL对外部的依赖程度

### 4. 边类型分类
- `fallthrough`: 顺序执行
- `taken`: 分支跳转
- `indirect`: 间接跳转（动态发现）
- `call`: 函数调用
- `ret`: 函数返回

## 研究应用

基于BBL Profiler可以研究：

1. **RQ1**: 循环头BBL的故障影响是否显著大于普通BBL？
2. **RQ2**: 高`live_out_count`的BBL是否SDC风险更高？
3. **RQ3**: BBL执行频率与故障屏蔽率的关系？
4. **RQ4**: 不同终结类型的BBL弹性特征差异？

## 数据分析示例

```python
import json

with open('bbl_profile.json') as f:
    data = json.load(f)

# 找出高频执行的热点BBL
hot_bbls = [bbl for bbl in data['bbls']
            if bbl['basic_stats']['exec_count'] > 1000]

# 分析循环头BBL的特征
loop_headers = [bbl for bbl in data['bbls']
                if bbl['control_flow']['is_loop_header']]

# 识别热路径（高频执行的边）
hot_edges = [edge for edge in data['edges']
             if edge['exec_count'] > 500]
```

## 文档

- `docs/QUICK_REFERENCE.md` - 指标快速参考表
- `docs/METRICS_GUIDE.md` - 详细指标说明与弹性关联

## 版本

- **版本**: 1.0
- **依赖**: Intel Pin, utils.h

## 相比函数维度的优势

| 维度 | 函数维度 | BBL维度 |
|------|---------|---------|
| 粒度 | 粗（包含多个BBL） | 细（单个控制流单元） |
| 内部控制流 | 有分支/循环 | 无分支，顺序执行 |
| 故障定位精度 | 粗略定位到函数 | 精确定位到代码块 |
| CFG分析 | 调用图 | 控制流图边 |
| 热点识别 | 函数级热点 | BBL级热点（更精确） |
