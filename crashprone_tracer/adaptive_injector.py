#!/usr/bin/env python3
"""
adaptive_injector.py - 基于 unified_tracer 的自适应故障注入脚本

用法:
    python3 adaptive_injector.py -p ./program -o results/ [-args "arg1 arg2"]

流程:
    1. 运行 unified_tracer.so 识别易崩溃指令及溯源
    2. 从 depth=0 (易崩溃点) 开始注错
    3. 若崩溃率 > 阈值，则将 depth+1 的溯源点加入队列
    4. 循环直到队列为空
"""

import os
import sys
import json
import argparse
import subprocess
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Tuple
from collections import deque
from pathlib import Path


# ============ 数据结构 ============

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
    hang_count: int = 0            # 挂起次数
    benign_count: int = 0          # 良性次数

    @property
    def crash_rate(self) -> float:
        """计算崩溃率"""
        if self.injection_count == 0:
            return 0.0
        return self.crash_count / self.injection_count

    def to_dict(self) -> dict:
        """转换为字典"""
        return {
            'offset': hex(self.offset),
            'register': self.register,
            'disasm': self.disasm,
            'depth': self.depth,
            'parent_offset': hex(self.parent_offset) if self.parent_offset else None,
            'injection_count': self.injection_count,
            'crash_count': self.crash_count,
            'hang_count': self.hang_count,
            'benign_count': self.benign_count,
            'crash_rate': self.crash_rate
        }


@dataclass
class CrashScenario:
    """一个易崩溃指令及其所有寄存器的溯源"""
    crashprone_offset: int         # 易崩溃指令偏移
    crashprone_disasm: str         # 易崩溃指令反汇编
    cp_type: str                   # 易崩溃类型
    exec_count: int                # 执行次数
    crash_regs: List[str]          # 崩溃寄存器列表

    # 每个寄存器的溯源链：register_name → list of sources
    register_traces: Dict[str, List[InjectionTarget]]


# ============ 溯源工具接口 ============

class UnifiedTracer:
    """unified_tracer.so 工具接口"""

    def __init__(self, pin_root: str = "/home/tongshiyu/pin"):
        self.pin_bin = os.path.join(pin_root, "pin")
        self.tracer_so = os.path.join(
            pin_root, "source/tools/pinfi/obj-intel64/crashprone_tracer/unified_tracer.so"
        )

        if not os.path.exists(self.pin_bin):
            raise FileNotFoundError(f"Pin 二进制不存在: {self.pin_bin}")
        if not os.path.exists(self.tracer_so):
            raise FileNotFoundError(f"Tracer 工具不存在: {self.tracer_so}")

    def run(self, program: str, args: List[str], output_file: str,
            depth: int = 5, min_exec: int = 2) -> str:
        """运行 unified_tracer 工具

        Args:
            program: 目标程序路径
            args: 程序参数
            output_file: JSON 输出文件路径
            depth: 最大溯源深度
            min_exec: 最小执行次数过滤

        Returns:
            输出文件路径
        """
        cmd = [
            self.pin_bin,
            "-t", self.tracer_so,
            "-o", output_file,
            "-depth", str(depth),
            "-min_exec", str(min_exec),
            "--", program
        ] + args

        print(f"[Tracer] 运行命令: {' '.join(cmd[:8])}...")

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            if result.returncode != 0:
                print(f"[Tracer] 警告: 程序退出码 {result.returncode}")
            return output_file
        except subprocess.TimeoutExpired:
            print(f"[Tracer] 警告: 运行超时")
            return output_file
        except Exception as e:
            raise RuntimeError(f"运行 tracer 失败: {e}")

    def parse_result(self, json_file: str) -> Tuple[int, List[CrashScenario]]:
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
            offset = int(inst['offset'], 16)

            # 解析每个寄存器的溯源链
            register_traces = {}
            for reg_name, sources in inst.get('register_traces', {}).items():
                trace_list = []
                for src in sources:
                    target = InjectionTarget(
                        offset=int(src['offset'], 16),
                        register=reg_name,
                        disasm=src['disasm'],
                        depth=src['depth'],
                        parent_offset=offset
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


# ============ 自适应注错器 ============

class AdaptiveFaultInjector:
    """自适应故障注入器"""

    def __init__(self, img_base_addr: int,
                 pin_root: str = "/home/tongshiyu/pin",
                 crash_threshold: float = 0.3,
                 injections_per_target: int = 10,
                 timeout: int = 30):
        self.img_base_addr = img_base_addr
        self.pin_bin = os.path.join(pin_root, "pin")
        self.faultinjection_so = os.path.join(
            pin_root, "source/tools/pinfi/obj-intel64/faultinjection.so"
        )
        self.crash_threshold = crash_threshold
        self.injections_per_target = injections_per_target
        self.timeout = timeout

        self.injection_queue = deque()
        self.completed_targets = {}  # (offset, register) -> InjectionTarget
        self.target_map = {}  # 用于快速查找目标

        if not os.path.exists(self.faultinjection_so):
            print(f"[警告] 故障注入工具不存在: {self.faultinjection_so}")
            print(f"[警告] 将使用模拟模式")
            self.simulate_mode = True
        else:
            self.simulate_mode = False

    def initialize_queue(self, scenarios: List[CrashScenario], top_n: int = None):
        """初始化注错队列：添加所有 depth=0 的易崩溃点

        Args:
            scenarios: 崩溃场景列表
            top_n: 只选择执行次数最多的前 N 个（None=全部）
        """
        # 按执行次数排序
        sorted_scenarios = sorted(scenarios, key=lambda s: s.exec_count, reverse=True)
        if top_n:
            sorted_scenarios = sorted_scenarios[:top_n]

        for scenario in sorted_scenarios:
            for reg in scenario.crash_regs:
                target = InjectionTarget(
                    offset=scenario.crashprone_offset,
                    register=reg,
                    disasm=scenario.crashprone_disasm,
                    depth=0,
                    parent_offset=None
                )
                self.injection_queue.append((target, scenario))

        print(f"[初始化] 注错队列中有 {len(self.injection_queue)} 个初始目标")

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
        if self.simulate_mode:
            # 模拟模式：随机返回结果
            import random
            return random.choice(['crash', 'benign', 'benign', 'benign'])

        # 计算绝对地址
        absolute_addr = self.img_base_addr + offset

        # 实际注错
        # 注意：根据你的 faultinjection.so 接口，可能需要使用绝对地址或相对偏移
        cmd = [
            self.pin_bin,
            "-t", self.faultinjection_so,
            "-targetOffset", hex(offset),  # 使用相对偏移
            # 或者使用: "-targetAddr", hex(absolute_addr),  # 使用绝对地址
            "-targetKth", str(kth),
            "-fioption", "AllInst",
            "--", program
        ] + args

        try:
            result = subprocess.run(cmd, timeout=self.timeout, capture_output=True)
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
                              scenarios: List[CrashScenario],
                              max_iterations: int = 100):
        """运行自适应故障注入

        Args:
            program: 目标程序路径
            args: 程序参数
            scenarios: 崩溃场景列表
            max_iterations: 最大迭代次数（防止无限循环）
        """
        self.initialize_queue(scenarios)
        iteration = 0

        while self.injection_queue and iteration < max_iterations:
            iteration += 1
            target, scenario = self.injection_queue.popleft()

            # 检查是否已处理
            key = (target.offset, target.register)
            if key in self.completed_targets:
                continue

            print(f"\n[迭代 {iteration}] [Depth {target.depth}] 注错目标:")
            print(f"  指令: 0x{target.offset:x} - {target.disasm}")
            print(f"  寄存器: {target.register}")

            # 执行注错
            for i in range(1, self.injections_per_target + 1):
                result = self.inject_once(program, args, target.offset,
                                         target.register, i)
                target.injection_count += 1

                if result == 'crash':
                    target.crash_count += 1
                elif result == 'hang':
                    target.hang_count += 1
                else:
                    target.benign_count += 1

            crash_rate = target.crash_rate
            print(f"  结果: 崩溃率 {crash_rate:.1%} " +
                  f"(崩溃:{target.crash_count} 挂起:{target.hang_count} " +
                  f"良性:{target.benign_count})")

            # 保存结果
            self.completed_targets[key] = target

            # 如果崩溃率超过阈值，添加溯源点到队列
            if crash_rate > self.crash_threshold:
                print(f"  → 崩溃率超过阈值 {self.crash_threshold:.1%}，添加溯源点")

                # 查找该寄存器的溯源链
                if target.register in scenario.register_traces:
                    sources = scenario.register_traces[target.register]

                    # 筛选 depth = target.depth + 1 的源指令
                    next_depth = target.depth + 1
                    added = 0
                    for src in sources:
                        if src.depth == next_depth:
                            # 检查是否已处理
                            src_key = (src.offset, src.register)
                            if src_key not in self.completed_targets:
                                print(f"    + 0x{src.offset:x} ({src.register}) depth={src.depth}")
                                self.injection_queue.append((src, scenario))
                                added += 1

                    if added == 0:
                        print(f"    (没有 depth={next_depth} 的未处理溯源点)")
            else:
                print(f"  ✓ 崩溃率低于阈值，不溯源")

        print(f"\n[完成] 注错循环结束，共处理 {len(self.completed_targets)} 个目标")

    def save_results(self, output_file: str):
        """保存注错结果到 JSON"""
        results = {
            'config': {
                'img_base_addr': hex(self.img_base_addr),
                'crash_threshold': self.crash_threshold,
                'injections_per_target': self.injections_per_target,
                'timeout': self.timeout
            },
            'targets': [target.to_dict() for target in self.completed_targets.values()],
            'summary': {
                'total_targets': len(self.completed_targets),
                'total_injections': sum(t.injection_count for t in self.completed_targets.values()),
                'total_crashes': sum(t.crash_count for t in self.completed_targets.values()),
                'overall_crash_rate': sum(t.crash_count for t in self.completed_targets.values()) /
                                     sum(t.injection_count for t in self.completed_targets.values())
                                     if self.completed_targets else 0
            }
        }

        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2, ensure_ascii=False)

        print(f"[保存] 注错结果已保存到: {output_file}")


# ============ 主函数 ============

def main():
    parser = argparse.ArgumentParser(description='自适应故障注入工具')
    parser.add_argument('-p', '--program', required=True, help='目标程序路径')
    parser.add_argument('-args', '--program-args', default='', help='程序参数（用引号括起）')
    parser.add_argument('-o', '--output-dir', default='./adaptive_results', help='输出目录')
    parser.add_argument('--pin-root', default='/home/tongshiyu/pin', help='Pin 根目录')
    parser.add_argument('--depth', type=int, default=5, help='最大溯源深度')
    parser.add_argument('--threshold', type=float, default=0.3, help='崩溃率阈值')
    parser.add_argument('--injections', type=int, default=10, help='每目标注错次数')
    parser.add_argument('--top-n', type=int, help='只处理执行次数最多的前 N 个指令')

    args = parser.parse_args()

    # 创建输出目录
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    # 解析程序参数
    program_args = args.program_args.split() if args.program_args else []

    print("=" * 60)
    print("自适应故障注入工具")
    print("=" * 60)
    print(f"目标程序: {args.program}")
    print(f"程序参数: {' '.join(program_args) if program_args else '(无)'}")
    print(f"输出目录: {output_dir}")
    print(f"崩溃率阈值: {args.threshold:.1%}")
    print(f"每目标注错: {args.injections} 次")
    print()

    # 步骤 1: 运行溯源工具
    print("[Step 1] 运行易崩溃指令溯源...")
    tracer = UnifiedTracer(pin_root=args.pin_root)
    trace_file = output_dir / "trace_result.json"

    tracer.run(args.program, program_args, str(trace_file),
               depth=args.depth, min_exec=2)

    # 步骤 2: 解析结果
    print("\n[Step 2] 解析溯源结果...")
    img_base_addr, scenarios = tracer.parse_result(str(trace_file))
    print(f"  主镜像基地址: 0x{img_base_addr:x}")
    print(f"  发现 {len(scenarios)} 个崩溃场景")

    if not scenarios:
        print("[错误] 没有发现易崩溃指令，退出")
        sys.exit(1)

    # 显示统计
    total_crash_regs = sum(len(s.crash_regs) for s in scenarios)
    total_sources = sum(len(sources) for s in scenarios
                       for sources in s.register_traces.values())
    print(f"  总崩溃寄存器: {total_crash_regs}")
    print(f"  总溯源指令: {total_sources}")

    # 步骤 3: 自适应注错
    print("\n[Step 3] 开始自适应故障注入...")
    injector = AdaptiveFaultInjector(
        img_base_addr=img_base_addr,
        pin_root=args.pin_root,
        crash_threshold=args.threshold,
        injections_per_target=args.injections
    )

    injector.run_adaptive_injection(args.program, program_args, scenarios)

    # 保存结果
    result_file = output_dir / "injection_results.json"
    injector.save_results(str(result_file))

    print("\n" + "=" * 60)
    print("注错完成！")
    print("=" * 60)
    print(f"总目标数: {len(injector.completed_targets)}")
    print(f"总注错次数: {sum(t.injection_count for t in injector.completed_targets.values())}")
    print(f"总崩溃次数: {sum(t.crash_count for t in injector.completed_targets.values())}")
    print(f"总体崩溃率: {injector.save_results.__self__.completed_targets}")


if __name__ == "__main__":
    main()
