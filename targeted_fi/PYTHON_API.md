# Python 接口文档

## 概述

本文档说明如何在 Python 脚本中调用 `targeted_faultinjection.so` 进行故障注入实验，以及如何与 `crashprone_tracer/unified_tracer.so` 和 `adaptive_injector.py` 集成。

## 基本调用方式

### 最小示例

```python
import subprocess

def inject_fault(program, args, target_pc, target_reg, kth, inject_bit=-1, timeout=30):
    """
    执行一次故障注入

    Args:
        program: 目标程序路径
        args: 程序参数列表
        target_pc: 目标指令绝对地址（整数）
        target_reg: 目标寄存器名称（字符串）
        kth: 第几次执行时注错（整数）
        inject_bit: 注入比特位（-1=随机）
        timeout: 超时时间（秒）

    Returns:
        tuple: (result, inject_info)
            result: 'crash', 'hang', 'benign'
            inject_info: 注错信息字典（如果有输出文件）
    """
    cmd = [
        "/home/tongshiyu/pin/pin",
        "-t", "/home/tongshiyu/pin/source/tools/pinfi/obj-intel64/targeted_fi/targeted_faultinjection.so",
        "-target_pc", hex(target_pc),
        "-target_reg", target_reg,
        "-target_kth", str(kth),
        "-inject_bit", str(inject_bit),
        "-o", "/tmp/inject_info.txt",
        "--", program
    ] + args

    try:
        result = subprocess.run(cmd, timeout=timeout, capture_output=True)

        # 读取注错信息
        inject_info = parse_inject_info("/tmp/inject_info.txt")

        if result.returncode < 0:
            return 'crash', inject_info
        elif result.returncode > 0:
            return 'crash', inject_info
        else:
            return 'benign', inject_info

    except subprocess.TimeoutExpired:
        return 'hang', None
    except Exception as e:
        print(f"注错执行失败: {e}")
        return 'crash', None


def parse_inject_info(filepath):
    """解析注错信息文件"""
    info = {}
    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if ':' in line:
                    key, value = line.split(':', 1)
                    info[key.strip()] = value.strip()
    except FileNotFoundError:
        return None
    return info


# 使用示例
if __name__ == "__main__":
    result, info = inject_fault(
        program="/bin/ls",
        args=["/tmp"],
        target_pc=0x402a16,
        target_reg="rsi",
        kth=1,
        inject_bit=-1
    )

    print(f"注错结果: {result}")
    if info:
        print(f"原始值: {info.get('original_value')}")
        print(f"注入值: {info.get('injected_value')}")
        print(f"翻转比特: {info.get('inject_bit')}")
```

## 完整工作流程

### 与 unified_tracer 和 adaptive_injector 集成

```python
#!/usr/bin/env python3
"""
完整的自适应故障注入工作流程示例

流程：
1. 使用 unified_tracer.so 识别易崩溃指令及其溯源
2. 对 depth=0 的指令开始注错
3. 如果崩溃率 > 阈值，将其 depth+1 的溯源点加入队列
4. 继续直到队列为空
"""

import subprocess
import json
import os
from dataclasses import dataclass
from typing import List, Dict, Optional, Tuple
from collections import deque

# ================ 配置 ================

PIN_ROOT = "/home/tongshiyu/pin"
PINFI_ROOT = "/home/tongshiyu/pin/source/tools/pinfi"
UNIFIED_TRACER = f"{PINFI_ROOT}/obj-intel64/crashprone_tracer/unified_tracer.so"
TARGETED_FI = f"{PINFI_ROOT}/obj-intel64/targeted_fi/targeted_faultinjection.so"


# ================ 数据结构 ================

@dataclass
class InjectionTarget:
    """单个注错目标"""
    pc: int                        # 绝对地址
    offset: int                    # 相对偏移
    register: str                  # 目标寄存器
    disasm: str                    # 指令反汇编
    depth: int                     # 溯源深度（0=易崩溃点）

    # 注错统计
    injection_count: int = 0
    crash_count: int = 0
    hang_count: int = 0
    benign_count: int = 0

    @property
    def crash_rate(self) -> float:
        if self.injection_count == 0:
            return 0.0
        return self.crash_count / self.injection_count


@dataclass
class CrashScenario:
    """一个易崩溃指令及其溯源"""
    offset: int                    # 易崩溃指令偏移
    disasm: str                    # 反汇编
    cp_type: str                   # 类型
    exec_count: int                # 执行次数
    crash_regs: List[str]          # 崩溃寄存器列表

    # 每个寄存器的溯源链
    register_traces: Dict[str, List[dict]]


# ================ unified_tracer 接口 ================

class UnifiedTracer:
    """unified_tracer.so 接口"""

    def __init__(self, pin_root=PIN_ROOT, tool_path=UNIFIED_TRACER):
        self.pin = f"{pin_root}/pin"
        self.tool = tool_path
        self.img_base = 0

    def run(self, program: str, args: List[str], output_file: str,
            depth: int = 5, min_exec: int = 2) -> bool:
        """
        运行 unified_tracer

        Returns:
            bool: 是否成功
        """
        cmd = [
            self.pin,
            "-t", self.tool,
            "-o", output_file,
            "-depth", str(depth),
            "-min_exec", str(min_exec),
            "--", program
        ] + args

        print(f"[Tracer] 运行: {' '.join(cmd[:8])}...")

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)

            # 从 stderr 提取镜像基址
            for line in result.stderr.split('\n'):
                if '主镜像' in line and '0x' in line:
                    # 格式: [UnifiedTracer] 主镜像: /bin/ls (0x400000 - 0x41da63)
                    import re
                    match = re.search(r'\(0x([0-9a-fA-F]+)', line)
                    if match:
                        self.img_base = int(match.group(1), 16)
                        print(f"[Tracer] 镜像基址: 0x{self.img_base:x}")

            return os.path.exists(output_file)
        except Exception as e:
            print(f"[Tracer] 错误: {e}")
            return False

    def parse_result(self, json_file: str) -> List[CrashScenario]:
        """
        解析 unified_tracer 输出

        Returns:
            崩溃场景列表
        """
        with open(json_file, 'r') as f:
            data = json.load(f)

        # 尝试从 config 获取镜像名称（如果需要）
        if 'config' in data and self.img_base == 0:
            # 镜像基址需要从运行时输出获取，这里假设已经设置
            pass

        scenarios = []
        for inst in data.get('crashprone_insts', []):
            offset = int(inst['offset'], 16)

            scenario = CrashScenario(
                offset=offset,
                disasm=inst['disasm'],
                cp_type=inst['type'],
                exec_count=inst['exec_count'],
                crash_regs=inst['crash_regs'],
                register_traces=inst.get('register_traces', {})
            )
            scenarios.append(scenario)

        print(f"[Tracer] 解析完成: {len(scenarios)} 个崩溃场景")
        return scenarios

    def offset_to_pc(self, offset: int) -> int:
        """将偏移转换为绝对地址"""
        return self.img_base + offset


# ================ targeted_faultinjection 接口 ================

class TargetedFaultInjector:
    """targeted_faultinjection.so 接口"""

    def __init__(self, pin_root=PIN_ROOT, tool_path=TARGETED_FI):
        self.pin = f"{pin_root}/pin"
        self.tool = tool_path

    def inject_once(self, program: str, args: List[str],
                    target_pc: int, target_reg: str, kth: int,
                    inject_bit: int = -1,
                    timeout: int = 30) -> Tuple[str, Optional[dict]]:
        """
        执行一次故障注入

        Returns:
            (result, inject_info)
            result: 'crash', 'hang', 'benign', 'not_injected'
        """
        output_file = f"/tmp/inject_{target_pc:x}_{target_reg}_{kth}.txt"

        cmd = [
            self.pin,
            "-t", self.tool,
            "-target_pc", hex(target_pc),
            "-target_reg", target_reg,
            "-target_kth", str(kth),
            "-inject_bit", str(inject_bit),
            "-o", output_file,
            "--", program
        ] + args

        try:
            result = subprocess.run(cmd, timeout=timeout,
                                    capture_output=True, text=True)

            # 解析注错信息
            inject_info = self._parse_inject_info(output_file)

            # 检查是否实际执行了注错
            if inject_info is None:
                return 'not_injected', None

            # 根据返回码判断结果
            if result.returncode < 0:
                # 被信号终止（如 SIGSEGV）
                return 'crash', inject_info
            elif result.returncode > 0:
                # 非零退出码
                return 'crash', inject_info
            else:
                return 'benign', inject_info

        except subprocess.TimeoutExpired:
            return 'hang', None
        except Exception as e:
            print(f"[Injector] 错误: {e}")
            return 'crash', None

    def _parse_inject_info(self, filepath: str) -> Optional[dict]:
        """解析注错信息文件"""
        if not os.path.exists(filepath):
            return None

        info = {}
        try:
            with open(filepath, 'r') as f:
                for line in f:
                    line = line.strip()
                    if ':' in line:
                        key, value = line.split(':', 1)
                        info[key.strip()] = value.strip()
            return info if info else None
        except Exception:
            return None


# ================ 自适应故障注入器 ================

class AdaptiveFaultInjector:
    """
    自适应故障注入器

    工作流程：
    1. 初始化队列：depth=0 的易崩溃指令
    2. 对队列中每个目标注错 N 次
    3. 如果崩溃率 > 阈值，添加 depth+1 的溯源点到队列
    4. 继续直到队列为空
    """

    def __init__(self,
                 crash_threshold: float = 0.3,
                 injections_per_target: int = 10,
                 timeout: int = 30):
        self.crash_threshold = crash_threshold
        self.injections_per_target = injections_per_target
        self.timeout = timeout

        self.injection_queue: deque = deque()
        self.completed_targets: Dict[Tuple[int, str], InjectionTarget] = {}
        self.tracer = UnifiedTracer()
        self.injector = TargetedFaultInjector()

    def run(self, program: str, args: List[str],
            trace_output: str = "/tmp/trace.json",
            trace_depth: int = 5) -> Dict:
        """
        运行完整的自适应故障注入流程

        Returns:
            结果字典
        """
        # Step 1: 运行溯源
        print("\n" + "=" * 60)
        print("[Step 1] 运行易崩溃指令溯源")
        print("=" * 60)

        if not self.tracer.run(program, args, trace_output, depth=trace_depth):
            print("[错误] 溯源失败")
            return {"error": "tracer_failed"}

        # Step 2: 解析结果
        print("\n" + "=" * 60)
        print("[Step 2] 解析溯源结果")
        print("=" * 60)

        scenarios = self.tracer.parse_result(trace_output)
        if not scenarios:
            print("[错误] 没有发现易崩溃指令")
            return {"error": "no_crashprone_insts"}

        # Step 3: 初始化注错队列
        print("\n" + "=" * 60)
        print("[Step 3] 初始化注错队列（depth=0）")
        print("=" * 60)

        self._initialize_queue(scenarios)
        print(f"[队列] 初始目标数: {len(self.injection_queue)}")

        # Step 4: 自适应注错循环
        print("\n" + "=" * 60)
        print("[Step 4] 开始自适应故障注入")
        print("=" * 60)

        self._run_adaptive_injection(program, args, scenarios)

        # Step 5: 生成报告
        return self._generate_report()

    def _initialize_queue(self, scenarios: List[CrashScenario]):
        """初始化注错队列：添加所有 depth=0 的易崩溃点"""
        for scenario in scenarios:
            pc = self.tracer.offset_to_pc(scenario.offset)

            for reg in scenario.crash_regs:
                target = InjectionTarget(
                    pc=pc,
                    offset=scenario.offset,
                    register=reg,
                    disasm=scenario.disasm,
                    depth=0
                )
                self.injection_queue.append((target, scenario))

    def _run_adaptive_injection(self, program: str, args: List[str],
                                 scenarios: List[CrashScenario]):
        """运行自适应故障注入循环"""
        round_num = 0

        while self.injection_queue:
            round_num += 1
            target, scenario = self.injection_queue.popleft()

            # 检查是否已处理
            key = (target.pc, target.register)
            if key in self.completed_targets:
                continue

            print(f"\n[Round {round_num}] Depth={target.depth} "
                  f"PC=0x{target.pc:x} Reg={target.register}")
            print(f"  指令: {target.disasm}")

            # 执行注错
            for i in range(1, self.injections_per_target + 1):
                result, info = self.injector.inject_once(
                    program, args,
                    target.pc, target.register, i,
                    inject_bit=-1,
                    timeout=self.timeout
                )

                target.injection_count += 1
                if result == 'crash':
                    target.crash_count += 1
                elif result == 'hang':
                    target.hang_count += 1
                elif result == 'benign':
                    target.benign_count += 1

                print(f"  [{i}/{self.injections_per_target}] {result}", end="")
                if info:
                    print(f" (bit={info.get('inject_bit', '?')})", end="")
                print()

            # 计算崩溃率
            crash_rate = target.crash_rate
            print(f"  崩溃率: {crash_rate:.1%} "
                  f"({target.crash_count}/{target.injection_count})")

            # 保存结果
            self.completed_targets[key] = target

            # 如果崩溃率超过阈值，添加溯源点
            if crash_rate > self.crash_threshold:
                print(f"  → 崩溃率超过阈值 {self.crash_threshold:.1%}，添加溯源点")
                self._add_trace_sources(target, scenario)

    def _add_trace_sources(self, target: InjectionTarget,
                           scenario: CrashScenario):
        """添加溯源点到队列"""
        reg_name = target.register
        next_depth = target.depth + 1

        # 获取该寄存器的溯源链
        traces = scenario.register_traces.get(reg_name, [])

        for source in traces:
            if source.get('depth') == next_depth:
                source_offset = int(source['offset'], 16)
                source_pc = self.tracer.offset_to_pc(source_offset)

                # 检查是否已处理
                key = (source_pc, reg_name)
                if key in self.completed_targets:
                    continue

                # 创建新目标
                new_target = InjectionTarget(
                    pc=source_pc,
                    offset=source_offset,
                    register=reg_name,
                    disasm=source.get('disasm', ''),
                    depth=next_depth
                )

                print(f"    添加: 0x{source_pc:x} ({reg_name}) depth={next_depth}")
                self.injection_queue.append((new_target, scenario))

    def _generate_report(self) -> Dict:
        """生成报告"""
        targets = []
        total_crashes = 0
        total_injections = 0

        for target in self.completed_targets.values():
            targets.append({
                "pc": hex(target.pc),
                "offset": hex(target.offset),
                "register": target.register,
                "disasm": target.disasm,
                "depth": target.depth,
                "injection_count": target.injection_count,
                "crash_count": target.crash_count,
                "hang_count": target.hang_count,
                "benign_count": target.benign_count,
                "crash_rate": target.crash_rate
            })
            total_crashes += target.crash_count
            total_injections += target.injection_count

        return {
            "summary": {
                "total_targets": len(self.completed_targets),
                "total_injections": total_injections,
                "total_crashes": total_crashes,
                "overall_crash_rate": total_crashes / total_injections if total_injections > 0 else 0
            },
            "targets": targets
        }


# ================ 主函数 ================

def main():
    import argparse

    parser = argparse.ArgumentParser(description="自适应故障注入")
    parser.add_argument("-p", "--program", required=True, help="目标程序路径")
    parser.add_argument("-args", default="", help="程序参数（用引号括起）")
    parser.add_argument("-o", "--output-dir", default="./adaptive_results",
                        help="输出目录")
    parser.add_argument("--threshold", type=float, default=0.3,
                        help="崩溃率阈值")
    parser.add_argument("--injections", type=int, default=10,
                        help="每目标注错次数")
    parser.add_argument("--depth", type=int, default=5,
                        help="最大溯源深度")
    parser.add_argument("--timeout", type=int, default=30,
                        help="单次注错超时（秒）")

    args = parser.parse_args()

    # 创建输出目录
    os.makedirs(args.output_dir, exist_ok=True)

    # 解析程序参数
    program_args = args.args.split() if args.args else []

    # 创建注入器
    injector = AdaptiveFaultInjector(
        crash_threshold=args.threshold,
        injections_per_target=args.injections,
        timeout=args.timeout
    )

    # 运行
    trace_output = os.path.join(args.output_dir, "trace.json")
    results = injector.run(
        program=args.program,
        args=program_args,
        trace_output=trace_output,
        trace_depth=args.depth
    )

    # 保存结果
    result_file = os.path.join(args.output_dir, "injection_results.json")
    with open(result_file, 'w') as f:
        json.dump(results, f, indent=2)

    print(f"\n" + "=" * 60)
    print(f"[完成] 结果已保存到: {result_file}")
    print(f"=" * 60)

    # 打印摘要
    summary = results.get("summary", {})
    print(f"\n总目标数: {summary.get('total_targets', 0)}")
    print(f"总注错次数: {summary.get('total_injections', 0)}")
    print(f"总崩溃次数: {summary.get('total_crashes', 0)}")
    print(f"总体崩溃率: {summary.get('overall_crash_rate', 0):.1%}")


if __name__ == "__main__":
    main()
```

## 单独使用 TargetedFaultInjector

### 基本用法

```python
from targeted_fi_api import TargetedFaultInjector

injector = TargetedFaultInjector()

# 单次注错
result, info = injector.inject_once(
    program="./myprogram",
    args=["arg1", "arg2"],
    target_pc=0x401234,
    target_reg="rax",
    kth=1,
    inject_bit=-1,  # 随机
    timeout=30
)

print(f"结果: {result}")  # 'crash', 'hang', 'benign', 'not_injected'
if info:
    print(f"翻转比特: {info['inject_bit']}")
    print(f"原值: {info['original_value']}")
    print(f"新值: {info['injected_value']}")
```

### 批量注错

```python
def batch_inject(program, args, targets, injections_per_target=10):
    """
    批量注错

    Args:
        targets: [(pc, register), ...] 目标列表
    """
    injector = TargetedFaultInjector()
    results = []

    for pc, reg in targets:
        target_results = {
            "pc": hex(pc),
            "register": reg,
            "crash_count": 0,
            "hang_count": 0,
            "benign_count": 0
        }

        for kth in range(1, injections_per_target + 1):
            result, info = injector.inject_once(
                program, args, pc, reg, kth
            )

            if result == 'crash':
                target_results["crash_count"] += 1
            elif result == 'hang':
                target_results["hang_count"] += 1
            else:
                target_results["benign_count"] += 1

        target_results["crash_rate"] = (
            target_results["crash_count"] / injections_per_target
        )
        results.append(target_results)

    return results


# 使用示例
targets = [
    (0x401234, "rax"),
    (0x401234, "rbx"),
    (0x405678, "rsi")
]

results = batch_inject("./myprogram", ["arg1"], targets)
for r in results:
    print(f"{r['pc']} {r['register']}: crash_rate={r['crash_rate']:.1%}")
```

### XMM 寄存器注错

```python
# 对 XMM0 寄存器注错
result, info = injector.inject_once(
    program="./fp_program",
    args=[],
    target_pc=0x403000,
    target_reg="xmm0",      # XMM 寄存器（128位）
    kth=1,
    inject_bit=64,          # 翻转第 64 位
    timeout=30
)
```

## 与 crashprone_tracer 的完整集成

### 自动获取目标并注错

```python
import json
import os

def auto_inject(program, args, output_dir="./results"):
    """
    自动获取易崩溃指令并注错

    流程：
    1. 运行 unified_tracer 获取易崩溃指令
    2. 对每个指令的每个寄存器注错
    3. 返回结果
    """
    os.makedirs(output_dir, exist_ok=True)

    tracer = UnifiedTracer()
    injector = TargetedFaultInjector()

    # Step 1: 运行溯源
    trace_file = os.path.join(output_dir, "trace.json")
    if not tracer.run(program, args, trace_file, depth=3):
        return {"error": "tracer_failed"}

    # Step 2: 解析结果
    scenarios = tracer.parse_result(trace_file)

    # Step 3: 注错
    results = []
    for scenario in scenarios[:10]:  # 只测试前10个
        pc = tracer.offset_to_pc(scenario.offset)

        for reg in scenario.crash_regs:
            # 注错 5 次
            crashes = 0
            for kth in range(1, 6):
                result, _ = injector.inject_once(
                    program, args, pc, reg, kth, inject_bit=-1
                )
                if result == 'crash':
                    crashes += 1

            results.append({
                "pc": hex(pc),
                "offset": hex(scenario.offset),
                "register": reg,
                "disasm": scenario.disasm,
                "crash_rate": crashes / 5
            })

    return {"targets": results}


# 使用
results = auto_inject("/bin/ls", ["/tmp"])
for r in results["targets"]:
    print(f"{r['pc']} {r['register']}: {r['crash_rate']:.0%}")
```

## 高级用法

### 并行注错

```python
from concurrent.futures import ProcessPoolExecutor
import multiprocessing

def inject_worker(args):
    """工作进程"""
    program, program_args, pc, reg, kth = args

    injector = TargetedFaultInjector()
    result, info = injector.inject_once(
        program, program_args, pc, reg, kth, inject_bit=-1
    )

    return (pc, reg, kth, result, info)


def parallel_inject(program, args, targets, max_workers=4):
    """
    并行注错

    Args:
        targets: [(pc, reg, kth), ...] 目标列表
    """
    work_items = [(program, args, pc, reg, kth) for pc, reg, kth in targets]

    results = []
    with ProcessPoolExecutor(max_workers=max_workers) as executor:
        for result in executor.map(inject_worker, work_items):
            results.append(result)

    return results


# 使用示例
targets = [
    (0x401234, "rax", 1),
    (0x401234, "rax", 2),
    (0x401234, "rax", 3),
    (0x401234, "rbx", 1),
]

results = parallel_inject("./myprogram", ["arg1"], targets, max_workers=4)
```

### 结果分析

```python
import pandas as pd

def analyze_results(results_file):
    """分析注错结果"""
    with open(results_file, 'r') as f:
        data = json.load(f)

    # 转换为 DataFrame
    df = pd.DataFrame(data['targets'])

    # 按崩溃率排序
    top_crash = df.sort_values('crash_rate', ascending=False).head(10)
    print("崩溃率最高的目标:")
    print(top_crash[['pc', 'register', 'depth', 'crash_rate']])

    # 按深度分组统计
    depth_stats = df.groupby('depth').agg({
        'crash_rate': 'mean',
        'injection_count': 'sum',
        'crash_count': 'sum'
    })
    print("\n按深度统计:")
    print(depth_stats)

    # 按寄存器分组统计
    reg_stats = df.groupby('register').agg({
        'crash_rate': 'mean',
        'crash_count': 'sum'
    }).sort_values('crash_rate', ascending=False)
    print("\n按寄存器统计:")
    print(reg_stats)


# 使用
analyze_results("./results/injection_results.json")
```

## 注意事项

1. **绝对地址计算**：
   ```python
   # unified_tracer 输出的是偏移，需要加上镜像基址
   absolute_pc = img_base + offset
   ```

2. **进程隔离**：
   - 每次注错在独立进程中运行
   - 崩溃不会影响主进程

3. **超时处理**：
   - 默认 30 秒超时
   - 超时返回 'hang'
   - 建议根据程序复杂度调整

4. **文件清理**：
   - 注错信息文件会覆盖
   - 建议每次使用不同的输出文件名

5. **性能考虑**：
   - Pin 有 5-10 倍性能开销
   - 并行注错可以提高效率
   - 建议限制同时运行的进程数

6. **结果可靠性**：
   - 随机比特位注错每次结果不同
   - 建议多次注错取统计值

## 常见问题

**Q: 如何获取镜像基址？**

A: 运行 `unified_tracer` 时会输出：
```
[UnifiedTracer] 主镜像: /bin/ls (0x400000 - 0x41da63)
                                   ^^^^^^^^ 镜像基址
```
或者使用 `tracer.img_base` 获取。

**Q: 返回 'not_injected' 是什么意思？**

A: 说明目标指令执行次数不足，没有达到 `target_kth`。

**Q: 如何处理 hang？**

A: 超时通常意味着程序进入死循环。可以：
1. 增加超时时间
2. 记录为异常结果
3. 使用信号终止进程

**Q: 可以同时注错多个寄存器吗？**

A: 单次运行只能注错一个寄存器。需要多个寄存器注错时，使用多次运行或并行注错。
