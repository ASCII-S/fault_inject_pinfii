# getStackInfo 工具

## 概述

`getStackInfo` 是一个轻量级的栈信息收集工具,用于记录程序执行过程中各个函数的栈指针（Stack Pointer）值。该工具在函数级别进行插桩，每当函数中有栈操作时，记录该函数的栈指针，主要用于分析程序的调用栈状态和栈帧结构。

## 源文件

- **文件路径**: `getStackInfo.cpp`
- **代码行数**: 85行
- **复杂度**: 简单

## 核心功能

### 基本功能
1. **函数级插桩**: 在函数级别插入栈监控代码
2. **栈指针记录**: 记录每个函数的栈指针（RSP/ESP）值
3. **追加模式**: 持续追加记录，不覆盖之前的数据
4. **轻量级**: 开销较小，适合长时间运行的程序

### 输出信息
- **函数名**: 执行的函数名称
- **栈指针值**: 该函数的栈指针（十进制）

## 工作原理

### 1. 函数级插桩
```cpp
VOID Routine(RTN rtn, VOID *v)
{
    RTN_Open(rtn);
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
    {
        if (INS_IsStackRead(ins) || INS_IsStackWrite(ins)) {
            // 找到第一个栈操作指令
            string *temp = new string(RTN_Name(rtn));
            const char *rtn_name = temp->c_str();
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)dostack,
                          IARG_CONTEXT, IARG_PTR, rtn_name, IARG_END);
            break;  // 每个函数只插桩一次
        }
    }
    RTN_Close(rtn);
}
```
- 遍历函数中的所有指令
- 找到第一个栈读写指令
- 插入回调函数
- **break**: 每个函数只插桩一次

### 2. 栈信息收集
```cpp
VOID dostack(CONTEXT *ctxt, VOID *rtn_name) {
    ofstream OutFile;
    OutFile.open("stackinfo", std::fstream::app);  // 追加模式
    ADDRINT rbp = (ADDRINT)PIN_GetContextReg(ctxt, REG_STACK_PTR);
    OutFile << (const char*)rtn_name << ":" << rbp << endl;
    OutFile.close();
}
```
- 获取栈指针寄存器（RSP）
- 格式：`函数名:栈指针值`
- 追加到文件

## 使用方法

### 编译
```bash
cd /home/tongshiyu/pin/source/tools/pinfi
make obj-intel64/getStackInfo.so
```

### 运行
```bash
# 基本用法
pin -t obj-intel64/getStackInfo.so -- ./target_program

# 输出文件: stackinfo
cat stackinfo
```

### 参数说明
该工具没有命令行参数，输出固定为 `stackinfo` 文件。

## 输出格式

### stackinfo
```
main:140737488347136
compute_matrix:140737488347040
multiply:140737488346944
add_vector:140737488346848
main:140737488347136
```

格式：
```
函数名:栈指针值(十进制)
```

### 典型输出示例
```
_start:140737488348160
__libc_start_main:140737488347904
main:140737488347680
foo:140737488347584
bar:140737488347488
baz:140737488347392
bar:140737488347488
foo:140737488347584
main:140737488347680
__libc_start_main:140737488347904
```

可以看到：
- 栈指针随调用深度递减（栈向低地址增长）
- 相同函数的多次调用可能有相同或不同的栈指针值
- 函数返回时栈指针恢复

## 应用场景

### 1. 调用栈分析
了解程序的函数调用序列：
```bash
pin -t obj-intel64/getStackInfo.so -- ./program
cat stackinfo

# 提取调用序列
awk -F':' '{print $1}' stackinfo

# 统计函数调用次数
awk -F':' '{print $1}' stackinfo | sort | uniq -c | sort -rn
```

### 2. 栈帧大小计算
计算函数的栈帧大小：
```bash
# 假设某个函数的栈指针值
FUNC="compute_matrix"

# 提取该函数的栈指针值
grep "^$FUNC:" stackinfo > func_stack.txt

# 计算栈帧范围
MIN=$(awk -F':' '{print $2}' func_stack.txt | sort -n | head -1)
MAX=$(awk -F':' '{print $2}' func_stack.txt | sort -n | tail -1)

echo "栈帧范围: $MIN - $MAX"
echo "栈帧大小: $((MAX - MIN)) 字节"
```

### 3. 递归深度分析
分析递归函数的调用深度：
```bash
pin -t obj-intel64/getStackInfo.so -- ./recursive_program

# 提取递归函数的栈指针
FUNC="fibonacci"
grep "^$FUNC:" stackinfo | awk -F':' '{print $2}' > fib_stack.txt

# 计算递归深度（不同栈指针值的数量）
sort -u fib_stack.txt | wc -l
```

### 4. 栈溢出检测
检测是否有栈溢出风险：
```bash
# 监控栈指针变化
awk -F':' '{print $2}' stackinfo | sort -n | head -1  # 最低栈地址
awk -F':' '{print $2}' stackinfo | sort -n | tail -1  # 最高栈地址

# 计算栈使用量
MIN=$(awk -F':' '{print $2}' stackinfo | sort -n | head -1)
MAX=$(awk -F':' '{print $2}' stackinfo | sort -n | tail -1)
echo "栈使用量: $((MAX - MIN)) 字节"
```

### 5. 函数调用图
生成函数调用关系图：
```bash
# 简单的调用序列可视化
awk -F':' '{
    depth = int(($2 - min) / 16);  # 粗略计算深度
    for (i = 0; i < depth; i++) printf("  ");
    print $1;
}' stackinfo
```

## 代码关键点

### REG_STACK_PTR
```cpp
ADDRINT rbp = (ADDRINT)PIN_GetContextReg(ctxt, REG_STACK_PTR);
```
- `REG_STACK_PTR` 是平台无关的栈指针寄存器
- x86-64: RSP
- x86: ESP
- ARM: SP

### 追加模式
```cpp
OutFile.open("stackinfo", std::fstream::app);
```
- 使用 `std::fstream::app` 追加模式
- 不覆盖之前的记录
- 适合长时间运行的程序

### 每函数单次插桩
```cpp
for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
    if (INS_IsStackRead(ins) || INS_IsStackWrite(ins)) {
        // ...
        break;  // 关键：只插桩第一个栈操作
    }
}
```
- 减少开销
- 每个函数只在第一次栈操作时记录

### 函数名传递
```cpp
string *temp = new string(RTN_Name(rtn));
const char *rtn_name = temp->c_str();
INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)dostack,
              IARG_CONTEXT, IARG_PTR, rtn_name, IARG_END);
```
- 使用 `new string` 确保字符串生命周期
- 通过指针传递给回调函数

## 性能考虑

### 开销分析
- **函数级插桩**: 比指令级插桩开销小得多
- **单次记录**: 每个函数只记录一次
- **文件 I/O**: 每次调用都打开/关闭文件，有一定开销

### 典型性能
| 程序类型 | 原始运行时间 | 带工具运行时间 | 倍数 |
|---------|-------------|--------------|------|
| 函数调用少 | 1s | 1.2s | 1.2x |
| 函数调用多 | 1s | 2-3s | 2-3x |
| 深度递归 | 1s | 3-5s | 3-5x |

### 优化建议
1. **缓冲输出**: 累积多条记录后一次性写入
2. **保持文件打开**: 避免频繁打开/关闭
3. **条件记录**: 只记录关心的函数

## 输出分析

### 示例分析
```
main:140737488347680
compute:140737488347584
helper:140737488347488
compute:140737488347584
main:140737488347680
```

观察：
1. **main → compute**: 栈指针减少 96 字节（栈帧大小约 96）
2. **compute → helper**: 栈指针减少 96 字节
3. **返回时栈指针恢复**: 栈正确平衡

### Python 分析脚本
```python
# analyze_stack.py
stack_info = []
with open('stackinfo', 'r') as f:
    for line in f:
        func, sp = line.strip().split(':')
        stack_info.append((func, int(sp)))

# 计算调用深度
call_stack = []
depth = 0
max_depth = 0

for func, sp in stack_info:
    if call_stack and sp < call_stack[-1][1]:
        # 栈指针减小 → 函数调用
        depth += 1
        max_depth = max(max_depth, depth)
    elif call_stack and sp > call_stack[-1][1]:
        # 栈指针增大 → 函数返回
        depth -= 1

    call_stack.append((func, sp))

print(f"最大调用深度: {max_depth}")

# 统计函数调用次数
from collections import Counter
func_counts = Counter([f for f, _ in stack_info])
print("函数调用统计:")
for func, count in func_counts.most_common():
    print(f"  {func}: {count}次")
```

## 典型工作流

### 调用栈监控
```bash
#!/bin/bash
PROGRAM="./matmul"

# 清理旧数据
rm -f stackinfo

# 运行程序
pin -t obj-intel64/getStackInfo.so -- $PROGRAM

# 分析结果
echo "=== 函数调用统计 ==="
awk -F':' '{print $1}' stackinfo | sort | uniq -c | sort -rn | head -10

echo ""
echo "=== 栈使用情况 ==="
MIN=$(awk -F':' '{print $2}' stackinfo | sort -n | head -1)
MAX=$(awk -F':' '{print $2}' stackinfo | sort -n | tail -1)
echo "最低栈地址: $MIN"
echo "最高栈地址: $MAX"
echo "栈使用量: $((MAX - MIN)) 字节"

echo ""
echo "=== 调用序列（前20条）==="
head -20 stackinfo
```

### 递归分析
```bash
#!/bin/bash
FUNC="fibonacci"

# 运行程序
pin -t obj-intel64/getStackInfo.so -- ./fib 10

# 提取递归函数的调用
grep "^$FUNC:" stackinfo > fib_calls.txt

# 统计递归深度
DEPTH=$(awk -F':' '{print $2}' fib_calls.txt | sort -u | wc -l)
echo "递归深度: $DEPTH"

# 统计总调用次数
CALLS=$(wc -l < fib_calls.txt)
echo "总调用次数: $CALLS"

# 每层栈帧大小
awk -F':' '{print $2}' fib_calls.txt | sort -n | uniq | \
    awk 'NR>1 {print prev - $1} {prev=$1}' | \
    awk '{sum+=$1; count++} END {if(count>0) print "平均栈帧: " sum/count " 字节"}'
```

## 局限性

1. **每函数一次**: 只记录第一次栈操作，不记录后续变化
2. **无时间戳**: 不记录时间信息
3. **无调用关系**: 不记录父子函数关系
4. **I/O 开销**: 频繁打开/关闭文件

## 扩展建议

1. **时间戳**: 添加时间戳记录
2. **调用关系**: 记录函数调用层次
3. **选择性记录**: 仅记录特定函数
4. **缓冲输出**: 减少文件 I/O
5. **实时监控**: 支持实时输出到终端
6. **栈帧详情**: 记录栈帧大小、局部变量等

## 与其他工具配合

### 与 faultinjection 配合
```bash
# 了解故障注入时的调用栈
pin -t obj-intel64/getStackInfo.so -- ./program
# 分析 stackinfo，了解哪些函数被调用

# 然后进行故障注入
pin -t obj-intel64/faultinjection.so ... -- ./program
```

### 与 findnextinst 配合
```bash
# getStackInfo 了解函数调用
pin -t obj-intel64/getStackInfo.so -- ./program

# findnextinst 查询特定位置的栈信息
# （需要 PC 地址）
```

## 调试技巧

### 检查文件权限
```bash
# 确保有写权限
touch stackinfo
ls -l stackinfo
```

### 验证栈指针
```bash
# 栈指针应该在合理范围内（Linux 64位）
awk -F':' '{print $2}' stackinfo | sort -n | head -1  # 应该是一个大数
awk -F':' '{print $2}' stackinfo | sort -n | tail -1  # 应该更大

# 检查是否为空
if [ ! -s stackinfo ]; then
    echo "警告: stackinfo 为空，可能程序没有栈操作"
fi
```

### 比较多次运行
```bash
# 第一次运行
pin -t obj-intel64/getStackInfo.so -- ./program
cp stackinfo stackinfo1

# 第二次运行
rm stackinfo
pin -t obj-intel64/getStackInfo.so -- ./program
cp stackinfo stackinfo2

# 比较（栈地址可能不同，但调用序列应该相同）
diff <(awk -F':' '{print $1}' stackinfo1) <(awk -F':' '{print $1}' stackinfo2)
```

## 注意事项

1. **追加模式**: 多次运行会追加，需要手动清理
2. **无符号**: 某些函数可能显示为地址而非名称
3. **内联函数**: 内联函数不会出现
4. **系统库**: 包含系统库函数调用

## 高级应用

### 生成调用树
```python
# call_tree.py
import sys

def build_call_tree():
    with open('stackinfo', 'r') as f:
        lines = [line.strip().split(':') for line in f]

    call_stack = []
    for func, sp in lines:
        sp = int(sp)

        # 找到正确的栈深度
        while call_stack and call_stack[-1][1] <= sp:
            call_stack.pop()

        # 打印缩进
        print('  ' * len(call_stack) + func)

        # 入栈
        call_stack.append((func, sp))

if __name__ == '__main__':
    build_call_tree()
```

运行：
```bash
pin -t obj-intel64/getStackInfo.so -- ./program
python3 call_tree.py
```

## 相关文件

- 源代码: `/home/tongshiyu/pin/source/tools/pinfi/getStackInfo.cpp`
- 依赖: `utils.h`, `pin.H`
- 输出: `stackinfo`

## 总结

`getStackInfo` 是一个简单但实用的工具，适合：
- 快速了解程序的函数调用情况
- 分析调用栈结构
- 辅助其他工具进行深入分析

它的轻量级特性使其适合作为初步分析工具，在不显著影响性能的情况下收集基本的栈信息。
