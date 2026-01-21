# Python 接口文档

## 概述

`unified_tracer.so` 用于识别程序的易崩溃指令及其寄存器数据流溯源，输出 JSON 格式结果供 Python 脚本解析使用。

## 使用流程

```
1. 运行 unified_tracer.so → 生成 trace_result.json
2. Python 解析 JSON → 提取崩溃场景
3. 自适应注错循环 → 根据崩溃率决定是否溯源
```

## JSON 输出格式

### 完整结构

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
      "offset": "0x1234",           // 指令偏移（十六进制字符串）
      "disasm": "mov [rax+rbx*4], rcx",
      "type": "mem_write",          // 易崩溃类型
      "exec_count": 100,            // 执行次数
      "crash_regs": ["rax", "rbx"], // 崩溃寄存器列表
      "register_traces": {          // 每个寄存器的溯源链
        "rax": [
          {
            "offset": "0x1200",     // 源指令偏移
            "disasm": "mov rax, [rdi]",
            "depth": 1,             // 溯源深度（1=直接来源）
            "hit_count": 50         // 命中次数
          },
          {
            "offset": "0x1180",
            "disasm": "lea rax, [rbp-0x20]",
            "depth": 2,
            "hit_count": 30
          }
        ],
        "rbx": [
          {
            "offset": "0x1210",
            "disasm": "mov rbx, rsi",
            "depth": 1,
            "hit_count": 100
          }
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

### 关键字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `img_base_addr` | string | 主镜像基地址（十六进制，如 "0x400000"） |
| `offset` | string | 指令偏移（十六进制，如 "0x1234"） |
| `disasm` | string | 指令反汇编 |
| `type` | string | 易崩溃类型：mem_write, mem_read, index_access, indirect_cf, div |
| `exec_count` | int | 执行次数 |
| `crash_regs` | list[string] | 崩溃寄存器名称列表 |
| `register_traces` | dict | 每个寄存器 → 溯源指令列表 |
| `depth` | int | 溯源深度（0=易崩溃点本身，1=直接来源，2=二级来源...） |

**注意**：
- JSON 中不包含 depth=0 的条目，易崩溃指令本身就是 depth=0。
- **计算绝对地址**：进行故障注入时需要使用绝对地址 = `img_base_addr` + `offset`

## Python 数据结构

### 注错目标

```python
from dataclasses import dataclass
from typing import List, Optional

@dataclass
class InjectionTarget:
    """单个注错目标"""
    offset: int                    # 指令偏移（整数）
    register: str                  # 目标寄存器名称
    disasm: str                    # 指令反汇编
    depth: int                     # 溯源深度（0=易崩溃点）
    parent_offset: Optional[int]   # 父指令偏移（depth>0时有效）

    # 注错统计
    injection_count: int = 0       # 已注错次数
    crash_count: int = 0           # 崩溃次数

    @property
    def crash_rate(self) -> float:
        """计算崩溃率"""
        if self.injection_count == 0:
            return 0.0
        return self.crash_count / self.injection_count
```

### 崩溃场景

```python
@dataclass
class CrashScenario:
    """一个易崩溃指令及其所有寄存器的溯源"""
    crashprone_offset: int         # 易崩溃指令偏移
    crashprone_disasm: str         # 易崩溃指令反汇编
    cp_type: str                   # 易崩溃类型
    exec_count: int                # 执行次数
    crash_regs: List[str]          # 崩溃寄存器列表

    # 每个寄存器的溯源链：register_name → list of sources
    register_traces: dict[str, List[InjectionTarget]]
```

## Python 接口示例

### 1. 解析 JSON 输出

```python
import json
import subprocess
from typing import List, Dict

def run_tracer(program: str, args: List[str], output_file: str,
               depth: int = 5) -> str:
    """运行 unified_tracer 工具

    Args:
        program: 目标程序路径
        args: 程序参数
        output_file: JSON 输出文件路径
        depth: 最大溯源深度

    Returns:
        输出文件路径
    """
    cmd = [
        "/home/tongshiyu/pin/pin",
        "-t", "obj-intel64/crashprone_tracer/unified_tracer.so",
        "-o", output_file,
        "-depth", str(depth),
        "--", program
    ] + args

    subprocess.run(cmd, check=True)
    return output_file


def parse_trace_result(json_file: str) -> Tuple[int, List[CrashScenario]]:
    """解析 unified_tracer 输出

    Args:
        json_file: JSON 文件路径

    Returns:
        (主镜像基地址, 崩溃场景列表)
    """
    with open(json_file, 'r') as f:
        data = json.load(f)

    # 提取主镜像基地址
    img_base_addr = int(data['config']['img_base_addr'], 16)

    scenarios = []

    for inst in data['crashprone_insts']:
        offset = int(inst['offset'], 16)  # 十六进制字符串转整数

        # 解析每个寄存器的溯源链
        register_traces = {}
        for reg_name, sources in inst['register_traces'].items():
            trace_list = []
            for src in sources:
                target = InjectionTarget(
                    offset=int(src['offset'], 16),
                    register=reg_name,
                    disasm=src['disasm'],
                    depth=src['depth'],
                    parent_offset=offset  # 溯源点的父指令
                )
                trace_list.append(target)
            register_traces[reg_name] = trace_list

        scenario = CrashScenario(
            crashprone_offset=offset,
            crashprone_disasm=inst['disasm'],
            cp_type=inst['type'],
            exec_count=inst['exec_count'],
            crash_regs=inst['crash_regs'],
            register_traces=register_traces
        )
        scenarios.append(scenario)

    return img_base_addr, scenarios


def get_absolute_address(base_addr: int, offset: int) -> int:
    """计算指令绝对地址

    Args:
        base_addr: 主镜像基地址
        offset: 指令相对偏移

    Returns:
        绝对地址
    """
    return base_addr + offset
```

### 2. 自适应注错逻辑

```python
from collections import deque

class AdaptiveFaultInjector:
    """自适应故障注入器"""

    def __init__(self, img_base_addr: int,
                 crash_threshold: float = 0.3,
                 injections_per_target: int = 10):
        self.img_base_addr = img_base_addr
        self.crash_threshold = crash_threshold
        self.injections_per_target = injections_per_target
        self.injection_queue = deque()
        self.completed_targets = set()  # (offset, register)

    def initialize_queue(self, scenarios: List[CrashScenario]):
        """初始化注错队列：添加所有 depth=0 的易崩溃点"""
        for scenario in scenarios:
            for reg in scenario.crash_regs:
                target = InjectionTarget(
                    offset=scenario.crashprone_offset,
                    register=reg,
                    disasm=scenario.crashprone_disasm,
                    depth=0,
                    parent_offset=None
                )
                self.injection_queue.append((target, scenario))

    def inject_once(self, program: str, args: List[str],
                   offset: int, register: str, kth: int) -> str:
        """执行一次故障注入

        Args:
            program: 目标程序
            args: 程序参数
            offset: 目标指令偏移（相对偏移）
            register: 目标寄存器
            kth: 第几次执行时注错

        Returns:
            注错结果：'crash', 'hang', 'benign'
        """
        # 计算绝对地址
        absolute_addr = self.img_base_addr + offset

        # 调用故障注入工具
        # 注意：某些工具可能需要绝对地址，某些需要偏移地址
        # 根据你的 faultinjection.so 接口选择使用 absolute_addr 或 offset
        cmd = [
            "/home/tongshiyu/pin/pin",
            "-t", "obj-intel64/faultinjection.so",
            "-targetAddr", hex(absolute_addr),  # 使用绝对地址
            # 或者使用: "-targetOffset", hex(offset),  # 使用相对偏移
            "-targetReg", register,
            "-targetKth", str(kth),
            "--", program
        ] + args

        try:
            result = subprocess.run(cmd, timeout=30, capture_output=True)
            if result.returncode < 0:
                return 'crash'
            elif result.returncode > 0:
                return 'crash'
            else:
                return 'benign'
        except subprocess.TimeoutExpired:
            return 'hang'
        except Exception:
            return 'crash'

    def run_adaptive_injection(self, program: str, args: List[str],
                              scenarios: List[CrashScenario]):
        """运行自适应故障注入

        流程：
        1. 从队列取出一个目标 (target, scenario)
        2. 注错 injections_per_target 次
        3. 如果崩溃率 > 阈值，将 depth+1 的溯源点加入队列
        4. 重复直到队列为空
        """
        self.initialize_queue(scenarios)

        while self.injection_queue:
            target, scenario = self.injection_queue.popleft()

            # 检查是否已处理
            key = (target.offset, target.register)
            if key in self.completed_targets:
                continue

            print(f"[Depth {target.depth}] 注错: 0x{target.offset:x} ({target.register}) - {target.disasm}")

            # 执行注错
            for i in range(1, self.injections_per_target + 1):
                result = self.inject_once(program, args, target.offset,
                                         target.register, i)
                target.injection_count += 1
                if result == 'crash':
                    target.crash_count += 1

            crash_rate = target.crash_rate
            print(f"  崩溃率: {crash_rate:.1%} ({target.crash_count}/{target.injection_count})")

            # 标记为已完成
            self.completed_targets.add(key)

            # 如果崩溃率超过阈值，添加溯源点到队列
            if crash_rate > self.crash_threshold:
                print(f"  → 崩溃率超过阈值 {self.crash_threshold:.1%}，添加溯源点")

                # 查找该寄存器的溯源链
                if target.register in scenario.register_traces:
                    sources = scenario.register_traces[target.register]

                    # 筛选 depth = target.depth + 1 的源指令
                    next_depth = target.depth + 1
                    for src in sources:
                        if src.depth == next_depth:
                            # 检查是否已处理
                            src_key = (src.offset, src.register)
                            if src_key not in self.completed_targets:
                                print(f"    添加: 0x{src.offset:x} ({src.register}) depth={src.depth}")
                                self.injection_queue.append((src, scenario))
```

### 3. 完整使用示例

```python
def main():
    # 配置
    program = "./myprogram"
    args = ["arg1", "arg2"]
    trace_output = "trace_result.json"

    # 步骤 1: 运行溯源工具
    print("[Step 1] 运行易崩溃指令溯源...")
    run_tracer(program, args, trace_output, depth=5)

    # 步骤 2: 解析结果
    print("[Step 2] 解析溯源结果...")
    img_base_addr, scenarios = parse_trace_result(trace_output)
    print(f"  主镜像基地址: 0x{img_base_addr:x}")
    print(f"  发现 {len(scenarios)} 个崩溃场景")

    # 步骤 3: 自适应注错
    print("[Step 3] 开始自适应故障注入...")
    injector = AdaptiveFaultInjector(
        img_base_addr=img_base_addr,  # 传入基地址用于计算绝对地址
        crash_threshold=0.3,
        injections_per_target=10
    )
    injector.run_adaptive_injection(program, args, scenarios)

    print("\n注错完成！")
    print(f"  总共处理了 {len(injector.completed_targets)} 个目标")

if __name__ == "__main__":
    main()
```

## 高级用法

### 按执行次数排序

```python
def get_top_crashprone(scenarios: List[CrashScenario],
                       top_n: int = 20) -> List[CrashScenario]:
    """按执行次数排序，返回前 N 个"""
    return sorted(scenarios, key=lambda s: s.exec_count, reverse=True)[:top_n]
```

### 过滤特定类型

```python
def filter_by_type(scenarios: List[CrashScenario],
                   cp_types: List[str]) -> List[CrashScenario]:
    """过滤特定易崩溃类型"""
    return [s for s in scenarios if s.cp_type in cp_types]

# 示例：只注错内存写指令
mem_write_scenarios = filter_by_type(scenarios, ['mem_write', 'index_access'])
```

### 保存注错结果

```python
def save_injection_results(injector: AdaptiveFaultInjector, output_file: str):
    """保存注错统计结果"""
    results = []
    for (offset, register) in injector.completed_targets:
        # 这里需要从 injector 内部数据结构获取统计信息
        results.append({
            "offset": hex(offset),
            "register": register,
            # ... 其他统计信息
        })

    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)
```

## 注意事项

1. **offset 格式转换**：JSON 中的 offset 是十六进制字符串（如 "0x1234"），需要用 `int(offset, 16)` 转为整数
2. **depth=0 不在 JSON 中**：易崩溃指令本身就是 depth=0，其溯源链从 depth=1 开始
3. **寄存器名称大小写**：JSON 中的寄存器名称使用小写（如 "rax", "rbx"）
4. **并发注错**：如果要并发执行注错，注意线程安全和资源管理
5. **超时处理**：注错可能导致程序挂起，建议设置超时（如 30 秒）
