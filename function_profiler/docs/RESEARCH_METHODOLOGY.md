# 基于函数特征的软件弹性研究方法论

## 一、研究背景

### 1.1 软件弹性定义

软件弹性(Software Resilience)指软件系统在面对故障时维持正确功能的能力。在硬件瞬态故障(Soft Error)场景下，弹性主要体现为：

| 故障结果 | 定义 | 弹性评价 |
|---------|------|---------|
| **Masked** | 故障被自然屏蔽，输出正确 | 最佳弹性 |
| **Detected** | 故障被检测到，程序报错/终止 | 良好弹性 |
| **Crash** | 程序崩溃(段错误等) | 安全失败 |
| **SDC** | 静默数据损坏，输出错误但未检测 | 最差弹性 |
| **Hang** | 程序挂起，无响应 | 较差弹性 |

### 1.2 研究动机

传统的弹性分析方法：
- **随机故障注入**: 覆盖率低，效率差
- **全量故障注入**: 时间成本高，不可扩展
- **静态分析**: 缺乏运行时信息

**本研究目标**: 通过函数级特征剖析，建立特征与弹性的关联模型，实现：
1. 弹性预测：基于特征预测函数的弹性行为
2. 选择性注入：优先测试高风险函数
3. 加固指导：识别需要加固的脆弱函数

---

## 二、研究方法框架

### 2.1 整体流程

```
┌─────────────────────────────────────────────────────────────────┐
│                      研究方法框架                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Phase 1: 特征收集                                              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐         │
│  │ 目标程序    │ → │ Function    │ → │ 函数特征    │         │
│  │ (Benchmark) │    │ Profiler    │    │ JSON数据    │         │
│  └─────────────┘    └─────────────┘    └─────────────┘         │
│                                                                 │
│  Phase 2: 故障注入                                              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐         │
│  │ 函数特征    │ → │ 故障注入    │ → │ 故障结果    │         │
│  │ (PC范围)    │    │ 工具        │    │ 数据集      │         │
│  └─────────────┘    └─────────────┘    └─────────────┘         │
│                                                                 │
│  Phase 3: 关联分析                                              │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐         │
│  │ 特征+结果   │ → │ 统计/ML    │ → │ 弹性模型    │         │
│  │ 数据集      │    │ 分析        │    │             │         │
│  └─────────────┘    └─────────────┘    └─────────────┘         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 数据收集流程

#### Step 1: 运行Function Profiler
```bash
# 对每个benchmark程序运行剖析
for prog in benchmarks/*; do
    pin -t function_profiler.so -o profiles/${prog}.json -- $prog
done
```

#### Step 2: 故障注入实验
```bash
# 对每个函数进行故障注入
for func in $(get_functions profiles/${prog}.json); do
    for trial in $(seq 1 1000); do
        inject_fault --function $func --trial $trial
        record_result $func $trial
    done
done
```

#### Step 3: 数据整合
```python
# 合并特征和故障结果
merged_data = []
for func in functions:
    features = get_features(func)
    results = get_fault_results(func)
    merged_data.append({
        'function': func,
        'features': features,
        'masked_rate': results['masked'] / results['total'],
        'crash_rate': results['crash'] / results['total'],
        'sdc_rate': results['sdc'] / results['total']
    })
```

---

## 三、特征工程

### 3.1 原始特征

Function Profiler提供的26个原始特征：

| 类别 | 特征 | 数量 |
|------|------|------|
| 执行统计 | call_count, total_instructions, static_inst_count | 3 |
| 数据流 | memory_reads, memory_writes, memory_access_ratio | 3 |
| 计算特性 | arithmetic_insts, logic_insts, float_insts, simd_insts, compute_ratio, float_ratio, arithmetic_exec_count, float_exec_count | 8 |
| 控制流 | branch_count, branch_density, loop_count, return_points, calls_to_functions, indirect_branch_count, indirect_branch_ratio, branch_exec_count | 8 |
| 故障敏感性 | crashprone_insts, crashprone_ratio, check_inst_count, check_inst_ratio, crashprone_exec_count | 5 |

### 3.2 派生特征（建议）

基于原始特征可构造更多派生特征：

```python
# 复杂度特征
cyclomatic_complexity = branch_count + 1
instruction_density = total_instructions / call_count

# 比例特征
read_write_ratio = memory_reads / (memory_writes + 1)
compute_memory_ratio = compute_ratio / (memory_access_ratio + 0.01)

# 风险特征
sdc_risk_score = compute_ratio * (1 - crashprone_ratio)
detection_capability = check_inst_ratio + crashprone_ratio

# 归一化特征
normalized_size = static_inst_count / max_static_inst_count
```

### 3.3 特征选择

推荐的特征选择方法：

1. **相关性过滤**: 移除与目标变量相关性<0.1的特征
2. **方差过滤**: 移除方差接近0的特征
3. **共线性处理**: 移除高度相关(>0.9)的冗余特征
4. **重要性排序**: 使用随机森林特征重要性

```python
from sklearn.feature_selection import SelectKBest, f_classif
from sklearn.ensemble import RandomForestClassifier

# 方法1: 统计检验
selector = SelectKBest(f_classif, k=10)
X_selected = selector.fit_transform(X, y)

# 方法2: 随机森林重要性
rf = RandomForestClassifier(n_estimators=100)
rf.fit(X, y)
importances = rf.feature_importances_
```

---

## 四、分析方法

### 4.1 描述性统计

```python
import pandas as pd
import seaborn as sns

# 基本统计
df.describe()

# 按弹性结果分组统计
df.groupby('result_type').mean()

# 相关性热力图
correlation_matrix = df.corr()
sns.heatmap(correlation_matrix, annot=True)
```

### 4.2 假设检验

**研究假设示例**:
- H1: check_inst_ratio高的函数具有更高的故障检测率
- H2: compute_ratio高的函数具有更高的SDC率
- H3: crashprone_ratio高的函数具有更高的崩溃率

```python
from scipy import stats

# 独立样本t检验
high_check = df[df['check_inst_ratio'] > median]['detection_rate']
low_check = df[df['check_inst_ratio'] <= median]['detection_rate']
t_stat, p_value = stats.ttest_ind(high_check, low_check)

# 相关性检验
r, p = stats.pearsonr(df['compute_ratio'], df['sdc_rate'])
print(f"Pearson r={r:.3f}, p={p:.4f}")
```

### 4.3 回归分析

```python
import statsmodels.api as sm

# 多元线性回归
X = df[['compute_ratio', 'memory_access_ratio', 'check_inst_ratio',
        'crashprone_ratio', 'branch_density']]
X = sm.add_constant(X)
y = df['masked_rate']

model = sm.OLS(y, X).fit()
print(model.summary())
```

### 4.4 机器学习建模

```python
from sklearn.model_selection import cross_val_score
from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
from sklearn.metrics import classification_report

# 分类任务：预测故障结果类型
X = df[feature_columns]
y = df['result_type']  # Masked, Crash, SDC

# 随机森林
rf = RandomForestClassifier(n_estimators=100, random_state=42)
scores = cross_val_score(rf, X, y, cv=5, scoring='f1_macro')
print(f"RF F1-score: {scores.mean():.3f} ± {scores.std():.3f}")

# 梯度提升
gb = GradientBoostingClassifier(n_estimators=100, random_state=42)
scores = cross_val_score(gb, X, y, cv=5, scoring='f1_macro')
print(f"GB F1-score: {scores.mean():.3f} ± {scores.std():.3f}")
```

---

## 五、实验设计

### 5.1 Benchmark选择

推荐的benchmark套件：

| 套件 | 特点 | 适用场景 |
|------|------|---------|
| SPEC CPU | 计算密集 | 通用弹性分析 |
| PARSEC | 并行程序 | 多线程弹性 |
| MiBench | 嵌入式 | 嵌入式系统弹性 |
| Rodinia | GPU/异构 | 异构计算弹性 |

### 5.2 故障模型

常用的故障模型：

| 模型 | 描述 | 实现方式 |
|------|------|---------|
| 单比特翻转 | 翻转一个随机比特 | XOR 1 << random_bit |
| 多比特翻转 | 翻转多个比特 | XOR random_mask |
| 寄存器故障 | 修改寄存器值 | Pin寄存器修改 |
| 内存故障 | 修改内存值 | Pin内存修改 |

### 5.3 采样策略

```python
# 分层采样：确保各类函数都有足够样本
from sklearn.model_selection import StratifiedKFold

# 按函数类型分层
df['func_type'] = pd.cut(df['compute_ratio'],
                         bins=[0, 0.1, 0.3, 1.0],
                         labels=['low', 'medium', 'high'])

skf = StratifiedKFold(n_splits=5, shuffle=True, random_state=42)
for train_idx, test_idx in skf.split(X, df['func_type']):
    # 训练和测试
    pass
```

### 5.4 统计显著性

确保结果的统计显著性：

```python
# 每个函数至少1000次故障注入
MIN_TRIALS = 1000

# 置信区间计算
import numpy as np
from scipy import stats

def confidence_interval(data, confidence=0.95):
    n = len(data)
    mean = np.mean(data)
    se = stats.sem(data)
    h = se * stats.t.ppf((1 + confidence) / 2, n - 1)
    return mean, mean - h, mean + h
```

---

## 六、结果呈现

### 6.1 可视化建议

```python
import matplotlib.pyplot as plt
import seaborn as sns

# 1. 特征分布
fig, axes = plt.subplots(2, 3, figsize=(15, 10))
for ax, feature in zip(axes.flat, key_features):
    sns.histplot(df[feature], ax=ax, kde=True)
    ax.set_title(feature)

# 2. 特征与弹性关系
fig, axes = plt.subplots(1, 3, figsize=(15, 5))
for ax, outcome in zip(axes, ['masked_rate', 'crash_rate', 'sdc_rate']):
    sns.scatterplot(data=df, x='compute_ratio', y=outcome, ax=ax)
    ax.set_title(f'Compute Ratio vs {outcome}')

# 3. 混淆矩阵
from sklearn.metrics import confusion_matrix, ConfusionMatrixDisplay
cm = confusion_matrix(y_true, y_pred)
disp = ConfusionMatrixDisplay(cm, display_labels=['Masked', 'Crash', 'SDC'])
disp.plot()

# 4. 特征重要性
importances = rf.feature_importances_
indices = np.argsort(importances)[::-1]
plt.barh(range(len(indices)), importances[indices])
plt.yticks(range(len(indices)), [feature_names[i] for i in indices])
```

### 6.2 论文表格模板

**表1: 函数特征与弹性结果的相关性**
| 特征 | Masked Rate | Crash Rate | SDC Rate |
|------|-------------|------------|----------|
| compute_ratio | -0.23* | -0.15 | 0.31** |
| memory_access_ratio | 0.18 | 0.42** | -0.28* |
| check_inst_ratio | 0.45** | 0.12 | -0.38** |
| crashprone_ratio | -0.21* | 0.56** | -0.31** |

*p<0.05, **p<0.01

**表2: 弹性预测模型性能**
| 模型 | Accuracy | Precision | Recall | F1-Score |
|------|----------|-----------|--------|----------|
| Logistic Regression | 0.72 | 0.70 | 0.71 | 0.70 |
| Random Forest | 0.81 | 0.79 | 0.80 | 0.79 |
| Gradient Boosting | 0.83 | 0.82 | 0.81 | 0.81 |
| Neural Network | 0.80 | 0.78 | 0.79 | 0.78 |

---

## 七、研究问题示例

### RQ1: 特征相关性
**问题**: 哪些函数特征与故障屏蔽成功率显著相关？

**方法**: Pearson/Spearman相关性分析 + 多元回归

**预期发现**:
- check_inst_ratio与masked_rate正相关
- compute_ratio与sdc_rate正相关

### RQ2: 函数分类
**问题**: 能否基于特征将函数分为不同的弹性类别？

**方法**: K-means聚类 + 聚类特征分析

**预期发现**:
- 计算密集型函数聚为一类，SDC风险高
- 访存密集型函数聚为一类，崩溃风险高

### RQ3: 弹性预测
**问题**: 能否基于静态特征预测函数的弹性行为？

**方法**: 机器学习分类模型 + 交叉验证

**预期发现**:
- 随机森林模型F1-score > 0.75
- 最重要特征：crashprone_ratio, check_inst_ratio

### RQ4: 加固指导
**问题**: 如何基于特征识别需要加固的函数？

**方法**: 风险评分模型 + 阈值分析

**预期发现**:
- 高compute_ratio + 低check_inst_ratio的函数需要优先加固
- 提出基于特征的加固优先级排序算法

---

## 八、工具链集成

### 8.1 完整实验脚本

```bash
#!/bin/bash
# run_experiment.sh

BENCHMARK_DIR="./benchmarks"
OUTPUT_DIR="./results"
PROFILER="function_profiler.so"
INJECTOR="fault_injector.so"

# Step 1: Profile all benchmarks
for prog in $BENCHMARK_DIR/*; do
    name=$(basename $prog)
    echo "Profiling $name..."
    pin -t $PROFILER -o $OUTPUT_DIR/profiles/${name}.json -- $prog
done

# Step 2: Run fault injection
for profile in $OUTPUT_DIR/profiles/*.json; do
    name=$(basename $profile .json)
    echo "Injecting faults into $name..."
    python3 run_injection.py \
        --profile $profile \
        --output $OUTPUT_DIR/faults/${name}.csv \
        --trials 1000
done

# Step 3: Analyze results
python3 analyze_results.py \
    --profiles $OUTPUT_DIR/profiles/ \
    --faults $OUTPUT_DIR/faults/ \
    --output $OUTPUT_DIR/analysis/
```

### 8.2 数据处理脚本

```python
# analyze_results.py
import json
import pandas as pd
import numpy as np
from pathlib import Path

def load_profile(profile_path):
    with open(profile_path) as f:
        return json.load(f)

def load_fault_results(fault_path):
    return pd.read_csv(fault_path)

def merge_data(profiles_dir, faults_dir):
    all_data = []

    for profile_file in Path(profiles_dir).glob('*.json'):
        profile = load_profile(profile_file)
        fault_file = Path(faults_dir) / f"{profile_file.stem}.csv"

        if fault_file.exists():
            faults = load_fault_results(fault_file)

            for func in profile['functions']:
                func_faults = faults[faults['function'] == func['function_name']]

                if len(func_faults) > 0:
                    record = {
                        'program': profile_file.stem,
                        'function': func['function_name'],
                        # Features
                        **func['execution_stats'],
                        **func['data_flow'],
                        **func['compute_characteristics'],
                        **func['control_flow'],
                        **func['fault_sensitivity'],
                        # Outcomes
                        'masked_rate': (func_faults['result'] == 'masked').mean(),
                        'crash_rate': (func_faults['result'] == 'crash').mean(),
                        'sdc_rate': (func_faults['result'] == 'sdc').mean(),
                    }
                    all_data.append(record)

    return pd.DataFrame(all_data)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--profiles', required=True)
    parser.add_argument('--faults', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()

    df = merge_data(args.profiles, args.faults)
    df.to_csv(f"{args.output}/merged_data.csv", index=False)
    print(f"Merged {len(df)} function records")
```

---

## 九、参考文献

1. Hari, S. K. S., et al. "Relyzer: Exploiting application-level fault equivalence to analyze application resiliency to transient faults." ASPLOS 2012.

2. Li, G., et al. "Understanding error propagation in deep learning neural network (DNN) accelerators and applications." SC 2017.

3. Reis, G. A., et al. "SWIFT: Software implemented fault tolerance." CGO 2005.

4. Feng, S., et al. "Shoestring: Probabilistic soft error reliability on the cheap." ASPLOS 2010.

5. Lu, Q., et al. "Llfi: An intermediate code-level fault injection tool for hardware faults." SELSE 2015.

---

*研究方法论文档 v1.0*
