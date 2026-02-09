/*
 * function_profiler.cpp - 函数维度剖析工具
 *
 * 功能：对应用程序进行函数级别的执行特性剖析
 * 目的：为学术研究提供函数特征数据，分析修复成功的决定因素
 *
 * 使用方法：
 *   pin -t function_profiler.so -o output.json -- <program> [args]
 *   pin -t function_profiler.so -min_calls 5 -o output.json -- <program>
 *
 * 输出格式：JSON
 */

#include "function_profiler.h"
#include "../utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

using std::cerr;
using std::endl;
using std::ofstream;
using std::stringstream;

// ========== 全局变量 ==========

// 函数剖析数据表：函数起始地址 -> FunctionProfile
map<ADDRINT, FunctionProfile> g_function_profiles;

// 主程序镜像信息
string g_main_img_name = "";
ADDRINT g_main_img_low = 0;
ADDRINT g_main_img_high = 0;

// 全局统计
UINT64 g_total_inst_executed = 0;

// 线程安全锁
PIN_LOCK g_lock;

// ========== KNOB 参数定义 ==========

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "function_profile.json", "输出JSON文件路径");

KNOB<int> KnobMinCallCount(KNOB_MODE_WRITEONCE, "pintool",
    "min_calls", "1", "最小调用次数过滤(只输出调用次数>=此值的函数)");

KNOB<BOOL> KnobEnableDep(KNOB_MODE_WRITEONCE, "pintool",
    "enable_dep", "0", "启用F类数据依赖分析(有性能开销)");

KNOB<BOOL> KnobEnableLifetime(KNOB_MODE_WRITEONCE, "pintool",
    "enable_lifetime", "0", "启用G类生命周期分析(有性能开销)");

// ========== 辅助函数 ==========

/**
 * JSON转义函数
 */
string escape_json(const string& s) {
    string result;
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

/**
 * 地址转十六进制字符串
 */
string addr_to_hex(ADDRINT addr) {
    stringstream ss;
    ss << "0x" << std::hex << addr;
    return ss.str();
}

/**
 * 判断是否为算术指令
 * 包括：ADD/SUB/MUL/DIV/INC/DEC/NEG/ADC/SBB等
 */
bool IsArithmeticInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    // 整数算术运算
    if (mnemonic.find("ADD") != string::npos ||
        mnemonic.find("SUB") != string::npos ||
        mnemonic.find("MUL") != string::npos ||
        mnemonic.find("DIV") != string::npos ||
        mnemonic.find("INC") != string::npos ||
        mnemonic.find("DEC") != string::npos ||
        mnemonic.find("NEG") != string::npos ||
        mnemonic.find("ADC") != string::npos ||
        mnemonic.find("SBB") != string::npos ||
        mnemonic.find("IMUL") != string::npos ||
        mnemonic.find("IDIV") != string::npos) {
        return true;
    }
    return false;
}

/**
 * 判断是否为逻辑指令
 * 包括：AND/OR/XOR/NOT/SHL/SHR/SAL/SAR/ROL/ROR等
 */
bool IsLogicInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    if (mnemonic.find("AND") != string::npos ||
        mnemonic.find("OR") != string::npos ||
        mnemonic.find("XOR") != string::npos ||
        mnemonic.find("NOT") != string::npos ||
        mnemonic.find("SHL") != string::npos ||
        mnemonic.find("SHR") != string::npos ||
        mnemonic.find("SAL") != string::npos ||
        mnemonic.find("SAR") != string::npos ||
        mnemonic.find("ROL") != string::npos ||
        mnemonic.find("ROR") != string::npos ||
        mnemonic.find("RCL") != string::npos ||
        mnemonic.find("RCR") != string::npos) {
        return true;
    }
    return false;
}

/**
 * 判断是否为浮点指令
 * 包括：所有以F开头的x87指令，以及SSE/AVX浮点指令
 */
bool IsFloatInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    // x87浮点指令(以F开头，但排除一些非浮点指令)
    if (mnemonic[0] == 'F' && mnemonic.find("FENCE") == string::npos) {
        return true;
    }
    // SSE/AVX浮点指令(以SS/SD/PS/PD结尾的通常是浮点)
    if (mnemonic.find("SS") != string::npos ||
        mnemonic.find("SD") != string::npos ||
        mnemonic.find("PS") != string::npos ||
        mnemonic.find("PD") != string::npos) {
        // 排除一些非浮点指令
        if (mnemonic.find("CVTT") != string::npos ||
            mnemonic.find("CVT") != string::npos ||
            mnemonic.find("ADD") != string::npos ||
            mnemonic.find("SUB") != string::npos ||
            mnemonic.find("MUL") != string::npos ||
            mnemonic.find("DIV") != string::npos ||
            mnemonic.find("SQRT") != string::npos ||
            mnemonic.find("MAX") != string::npos ||
            mnemonic.find("MIN") != string::npos ||
            mnemonic.find("MOV") != string::npos ||
            mnemonic.find("CMP") != string::npos) {
            return true;
        }
    }
    return false;
}

/**
 * 判断是否为SIMD/向量指令
 * 包括：SSE/AVX/MMX等向量指令
 */
bool IsSIMDInstruction(INS ins) {
    // 检查是否使用XMM/YMM/ZMM/MMX寄存器
    for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
        REG reg = INS_RegR(ins, i);
        if (REG_is_xmm(reg) || REG_is_ymm(reg) || REG_is_zmm(reg) || REG_is_mm(reg)) {
            return true;
        }
    }
    for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
        REG reg = INS_RegW(ins, i);
        if (REG_is_xmm(reg) || REG_is_ymm(reg) || REG_is_zmm(reg) || REG_is_mm(reg)) {
            return true;
        }
    }
    return false;
}

/**
 * 判断是否为数据移动指令
 * 包括：MOV/MOVSX/MOVZX/LEA/XCHG/PUSH/POP等
 */
bool IsDataMovementInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    // MOV系列指令
    if (mnemonic.find("MOV") != string::npos ||
        mnemonic.find("LEA") != string::npos ||
        mnemonic.find("XCHG") != string::npos ||
        mnemonic.find("PUSH") != string::npos ||
        mnemonic.find("POP") != string::npos ||
        mnemonic.find("BSWAP") != string::npos ||
        mnemonic.find("CBW") != string::npos ||
        mnemonic.find("CWD") != string::npos ||
        mnemonic.find("CDQ") != string::npos ||
        mnemonic.find("CQO") != string::npos) {
        return true;
    }
    return false;
}

/**
 * 判断是否为纯计算指令（不涉及内存访问）
 * 纯计算 = (算术 || 逻辑 || 浮点 || SIMD) && !内存访问
 */
bool IsPureComputeInstruction(INS ins) {
    // 首先检查是否有内存操作
    if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        return false;
    }
    // 然后检查是否是计算类指令
    return IsArithmeticInstruction(ins) ||
           IsLogicInstruction(ins) ||
           IsFloatInstruction(ins) ||
           IsSIMDInstruction(ins);
}

/**
 * 判断是否应该跳过该函数
 * 过滤系统函数和框架函数
 */
bool ShouldSkipFunction(const string& func_name) {
    // 过滤系统函数前缀
    if (func_name.find("__libc") == 0 ||
        func_name.find("_start") == 0 ||
        func_name.find("call_gmon_start") == 0 ||
        func_name.find("frame_dummy") == 0 ||
        func_name.find("__do_global") == 0 ||
        func_name.find("__stat") == 0 ||
        func_name.find("_init") == 0 ||
        func_name.find("_fini") == 0 ||
        func_name.empty()) {
        return true;
    }
    return false;
}

// ========== 动态分析回调函数 ==========

/**
 * 函数入口回调：统计调用次数
 */
VOID FunctionEntry(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->call_exec), 1);
}

/**
 * 指令执行回调：统计总指令数
 */
VOID CountInstruction(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->inst_exec), 1);
    __sync_fetch_and_add(&g_total_inst_executed, 1);
}

// ========== 内存访问模式分析阈值 ==========
#define SEQUENTIAL_THRESHOLD 64   // ±64字节内视为连续（缓存行大小）
#define STRIDE_VARIANCE_THRESHOLD 8  // stride变化在8字节内视为步长访问

/**
 * 内存读访问模式分析回调
 * 统计读次数并分析访问模式（连续/步长/随机）
 */
VOID AnalyzeMemoryReadPattern(FunctionProfile* profile, ADDRINT addr) {
    __sync_fetch_and_add(&(profile->mem_read_exec), 1);

    if (profile->has_last_read) {
        INT64 stride = (INT64)addr - (INT64)profile->last_read_addr;
        INT64 abs_stride = stride < 0 ? -stride : stride;

        if (abs_stride <= SEQUENTIAL_THRESHOLD) {
            // 连续访问：地址差在缓存行范围内
            __sync_fetch_and_add(&(profile->seq_read_exec), 1);
        } else {
            // 检查stride是否稳定（与上次stride相近）
            INT64 stride_diff = stride - profile->last_read_stride;
            if (stride_diff < 0) stride_diff = -stride_diff;

            if (stride_diff <= STRIDE_VARIANCE_THRESHOLD) {
                // 步长访问：stride稳定
                __sync_fetch_and_add(&(profile->stride_read_exec), 1);
            } else {
                // 随机访问：stride变化大
                __sync_fetch_and_add(&(profile->random_read_exec), 1);
            }
        }
        profile->last_read_stride = stride;
    }

    profile->last_read_addr = addr;
    profile->has_last_read = true;
}

/**
 * 内存写访问模式分析回调
 * 统计写次数并分析访问模式（连续/步长/随机）
 */
VOID AnalyzeMemoryWritePattern(FunctionProfile* profile, ADDRINT addr) {
    __sync_fetch_and_add(&(profile->mem_write_exec), 1);

    if (profile->has_last_write) {
        INT64 stride = (INT64)addr - (INT64)profile->last_write_addr;
        INT64 abs_stride = stride < 0 ? -stride : stride;

        if (abs_stride <= SEQUENTIAL_THRESHOLD) {
            // 连续访问
            __sync_fetch_and_add(&(profile->seq_write_exec), 1);
        } else {
            // 检查stride是否稳定
            INT64 stride_diff = stride - profile->last_write_stride;
            if (stride_diff < 0) stride_diff = -stride_diff;

            if (stride_diff <= STRIDE_VARIANCE_THRESHOLD) {
                // 步长访问
                __sync_fetch_and_add(&(profile->stride_write_exec), 1);
            } else {
                // 随机访问
                __sync_fetch_and_add(&(profile->random_write_exec), 1);
            }
        }
        profile->last_write_stride = stride;
    }

    profile->last_write_addr = addr;
    profile->has_last_write = true;
}

/**
 * 访存指令执行回调(不重复计数)
 */
VOID CountMemoryInstExec(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->mem_inst_exec), 1);
}

/**
 * 分支执行回调
 */
VOID CountBranch(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->branch_exec), 1);
}

/**
 * 间接跳转回调
 */
VOID CountIndirectBranch(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->indirect_exec), 1);
}

/**
 * 算术指令执行回调
 */
VOID CountArithmeticExec(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->arith_exec), 1);
}

/**
 * 浮点指令执行回调
 */
VOID CountFloatExec(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->float_exec), 1);
}

/**
 * 逻辑指令执行回调
 */
VOID CountLogicExec(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->logic_exec), 1);
}

/**
 * SIMD指令执行回调
 */
VOID CountSIMDExec(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->simd_exec), 1);
}

/**
 * 纯计算指令执行回调
 */
VOID CountPureComputeExec(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->pure_compute_exec), 1);
}

/**
 * 数据移动指令执行回调
 */
VOID CountDataMovementExec(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->data_movement_exec), 1);
}

// ========== D类: 寄存器使用回调 ==========

/**
 * 寄存器操作计数回调
 */
VOID CountRegisterOps(FunctionProfile* profile, UINT32 num_reads, UINT32 num_writes) {
    __sync_fetch_and_add(&(profile->reg_read_exec), num_reads);
    __sync_fetch_and_add(&(profile->reg_write_exec), num_writes);
}

// ========== E类: 控制流细化回调 ==========

/**
 * 分支方向统计回调
 */
VOID CountBranchDirection(FunctionProfile* profile, BOOL taken) {
    if (taken) {
        __sync_fetch_and_add(&(profile->branch_taken_exec), 1);
    } else {
        __sync_fetch_and_add(&(profile->branch_not_taken_exec), 1);
    }
}

/**
 * 循环迭代计数回调（回边执行时调用）
 */
VOID CountLoopIteration(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->loop_iter_total), 1);
}

/**
 * 调用深度跟踪回调 - 函数调用
 */
VOID TrackCallDepthEnter(FunctionProfile* profile) {
    UINT32 new_depth = __sync_add_and_fetch(&(profile->current_call_depth), 1);
    // 更新最大深度（无锁方式）
    UINT32 old_max = profile->call_depth_max;
    while (new_depth > old_max) {
        if (__sync_bool_compare_and_swap(&(profile->call_depth_max), old_max, new_depth)) {
            break;
        }
        old_max = profile->call_depth_max;
    }
}

/**
 * 调用深度跟踪回调 - 函数返回
 */
VOID TrackCallDepthExit(FunctionProfile* profile) {
    if (profile->current_call_depth > 0) {
        __sync_fetch_and_sub(&(profile->current_call_depth), 1);
    }
}

// ========== F类: 数据依赖回调 ==========

/**
 * 将Pin寄存器映射到0-15的通用寄存器索引
 * 返回-1表示不是通用寄存器
 */
inline INT32 MapRegToIndex(REG reg) {
    // 64位通用寄存器
    if (reg >= REG_RAX && reg <= REG_R15) {
        return reg - REG_RAX;
    }
    // 32位通用寄存器映射到对应的64位
    if (reg >= REG_EAX && reg <= REG_R15D) {
        return reg - REG_EAX;
    }
    // 16位通用寄存器
    if (reg >= REG_AX && reg <= REG_R15W) {
        return reg - REG_AX;
    }
    // 8位低位寄存器 (AL, CL, DL, BL, SPL, BPL, SIL, DIL, R8B-R15B)
    if (reg >= REG_AL && reg <= REG_R15B) {
        return reg - REG_AL;
    }
    return -1;  // 不是通用寄存器
}

/**
 * 数据依赖跟踪回调
 * 统计定义-使用对和依赖链长度
 */
VOID TrackDataDependency(FunctionProfile* profile,
                         REG read_reg1, REG read_reg2, REG read_reg3, REG read_reg4,
                         REG write_reg1, REG write_reg2) {
    profile->current_dyn_id++;
    UINT64 current_id = profile->current_dyn_id;

    // 处理读寄存器（最多4个）
    REG read_regs[4] = {read_reg1, read_reg2, read_reg3, read_reg4};
    for (int i = 0; i < 4; i++) {
        if (read_regs[i] == REG_INVALID()) continue;
        INT32 idx = MapRegToIndex(read_regs[i]);
        if (idx >= 0 && idx < 16) {
            UINT64 def_id = profile->reg_last_def_id[idx];
            if (def_id > 0) {
                __sync_fetch_and_add(&(profile->def_use_pairs), 1);
                // 计算依赖链长度
                UINT32 chain_len = (UINT32)(current_id - def_id);
                UINT32 old_max = profile->reg_dep_chain_max;
                while (chain_len > old_max) {
                    if (__sync_bool_compare_and_swap(&(profile->reg_dep_chain_max), old_max, chain_len)) {
                        break;
                    }
                    old_max = profile->reg_dep_chain_max;
                }
            }
        }
    }

    // 处理写寄存器（最多2个）
    REG write_regs[2] = {write_reg1, write_reg2};
    for (int i = 0; i < 2; i++) {
        if (write_regs[i] == REG_INVALID()) continue;
        INT32 idx = MapRegToIndex(write_regs[i]);
        if (idx >= 0 && idx < 16) {
            profile->reg_last_def_id[idx] = current_id;
        }
    }
}

/**
 * 内存到寄存器传递计数
 */
VOID CountMemToReg(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->mem_to_reg_exec), 1);
}

/**
 * 寄存器到内存传递计数
 */
VOID CountRegToMem(FunctionProfile* profile) {
    __sync_fetch_and_add(&(profile->reg_to_mem_exec), 1);
}

// ========== G类: 生命周期回调 ==========

/**
 * 寄存器生命周期跟踪回调
 */
VOID TrackRegisterLifetime(FunctionProfile* profile,
                           REG read_reg1, REG read_reg2, REG read_reg3, REG read_reg4,
                           REG write_reg1, REG write_reg2) {
    UINT64 current_id = profile->current_dyn_id;  // 使用F类已更新的ID

    // 处理读寄存器：计算生命周期
    REG read_regs[4] = {read_reg1, read_reg2, read_reg3, read_reg4};
    for (int i = 0; i < 4; i++) {
        if (read_regs[i] == REG_INVALID()) continue;
        INT32 idx = MapRegToIndex(read_regs[i]);
        if (idx >= 0 && idx < 16) {
            UINT64 def_id = profile->reg_def_id[idx];
            if (def_id > 0) {
                UINT64 lifetime = current_id - def_id;
                __sync_fetch_and_add(&(profile->reg_lifetime_total), lifetime);

                // 首次使用距离
                if (!profile->reg_was_used[idx]) {
                    __sync_fetch_and_add(&(profile->first_use_dist_total), lifetime);
                    profile->reg_was_used[idx] = true;
                }
            }
        }
    }

    // 处理写寄存器：检测死写
    REG write_regs[2] = {write_reg1, write_reg2};
    for (int i = 0; i < 2; i++) {
        if (write_regs[i] == REG_INVALID()) continue;
        INT32 idx = MapRegToIndex(write_regs[i]);
        if (idx >= 0 && idx < 16) {
            // 如果上次写入后未被读取，是死写
            if (profile->reg_def_id[idx] > 0 && !profile->reg_was_used[idx]) {
                __sync_fetch_and_add(&(profile->dead_write_exec), 1);
            }
            profile->reg_def_id[idx] = current_id;
            profile->reg_was_used[idx] = false;
        }
    }
}

// ========== 静态分析函数 ==========

/**
 * 静态分析单条指令
 * 在插桩时调用，收集静态特征
 */
VOID AnalyzeStaticInstruction(INS ins, FunctionProfile& profile) {
    ADDRINT pc = INS_Address(ins);

    // 统计静态指令数
    profile.inst_static++;

    // ========== D类: 寄存器使用静态统计 ==========
    // 统计寄存器读操作数
    UINT32 num_rregs = INS_MaxNumRRegs(ins);
    profile.reg_read_static += num_rregs;
    for (UINT32 i = 0; i < num_rregs; i++) {
        REG reg = INS_RegR(ins, i);
        if (REG_is_gr(REG_FullRegName(reg))) {
            profile.read_regs_set.insert(REG_FullRegName(reg));
        }
    }

    // 统计寄存器写操作数
    UINT32 num_wregs = INS_MaxNumWRegs(ins);
    profile.reg_write_static += num_wregs;
    for (UINT32 i = 0; i < num_wregs; i++) {
        REG reg = INS_RegW(ins, i);
        if (REG_is_gr(REG_FullRegName(reg))) {
            profile.write_regs_set.insert(REG_FullRegName(reg));
        }
    }

    // ========== C类 & E类: 分支指令统计 ==========
    if (INS_IsBranch(ins)) {
        profile.branch_static++;

        // E类: 区分条件/无条件分支
        if (INS_HasFallThrough(ins)) {
            profile.cond_branch_static++;
        } else {
            profile.uncond_branch_static++;
        }

        // 检测循环(回边：target < pc 且 target在函数范围内)
        if (INS_IsDirectControlFlow(ins)) {
            ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
            if (target < pc && target >= profile.start_addr && target < profile.end_addr) {
                profile.loop_static++;
            }
        }
    }

    // 函数调用
    if (INS_IsCall(ins)) {
        profile.call_static++;
    }

    // 返回指令
    if (INS_IsRet(ins)) {
        profile.return_static++;
    }

    // ========== B1类: 内存访问静态统计 ==========
    bool is_mem_read = INS_IsMemoryRead(ins);
    bool is_mem_write = INS_IsMemoryWrite(ins);
    if (is_mem_read) {
        profile.mem_read_static++;
    }
    if (is_mem_write) {
        profile.mem_write_static++;
    }
    if (is_mem_read || is_mem_write) {
        profile.mem_inst_static++;
    }

    // ========== 计算特性统计 ==========
    // 算术指令
    if (IsArithmeticInstruction(ins)) {
        profile.arith_static++;
    }

    // 逻辑指令
    if (IsLogicInstruction(ins)) {
        profile.logic_static++;
    }

    // 浮点指令
    if (IsFloatInstruction(ins)) {
        profile.float_static++;
    }

    // SIMD/向量指令
    if (IsSIMDInstruction(ins)) {
        profile.simd_static++;
    }

    // 纯计算指令（不涉及内存）
    if (IsPureComputeInstruction(ins)) {
        profile.pure_compute_static++;
    }

    // 数据移动指令
    if (IsDataMovementInstruction(ins)) {
        profile.data_movement_static++;
    }
}

/**
 * 动态插桩
 * 在插桩时为指令插入运行时回调
 */
VOID InstrumentDynamicAnalysis(INS ins, FunctionProfile* profile, ADDRINT pc) {
    // 所有指令：统计执行次数
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountInstruction,
                   IARG_PTR, profile,
                   IARG_END);

    // 内存读 - 使用地址分析访问模式
    if (INS_IsMemoryRead(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeMemoryReadPattern,
                       IARG_PTR, profile,
                       IARG_MEMORYREAD_EA,
                       IARG_END);
    }

    // 内存写 - 使用地址分析访问模式
    if (INS_IsMemoryWrite(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeMemoryWritePattern,
                       IARG_PTR, profile,
                       IARG_MEMORYWRITE_EA,
                       IARG_END);
    }

    // 访存指令执行(不重复计数：一条指令同时读写只计一次)
    if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountMemoryInstExec,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // 分支
    if (INS_IsBranch(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountBranch,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // 间接跳转
    if (INS_IsIndirectControlFlow(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountIndirectBranch,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // ========== 计算特性动态统计 ==========
    // 算术指令执行
    if (IsArithmeticInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountArithmeticExec,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // 浮点指令执行
    if (IsFloatInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountFloatExec,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // 逻辑指令执行
    if (IsLogicInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountLogicExec,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // SIMD指令执行
    if (IsSIMDInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountSIMDExec,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // 纯计算指令执行
    if (IsPureComputeInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountPureComputeExec,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // 数据移动指令执行
    if (IsDataMovementInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountDataMovementExec,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // ========== D类: 寄存器使用动态统计 ==========
    UINT32 num_rregs = INS_MaxNumRRegs(ins);
    UINT32 num_wregs = INS_MaxNumWRegs(ins);
    if (num_rregs > 0 || num_wregs > 0) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountRegisterOps,
                       IARG_PTR, profile,
                       IARG_UINT32, num_rregs,
                       IARG_UINT32, num_wregs,
                       IARG_END);
    }

    // ========== E类: 控制流细化动态统计 ==========
    // 分支方向统计（只对条件分支）
    if (INS_IsBranch(ins) && INS_HasFallThrough(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountBranchDirection,
                       IARG_PTR, profile,
                       IARG_BRANCH_TAKEN,
                       IARG_END);
    }

    // 循环迭代统计（回边）
    if (INS_IsBranch(ins) && INS_IsDirectControlFlow(ins)) {
        ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
        if (target < pc && target >= profile->start_addr && target < profile->end_addr) {
            // 这是一个回边，统计循环迭代
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountLoopIteration,
                           IARG_PTR, profile,
                           IARG_END);
        }
    }

    // 调用深度跟踪
    if (INS_IsCall(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TrackCallDepthEnter,
                       IARG_PTR, profile,
                       IARG_END);
    }
    if (INS_IsRet(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TrackCallDepthExit,
                       IARG_PTR, profile,
                       IARG_END);
    }

    // ========== F类: 数据依赖 (可选) ==========
    if (KnobEnableDep.Value()) {
        // 获取读写寄存器（最多4读2写）
        REG read_regs[4] = {REG_INVALID(), REG_INVALID(), REG_INVALID(), REG_INVALID()};
        REG write_regs[2] = {REG_INVALID(), REG_INVALID()};

        for (UINT32 i = 0; i < num_rregs && i < 4; i++) {
            read_regs[i] = INS_RegR(ins, i);
        }
        for (UINT32 i = 0; i < num_wregs && i < 2; i++) {
            write_regs[i] = INS_RegW(ins, i);
        }

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TrackDataDependency,
                       IARG_PTR, profile,
                       IARG_UINT32, read_regs[0],
                       IARG_UINT32, read_regs[1],
                       IARG_UINT32, read_regs[2],
                       IARG_UINT32, read_regs[3],
                       IARG_UINT32, write_regs[0],
                       IARG_UINT32, write_regs[1],
                       IARG_END);

        // 内存到寄存器传递
        if (INS_IsMemoryRead(ins) && num_wregs > 0) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountMemToReg,
                           IARG_PTR, profile,
                           IARG_END);
        }

        // 寄存器到内存传递
        if (INS_IsMemoryWrite(ins) && num_rregs > 0) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountRegToMem,
                           IARG_PTR, profile,
                           IARG_END);
        }
    }

    // ========== G类: 生命周期 (可选) ==========
    if (KnobEnableLifetime.Value()) {
        // 获取读写寄存器（最多4读2写）
        REG read_regs[4] = {REG_INVALID(), REG_INVALID(), REG_INVALID(), REG_INVALID()};
        REG write_regs[2] = {REG_INVALID(), REG_INVALID()};

        for (UINT32 i = 0; i < num_rregs && i < 4; i++) {
            read_regs[i] = INS_RegR(ins, i);
        }
        for (UINT32 i = 0; i < num_wregs && i < 2; i++) {
            write_regs[i] = INS_RegW(ins, i);
        }

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)TrackRegisterLifetime,
                       IARG_PTR, profile,
                       IARG_UINT32, read_regs[0],
                       IARG_UINT32, read_regs[1],
                       IARG_UINT32, read_regs[2],
                       IARG_UINT32, read_regs[3],
                       IARG_UINT32, write_regs[0],
                       IARG_UINT32, write_regs[1],
                       IARG_END);
    }
}

// ========== 镜像加载回调 ==========

/**
 * 镜像加载回调：主插桩逻辑
 */
VOID ImageLoad(IMG img, VOID *v) {
    // 只分析主程序
    if (!IMG_IsMainExecutable(img)) {
        return;
    }

    // 记录主程序信息
    g_main_img_name = IMG_Name(img);
    g_main_img_low = IMG_LowAddress(img);
    g_main_img_high = IMG_HighAddress(img);

    cerr << "[Function Profiler] Analyzing main image: " << g_main_img_name << endl;
    cerr << "[Function Profiler] Base address: " << addr_to_hex(g_main_img_low) << endl;

    // 遍历所有Section
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        // 只分析.text段
        if (SEC_Name(sec) != ".text") {
            continue;
        }

        // 遍历所有函数(Routine)
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            string func_name = RTN_Name(rtn);

            // 过滤系统函数
            if (ShouldSkipFunction(func_name)) {
                continue;
            }

            RTN_Open(rtn);

            // 初始化函数剖析数据
            ADDRINT rtn_addr = RTN_Address(rtn);
            FunctionProfile& profile = g_function_profiles[rtn_addr];

            // 基本信息
            profile.function_name = func_name;
            profile.start_addr = rtn_addr;
            profile.end_addr = rtn_addr + RTN_Size(rtn);
            profile.offset_start = rtn_addr - g_main_img_low;
            profile.offset_end = profile.offset_start + RTN_Size(rtn);
            profile.function_size_bytes = RTN_Size(rtn);

            // 插入函数入口回调(统计调用次数)
            RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)FunctionEntry,
                          IARG_PTR, &profile,
                          IARG_END);

            // 静态分析并动态插桩每条指令
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                ADDRINT pc = INS_Address(ins);

                // 静态分析
                AnalyzeStaticInstruction(ins, profile);

                // 动态插桩
                InstrumentDynamicAnalysis(ins, &profile, pc);
            }

            // D类: 计算unique寄存器数量
            profile.unique_reg_read = profile.read_regs_set.size();
            profile.unique_reg_write = profile.write_regs_set.size();

            RTN_Close(rtn);
        }
    }
}

// ========== 程序结束回调 ==========

/**
 * Fini回调：计算派生指标并输出JSON
 */
VOID Fini(INT32 code, VOID *v) {
    cerr << "[Function Profiler] Program finished. Writing results..." << endl;

    // 打开输出文件
    ofstream outFile(KnobOutputFile.Value().c_str());
    if (!outFile.is_open()) {
        cerr << "[ERROR] Cannot open output file: " << KnobOutputFile.Value() << endl;
        return;
    }

    int min_calls = KnobMinCallCount.Value();

    // 写入JSON（不再计算派生指标，只记录原始数据）
    outFile << "{\n";

    // 工具信息
    outFile << "  \"tool_info\": {\n";
    outFile << "    \"name\": \"Function Profiler\",\n";
    outFile << "    \"version\": \"1.0\",\n";
    outFile << "    \"description\": \"函数维度的执行特性剖析工具\",\n";
    outFile << "    \"main_image\": \"" << escape_json(g_main_img_name) << "\",\n";
    outFile << "    \"base_address\": \"" << addr_to_hex(g_main_img_low) << "\"\n";
    outFile << "  },\n";

    // 函数数据
    outFile << "  \"functions\": [\n";

    bool first_func = true;
    int func_count = 0;

    for (const auto& kv : g_function_profiles) {
        const FunctionProfile& profile = kv.second;

        // 过滤：只输出调用次数满足条件的函数
        if (profile.call_exec < (UINT64)min_calls) {
            continue;
        }

        if (!first_func) {
            outFile << ",\n";
        }
        first_func = false;
        func_count++;

        outFile << "    {\n";

        // 基本信息
        outFile << "      \"function_name\": \"" << escape_json(profile.function_name) << "\",\n";
        outFile << "      \"start_addr\": \"" << addr_to_hex(profile.start_addr) << "\",\n";
        outFile << "      \"end_addr\": \"" << addr_to_hex(profile.end_addr) << "\",\n";
        outFile << "      \"offset_start\": \"" << addr_to_hex(profile.offset_start) << "\",\n";
        outFile << "      \"offset_end\": \"" << addr_to_hex(profile.offset_end) << "\",\n";
        outFile << "      \"function_size_bytes\": " << profile.function_size_bytes << ",\n";

        // A类：执行统计
        outFile << "      \"execution_stats\": {\n";
        outFile << "        \"call_exec\": " << profile.call_exec << ",\n";
        outFile << "        \"inst_exec\": " << profile.inst_exec << ",\n";
        outFile << "        \"inst_static\": " << profile.inst_static << "\n";
        outFile << "      },\n";

        // B1类：数据流
        outFile << "      \"data_flow\": {\n";
        outFile << "        \"mem_read_static\": " << profile.mem_read_static << ",\n";
        outFile << "        \"mem_write_static\": " << profile.mem_write_static << ",\n";
        outFile << "        \"mem_inst_static\": " << profile.mem_inst_static << ",\n";
        outFile << "        \"mem_read_exec\": " << profile.mem_read_exec << ",\n";
        outFile << "        \"mem_write_exec\": " << profile.mem_write_exec << ",\n";
        outFile << "        \"mem_inst_exec\": " << profile.mem_inst_exec << "\n";
        outFile << "      },\n";

        // B1.5类：内存访问模式
        outFile << "      \"memory_access_pattern\": {\n";
        outFile << "        \"seq_read_exec\": " << profile.seq_read_exec << ",\n";
        outFile << "        \"stride_read_exec\": " << profile.stride_read_exec << ",\n";
        outFile << "        \"random_read_exec\": " << profile.random_read_exec << ",\n";
        outFile << "        \"seq_write_exec\": " << profile.seq_write_exec << ",\n";
        outFile << "        \"stride_write_exec\": " << profile.stride_write_exec << ",\n";
        outFile << "        \"random_write_exec\": " << profile.random_write_exec << "\n";
        outFile << "      },\n";

        // B2类：计算特性
        outFile << "      \"compute_characteristics\": {\n";
        outFile << "        \"arith_static\": " << profile.arith_static << ",\n";
        outFile << "        \"logic_static\": " << profile.logic_static << ",\n";
        outFile << "        \"float_static\": " << profile.float_static << ",\n";
        outFile << "        \"simd_static\": " << profile.simd_static << ",\n";
        outFile << "        \"pure_compute_static\": " << profile.pure_compute_static << ",\n";
        outFile << "        \"data_movement_static\": " << profile.data_movement_static << ",\n";
        outFile << "        \"arith_exec\": " << profile.arith_exec << ",\n";
        outFile << "        \"logic_exec\": " << profile.logic_exec << ",\n";
        outFile << "        \"float_exec\": " << profile.float_exec << ",\n";
        outFile << "        \"simd_exec\": " << profile.simd_exec << ",\n";
        outFile << "        \"pure_compute_exec\": " << profile.pure_compute_exec << ",\n";
        outFile << "        \"data_movement_exec\": " << profile.data_movement_exec << "\n";
        outFile << "      },\n";

        // C类：控制流
        outFile << "      \"control_flow\": {\n";
        outFile << "        \"branch_static\": " << profile.branch_static << ",\n";
        outFile << "        \"branch_exec\": " << profile.branch_exec << ",\n";
        outFile << "        \"loop_static\": " << profile.loop_static << ",\n";
        outFile << "        \"return_static\": " << profile.return_static << ",\n";
        outFile << "        \"call_static\": " << profile.call_static << ",\n";
        outFile << "        \"indirect_exec\": " << profile.indirect_exec << "\n";
        outFile << "      },\n";

        // D类：寄存器使用
        outFile << "      \"register_usage\": {\n";
        outFile << "        \"reg_read_exec\": " << profile.reg_read_exec << ",\n";
        outFile << "        \"reg_write_exec\": " << profile.reg_write_exec << ",\n";
        outFile << "        \"reg_read_static\": " << profile.reg_read_static << ",\n";
        outFile << "        \"reg_write_static\": " << profile.reg_write_static << ",\n";
        outFile << "        \"unique_reg_read\": " << profile.unique_reg_read << ",\n";
        outFile << "        \"unique_reg_write\": " << profile.unique_reg_write << "\n";
        outFile << "      },\n";

        // E类：控制流细化
        outFile << "      \"control_flow_detail\": {\n";
        outFile << "        \"branch_taken_exec\": " << profile.branch_taken_exec << ",\n";
        outFile << "        \"branch_not_taken_exec\": " << profile.branch_not_taken_exec << ",\n";
        outFile << "        \"cond_branch_static\": " << profile.cond_branch_static << ",\n";
        outFile << "        \"uncond_branch_static\": " << profile.uncond_branch_static << ",\n";
        outFile << "        \"loop_iter_total\": " << profile.loop_iter_total << ",\n";
        outFile << "        \"call_depth_max\": " << profile.call_depth_max << "\n";
        outFile << "      }";

        // F类：数据依赖（可选）
        if (KnobEnableDep.Value()) {
            outFile << ",\n";
            outFile << "      \"data_dependency\": {\n";
            outFile << "        \"def_use_pairs\": " << profile.def_use_pairs << ",\n";
            outFile << "        \"reg_dep_chain_max\": " << profile.reg_dep_chain_max << ",\n";
            outFile << "        \"mem_to_reg_exec\": " << profile.mem_to_reg_exec << ",\n";
            outFile << "        \"reg_to_mem_exec\": " << profile.reg_to_mem_exec << "\n";
            outFile << "      }";
        }

        // G类：生命周期（可选）
        if (KnobEnableLifetime.Value()) {
            outFile << ",\n";
            outFile << "      \"lifetime\": {\n";
            outFile << "        \"reg_lifetime_total\": " << profile.reg_lifetime_total << ",\n";
            outFile << "        \"dead_write_exec\": " << profile.dead_write_exec << ",\n";
            outFile << "        \"first_use_dist_total\": " << profile.first_use_dist_total << "\n";
            outFile << "      }";
        }

        outFile << "\n";

        outFile << "    }";
    }

    outFile << "\n  ],\n";

    // 统计信息
    outFile << "  \"statistics\": {\n";
    outFile << "    \"total_functions_analyzed\": " << func_count << ",\n";
    outFile << "    \"total_instructions_executed\": " << g_total_inst_executed << "\n";
    outFile << "  }\n";

    outFile << "}\n";

    outFile.close();

    cerr << "[Function Profiler] Results written to: " << KnobOutputFile.Value() << endl;
    cerr << "[Function Profiler] Total functions analyzed: " << func_count << endl;
}

// ========== Main 函数 ==========

int main(int argc, char *argv[]) {
    // 初始化Pin符号处理
    PIN_InitSymbols();

    // 初始化Pin
    if (PIN_Init(argc, argv)) {
        cerr << "Usage: pin -t function_profiler.so [options] -- <program> [program args]" << endl;
        cerr << "Options:" << endl;
        cerr << "  -o <file>          输出JSON文件路径 (默认: function_profile.json)" << endl;
        cerr << "  -min_calls <n>     最小调用次数过滤 (默认: 1)" << endl;
        cerr << "  -enable_dep        启用F类数据依赖分析 (有性能开销)" << endl;
        cerr << "  -enable_lifetime   启用G类生命周期分析 (有性能开销)" << endl;
        return 1;
    }

    // 初始化锁
    PIN_InitLock(&g_lock);

    // 注册镜像加载回调
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // 注册程序结束回调
    PIN_AddFiniFunction(Fini, 0);

    cerr << "[Function Profiler] Starting instrumentation..." << endl;

    // 启动程序
    PIN_StartProgram();

    return 0;
}
