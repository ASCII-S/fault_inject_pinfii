/*
 * bbl_profiler.cpp - BBL维度剖析工具
 *
 * 功能：对应用程序进行BBL级别的执行特性剖析
 * 目的：为学术研究提供BBL特征数据，分析BBL弹性/可修复性
 *
 * 使用方法：
 *   pin -t bbl_profiler.so -o output.json -- <program> [args]
 *
 * 输出格式：JSON
 */

#include "bbl_profiler.h"
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

// BBL剖析数据表：BBL起始地址 -> BBLProfile
map<ADDRINT, BBLProfile> g_bbl_profiles;

// 边剖析数据表：(from, to) -> EdgeProfile
map<EdgeKey, EdgeProfile> g_edges;

// 函数入口地址集合（用于标记函数入口BBL）
set<ADDRINT> g_function_entries;

// 主程序镜像信息
string g_main_img_name = "";
ADDRINT g_main_img_low = 0;
ADDRINT g_main_img_high = 0;

// 全局统计
UINT64 g_total_bbl_executed = 0;
UINT64 g_total_inst_executed = 0;

// 线程安全锁
PIN_LOCK g_lock;

// ========== KNOB 参数定义 ==========

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "bbl_profile.json", "输出JSON文件路径");

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
 */
bool IsArithmeticInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
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
 */
bool IsFloatInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    if (mnemonic[0] == 'F' && mnemonic.find("FENCE") == string::npos) {
        return true;
    }
    if (mnemonic.find("SS") != string::npos ||
        mnemonic.find("SD") != string::npos ||
        mnemonic.find("PS") != string::npos ||
        mnemonic.find("PD") != string::npos) {
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
 */
bool IsSIMDInstruction(INS ins) {
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
 */
bool IsDataMovementInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
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
 */
bool IsPureComputeInstruction(INS ins) {
    if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        return false;
    }
    return IsArithmeticInstruction(ins) ||
           IsLogicInstruction(ins) ||
           IsFloatInstruction(ins) ||
           IsSIMDInstruction(ins);
}

/**
 * 判断地址是否在主程序范围内
 */
bool IsInMainImage(ADDRINT addr) {
    return addr >= g_main_img_low && addr < g_main_img_high;
}

/**
 * 获取地址对应的函数名
 */
string GetFunctionName(ADDRINT addr) {
    PIN_LockClient();
    RTN rtn = RTN_FindByAddress(addr);
    string name = RTN_Valid(rtn) ? RTN_Name(rtn) : "unknown";
    PIN_UnlockClient();
    return name;
}

// ========== 内存访问模式分析阈值 ==========
#define SEQUENTIAL_THRESHOLD 64   // ±64字节内视为连续（缓存行大小）
#define STRIDE_VARIANCE_THRESHOLD 8  // stride变化在8字节内视为步长访问

// ========== 动态分析回调函数 ==========

/**
 * BBL执行计数回调
 */
VOID CountBBLExec(BBLProfile* profile, UINT32 inst_count) {
    __sync_fetch_and_add(&(profile->exec_count), 1);
    __sync_fetch_and_add(&(profile->inst_exec), inst_count);
    __sync_fetch_and_add(&g_total_bbl_executed, 1);
    __sync_fetch_and_add(&g_total_inst_executed, inst_count);
}

/**
 * 内存读访问模式分析回调
 * 统计读次数并分析访问模式（连续/步长/随机）
 */
VOID AnalyzeMemoryReadPattern(BBLProfile* profile, ADDRINT addr) {
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
VOID AnalyzeMemoryWritePattern(BBLProfile* profile, ADDRINT addr) {
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
 * 访存指令执行回调
 */
VOID CountMemoryInstExec(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->mem_inst_exec), 1);
}

/**
 * 算术指令执行回调
 */
VOID CountArithmeticExec(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->arith_exec), 1);
}

/**
 * 逻辑指令执行回调
 */
VOID CountLogicExec(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->logic_exec), 1);
}

/**
 * 浮点指令执行回调
 */
VOID CountFloatExec(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->float_exec), 1);
}

/**
 * SIMD指令执行回调
 */
VOID CountSIMDExec(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->simd_exec), 1);
}

/**
 * 纯计算指令执行回调
 */
VOID CountPureComputeExec(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->pure_compute_exec), 1);
}

/**
 * 数据移动指令执行回调
 */
VOID CountDataMovementExec(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->data_movement_exec), 1);
}

/**
 * 寄存器操作计数回调
 */
VOID CountRegisterOps(BBLProfile* profile, UINT32 num_reads, UINT32 num_writes) {
    __sync_fetch_and_add(&(profile->reg_read_exec), num_reads);
    __sync_fetch_and_add(&(profile->reg_write_exec), num_writes);
}

/**
 * 内存到寄存器传递计数
 */
VOID CountMemToReg(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->mem_to_reg_exec), 1);
}

/**
 * 寄存器到内存传递计数
 */
VOID CountRegToMem(BBLProfile* profile) {
    __sync_fetch_and_add(&(profile->reg_to_mem_exec), 1);
}

/**
 * 边执行记录回调（用于直接跳转）
 */
VOID RecordDirectEdge(ADDRINT from_bbl, ADDRINT to_bbl, BOOL taken) {
    if (!taken) return;  // 对于条件分支，只在taken时记录
    if (!IsInMainImage(to_bbl)) return;

    EdgeKey key(from_bbl, to_bbl);
    PIN_GetLock(&g_lock, 1);
    auto it = g_edges.find(key);
    if (it != g_edges.end()) {
        it->second.exec_count++;
    }
    PIN_ReleaseLock(&g_lock);
}

/**
 * 边执行记录回调（用于fallthrough）
 */
VOID RecordFallthroughEdge(ADDRINT from_bbl, ADDRINT to_bbl) {
    if (!IsInMainImage(to_bbl)) return;

    EdgeKey key(from_bbl, to_bbl);
    PIN_GetLock(&g_lock, 1);
    auto it = g_edges.find(key);
    if (it != g_edges.end()) {
        it->second.exec_count++;
    }
    PIN_ReleaseLock(&g_lock);
}

/**
 * 边执行记录回调（用于间接跳转，动态发现目标）
 */
VOID RecordIndirectEdge(ADDRINT from_bbl, ADDRINT to_bbl) {
    if (!IsInMainImage(to_bbl)) return;

    EdgeKey key(from_bbl, to_bbl);
    PIN_GetLock(&g_lock, 1);
    auto it = g_edges.find(key);
    if (it != g_edges.end()) {
        it->second.exec_count++;
    } else {
        // 动态发现新边
        EdgeProfile edge(from_bbl, to_bbl, "indirect");
        edge.exec_count = 1;
        g_edges[key] = edge;

        // 检查是否是回边（目标地址 < 源地址）
        if (to_bbl < from_bbl) {
            auto bbl_it = g_bbl_profiles.find(to_bbl);
            if (bbl_it != g_bbl_profiles.end()) {
                bbl_it->second.is_loop_header = true;
            }
        }
    }
    PIN_ReleaseLock(&g_lock);
}

// ========== 静态分析函数 ==========

/**
 * 静态分析BBL内单条指令
 */
VOID AnalyzeStaticInstruction(INS ins, BBLProfile& profile) {
    // 统计静态指令数
    profile.inst_static++;

    // ========== D类: 计算特征静态统计 ==========
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

    if (IsArithmeticInstruction(ins)) {
        profile.arith_static++;
    }
    if (IsLogicInstruction(ins)) {
        profile.logic_static++;
    }
    if (IsFloatInstruction(ins)) {
        profile.float_static++;
    }
    if (IsSIMDInstruction(ins)) {
        profile.simd_static++;
    }
    if (IsDataMovementInstruction(ins)) {
        profile.data_movement_static++;
    }
    if (IsPureComputeInstruction(ins)) {
        profile.pure_compute_static++;
    }

    // ========== E类: 寄存器使用静态统计 ==========
    UINT32 num_rregs = INS_MaxNumRRegs(ins);
    UINT32 num_wregs = INS_MaxNumWRegs(ins);

    profile.reg_read_static += num_rregs;
    profile.reg_write_static += num_wregs;
    profile.use_count += num_rregs;
    profile.def_count += num_wregs;

    // 计算 live_in (在定义前使用的寄存器)
    for (UINT32 i = 0; i < num_rregs; i++) {
        REG reg = INS_RegR(ins, i);
        REG full_reg = REG_FullRegName(reg);
        if (REG_is_gr(full_reg)) {
            // 如果该寄存器还没有被定义过，则是外部输入
            if (profile.def_regs.find(full_reg) == profile.def_regs.end()) {
                profile.use_before_def.insert(full_reg);
            }
        }
    }

    // 记录定义的寄存器
    for (UINT32 i = 0; i < num_wregs; i++) {
        REG reg = INS_RegW(ins, i);
        REG full_reg = REG_FullRegName(reg);
        if (REG_is_gr(full_reg)) {
            profile.def_regs.insert(full_reg);
            profile.last_def_regs.insert(full_reg);  // 可能被后续覆盖，最后剩下的就是live_out候选
        }
    }
}

/**
 * 获取终结指令类型
 */
string GetTerminatorType(INS ins) {
    if (INS_IsRet(ins)) {
        return "ret";
    }
    if (INS_IsSyscall(ins)) {
        return "syscall";
    }
    if (INS_IsCall(ins)) {
        return "call";
    }
    if (INS_IsBranch(ins)) {
        if (INS_IsIndirectControlFlow(ins)) {
            return "indirect_branch";
        } else {
            return "direct_branch";
        }
    }
    return "fallthrough";
}

/**
 * 计算后继数量
 */
UINT32 GetSuccessorCount(INS tail) {
    if (INS_IsRet(tail) || INS_IsSyscall(tail)) {
        return 0;  // 返回或系统调用，后继不确定
    }
    if (INS_IsCall(tail)) {
        return 1;  // 调用后返回到下一指令
    }
    if (INS_IsBranch(tail)) {
        if (INS_HasFallThrough(tail)) {
            return 2;  // 条件分支：taken + fallthrough
        } else {
            return 1;  // 无条件跳转
        }
    }
    return 1;  // 顺序执行
}

// ========== TRACE插桩回调 ==========

VOID Trace(TRACE trace, VOID *v) {
    // 遍历TRACE中的每个BBL
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        ADDRINT bbl_addr = BBL_Address(bbl);

        // 只分析主程序中的BBL
        if (!IsInMainImage(bbl_addr)) {
            continue;
        }

        // 获取或创建BBL剖析数据
        PIN_GetLock(&g_lock, 1);
        BBLProfile& profile = g_bbl_profiles[bbl_addr];
        bool need_static_analysis = !profile.static_analyzed;
        PIN_ReleaseLock(&g_lock);

        if (need_static_analysis) {
            // 初始化基本信息
            profile.bbl_addr = bbl_addr;
            profile.bbl_offset = bbl_addr - g_main_img_low;
            profile.function_name = GetFunctionName(bbl_addr);
            profile.bbl_size_bytes = BBL_Size(bbl);

            // 记录指令地址范围
            INS head = BBL_InsHead(bbl);
            INS tail = BBL_InsTail(bbl);
            profile.inst_addr_start = INS_Address(head);
            profile.inst_addr_end = INS_Address(tail);

            // 检查是否是函数入口
            profile.is_function_entry = (g_function_entries.find(bbl_addr) != g_function_entries.end());

            // 静态分析BBL内每条指令
            for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
                AnalyzeStaticInstruction(ins, profile);
            }

            // 分析终结指令（tail已在上面定义）
            profile.terminator_type = GetTerminatorType(tail);
            profile.succ_count = GetSuccessorCount(tail);
            profile.has_indirect_branch = INS_IsIndirectControlFlow(tail);
            profile.is_function_exit = INS_IsRet(tail);

            // 计算 live_in 和 live_out 数量
            profile.live_in_count = profile.use_before_def.size();
            profile.live_out_count = profile.last_def_regs.size();

            // 创建静态边
            if (INS_IsBranch(tail) && INS_IsDirectControlFlow(tail)) {
                ADDRINT target = INS_DirectControlFlowTargetAddress(tail);
                if (IsInMainImage(target)) {
                    EdgeKey key(bbl_addr, target);
                    PIN_GetLock(&g_lock, 1);
                    if (g_edges.find(key) == g_edges.end()) {
                        g_edges[key] = EdgeProfile(bbl_addr, target, "taken");
                    }
                    // 检查是否是回边（循环）
                    if (target < bbl_addr) {
                        g_bbl_profiles[target].is_loop_header = true;
                    }
                    PIN_ReleaseLock(&g_lock);
                }
            }

            // fallthrough边
            if (INS_HasFallThrough(tail)) {
                ADDRINT fallthrough = INS_NextAddress(tail);
                if (IsInMainImage(fallthrough)) {
                    EdgeKey key(bbl_addr, fallthrough);
                    PIN_GetLock(&g_lock, 1);
                    if (g_edges.find(key) == g_edges.end()) {
                        g_edges[key] = EdgeProfile(bbl_addr, fallthrough, "fallthrough");
                    }
                    PIN_ReleaseLock(&g_lock);
                }
            }

            profile.static_analyzed = true;
        }

        // ========== 动态插桩 ==========

        // BBL执行计数
        UINT32 inst_count = BBL_NumIns(bbl);
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBBLExec,
                       IARG_PTR, &profile,
                       IARG_UINT32, inst_count,
                       IARG_END);

        // 遍历每条指令进行动态插桩
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
            // 内存读 - 使用地址分析访问模式
            if (INS_IsMemoryRead(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeMemoryReadPattern,
                               IARG_PTR, &profile,
                               IARG_MEMORYREAD_EA,
                               IARG_END);
            }

            // 内存写 - 使用地址分析访问模式
            if (INS_IsMemoryWrite(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeMemoryWritePattern,
                               IARG_PTR, &profile,
                               IARG_MEMORYWRITE_EA,
                               IARG_END);
            }

            // 访存指令
            if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountMemoryInstExec,
                               IARG_PTR, &profile,
                               IARG_END);
            }

            // 计算类指令
            if (IsArithmeticInstruction(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountArithmeticExec,
                               IARG_PTR, &profile,
                               IARG_END);
            }
            if (IsLogicInstruction(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountLogicExec,
                               IARG_PTR, &profile,
                               IARG_END);
            }
            if (IsFloatInstruction(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountFloatExec,
                               IARG_PTR, &profile,
                               IARG_END);
            }
            if (IsSIMDInstruction(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountSIMDExec,
                               IARG_PTR, &profile,
                               IARG_END);
            }
            if (IsPureComputeInstruction(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountPureComputeExec,
                               IARG_PTR, &profile,
                               IARG_END);
            }
            if (IsDataMovementInstruction(ins)) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountDataMovementExec,
                               IARG_PTR, &profile,
                               IARG_END);
            }

            // 寄存器操作
            UINT32 num_rregs = INS_MaxNumRRegs(ins);
            UINT32 num_wregs = INS_MaxNumWRegs(ins);
            if (num_rregs > 0 || num_wregs > 0) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountRegisterOps,
                               IARG_PTR, &profile,
                               IARG_UINT32, num_rregs,
                               IARG_UINT32, num_wregs,
                               IARG_END);
            }

            // 内存到寄存器传递
            if (INS_IsMemoryRead(ins) && num_wregs > 0) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountMemToReg,
                               IARG_PTR, &profile,
                               IARG_END);
            }

            // 寄存器到内存传递
            if (INS_IsMemoryWrite(ins) && num_rregs > 0) {
                INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountRegToMem,
                               IARG_PTR, &profile,
                               IARG_END);
            }
        }

        // ========== 边统计插桩 ==========
        INS tail = BBL_InsTail(bbl);

        if (INS_IsIndirectControlFlow(tail) && !INS_IsRet(tail)) {
            // 间接跳转：动态捕获目标
            INS_InsertCall(tail, IPOINT_BEFORE, (AFUNPTR)RecordIndirectEdge,
                           IARG_ADDRINT, bbl_addr,
                           IARG_BRANCH_TARGET_ADDR,
                           IARG_END);
        } else if (INS_IsBranch(tail) && INS_IsDirectControlFlow(tail)) {
            // 直接跳转
            ADDRINT target = INS_DirectControlFlowTargetAddress(tail);
            if (IsInMainImage(target)) {
                INS_InsertCall(tail, IPOINT_BEFORE, (AFUNPTR)RecordDirectEdge,
                               IARG_ADDRINT, bbl_addr,
                               IARG_ADDRINT, target,
                               IARG_BRANCH_TAKEN,
                               IARG_END);
            }
        }

        // Fallthrough边统计
        if (INS_HasFallThrough(tail)) {
            ADDRINT fallthrough = INS_NextAddress(tail);
            if (IsInMainImage(fallthrough)) {
                // 对于条件分支，需要在未跳转时记录
                if (INS_IsBranch(tail)) {
                    INS_InsertCall(tail, IPOINT_BEFORE, (AFUNPTR)RecordDirectEdge,
                                   IARG_ADDRINT, bbl_addr,
                                   IARG_ADDRINT, fallthrough,
                                   IARG_BRANCH_TAKEN,
                                   IARG_END);
                } else {
                    // 非分支的fallthrough，直接记录
                    INS_InsertCall(tail, IPOINT_BEFORE, (AFUNPTR)RecordFallthroughEdge,
                                   IARG_ADDRINT, bbl_addr,
                                   IARG_ADDRINT, fallthrough,
                                   IARG_END);
                }
            }
        }
    }
}

// ========== 镜像加载回调 ==========

VOID ImageLoad(IMG img, VOID *v) {
    // 只记录主程序信息
    if (!IMG_IsMainExecutable(img)) {
        return;
    }

    g_main_img_name = IMG_Name(img);
    g_main_img_low = IMG_LowAddress(img);
    g_main_img_high = IMG_HighAddress(img);

    cerr << "[BBL Profiler] Analyzing main image: " << g_main_img_name << endl;
    cerr << "[BBL Profiler] Address range: " << addr_to_hex(g_main_img_low)
         << " - " << addr_to_hex(g_main_img_high) << endl;

    // 收集所有函数入口地址
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        if (SEC_Name(sec) != ".text") {
            continue;
        }
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            g_function_entries.insert(RTN_Address(rtn));
        }
    }

    cerr << "[BBL Profiler] Found " << g_function_entries.size() << " functions" << endl;
}

// ========== 程序结束回调 ==========

VOID Fini(INT32 code, VOID *v) {
    cerr << "[BBL Profiler] Program finished. Writing results..." << endl;

    ofstream outFile(KnobOutputFile.Value().c_str());
    if (!outFile.is_open()) {
        cerr << "[ERROR] Cannot open output file: " << KnobOutputFile.Value() << endl;
        return;
    }

    // 写入JSON
    outFile << "{\n";

    // 工具信息
    outFile << "  \"tool_info\": {\n";
    outFile << "    \"name\": \"BBL Profiler\",\n";
    outFile << "    \"version\": \"1.0\",\n";
    outFile << "    \"description\": \"BBL维度的执行特性剖析工具\",\n";
    outFile << "    \"main_image\": \"" << escape_json(g_main_img_name) << "\",\n";
    outFile << "    \"base_address\": \"" << addr_to_hex(g_main_img_low) << "\"\n";
    outFile << "  },\n";

    // BBL数据
    outFile << "  \"bbls\": [\n";

    bool first_bbl = true;
    int bbl_count = 0;

    for (const auto& kv : g_bbl_profiles) {
        const BBLProfile& profile = kv.second;

        if (!first_bbl) {
            outFile << ",\n";
        }
        first_bbl = false;
        bbl_count++;

        outFile << "    {\n";

        // A类：基本属性
        outFile << "      \"bbl_addr\": \"" << addr_to_hex(profile.bbl_addr) << "\",\n";
        outFile << "      \"bbl_offset\": \"" << addr_to_hex(profile.bbl_offset) << "\",\n";
        outFile << "      \"inst_addr_start\": \"" << addr_to_hex(profile.inst_addr_start) << "\",\n";
        outFile << "      \"inst_addr_end\": \"" << addr_to_hex(profile.inst_addr_end) << "\",\n";
        outFile << "      \"function_name\": \"" << escape_json(profile.function_name) << "\",\n";

        // B类：执行统计
        outFile << "      \"basic_stats\": {\n";
        outFile << "        \"inst_static\": " << profile.inst_static << ",\n";
        outFile << "        \"bbl_size_bytes\": " << profile.bbl_size_bytes << ",\n";
        outFile << "        \"exec_count\": " << profile.exec_count << ",\n";
        outFile << "        \"inst_exec\": " << profile.inst_exec << "\n";
        outFile << "      },\n";

        // C类：控制流特征
        outFile << "      \"control_flow\": {\n";
        outFile << "        \"succ_count\": " << profile.succ_count << ",\n";
        outFile << "        \"is_loop_header\": " << (profile.is_loop_header ? "true" : "false") << ",\n";
        outFile << "        \"is_function_entry\": " << (profile.is_function_entry ? "true" : "false") << ",\n";
        outFile << "        \"is_function_exit\": " << (profile.is_function_exit ? "true" : "false") << ",\n";
        outFile << "        \"terminator_type\": \"" << profile.terminator_type << "\",\n";
        outFile << "        \"has_indirect_branch\": " << (profile.has_indirect_branch ? "true" : "false") << "\n";
        outFile << "      },\n";

        // D类：数据流
        outFile << "      \"data_flow\": {\n";
        outFile << "        \"mem_read_static\": " << profile.mem_read_static << ",\n";
        outFile << "        \"mem_write_static\": " << profile.mem_write_static << ",\n";
        outFile << "        \"mem_inst_static\": " << profile.mem_inst_static << ",\n";
        outFile << "        \"mem_read_exec\": " << profile.mem_read_exec << ",\n";
        outFile << "        \"mem_write_exec\": " << profile.mem_write_exec << ",\n";
        outFile << "        \"mem_inst_exec\": " << profile.mem_inst_exec << "\n";
        outFile << "      },\n";

        // D2类：内存访问模式
        outFile << "      \"memory_access_pattern\": {\n";
        outFile << "        \"seq_read_exec\": " << profile.seq_read_exec << ",\n";
        outFile << "        \"stride_read_exec\": " << profile.stride_read_exec << ",\n";
        outFile << "        \"random_read_exec\": " << profile.random_read_exec << ",\n";
        outFile << "        \"seq_write_exec\": " << profile.seq_write_exec << ",\n";
        outFile << "        \"stride_write_exec\": " << profile.stride_write_exec << ",\n";
        outFile << "        \"random_write_exec\": " << profile.random_write_exec << "\n";
        outFile << "      },\n";

        // D3类：计算特征
        outFile << "      \"compute\": {\n";
        outFile << "        \"arith_static\": " << profile.arith_static << ",\n";
        outFile << "        \"logic_static\": " << profile.logic_static << ",\n";
        outFile << "        \"float_static\": " << profile.float_static << ",\n";
        outFile << "        \"simd_static\": " << profile.simd_static << ",\n";
        outFile << "        \"data_movement_static\": " << profile.data_movement_static << ",\n";
        outFile << "        \"pure_compute_static\": " << profile.pure_compute_static << ",\n";
        outFile << "        \"arith_exec\": " << profile.arith_exec << ",\n";
        outFile << "        \"logic_exec\": " << profile.logic_exec << ",\n";
        outFile << "        \"float_exec\": " << profile.float_exec << ",\n";
        outFile << "        \"simd_exec\": " << profile.simd_exec << ",\n";
        outFile << "        \"data_movement_exec\": " << profile.data_movement_exec << ",\n";
        outFile << "        \"pure_compute_exec\": " << profile.pure_compute_exec << "\n";
        outFile << "      },\n";

        // E类：数据依赖
        outFile << "      \"data_dependency\": {\n";
        outFile << "        \"live_in_count\": " << profile.live_in_count << ",\n";
        outFile << "        \"live_out_count\": " << profile.live_out_count << ",\n";
        outFile << "        \"def_count\": " << profile.def_count << ",\n";
        outFile << "        \"use_count\": " << profile.use_count << ",\n";
        outFile << "        \"reg_read_static\": " << profile.reg_read_static << ",\n";
        outFile << "        \"reg_write_static\": " << profile.reg_write_static << ",\n";
        outFile << "        \"reg_read_exec\": " << profile.reg_read_exec << ",\n";
        outFile << "        \"reg_write_exec\": " << profile.reg_write_exec << ",\n";
        outFile << "        \"mem_to_reg_exec\": " << profile.mem_to_reg_exec << ",\n";
        outFile << "        \"reg_to_mem_exec\": " << profile.reg_to_mem_exec << "\n";
        outFile << "      }\n";

        outFile << "    }";
    }

    outFile << "\n  ],\n";

    // F类：边数据
    outFile << "  \"edges\": [\n";

    bool first_edge = true;
    int edge_count = 0;

    for (const auto& kv : g_edges) {
        const EdgeProfile& edge = kv.second;

        // 只输出执行过的边
        if (edge.exec_count == 0) {
            continue;
        }

        if (!first_edge) {
            outFile << ",\n";
        }
        first_edge = false;
        edge_count++;

        outFile << "    {\n";
        outFile << "      \"from_bbl\": \"" << addr_to_hex(edge.from_bbl) << "\",\n";
        outFile << "      \"to_bbl\": \"" << addr_to_hex(edge.to_bbl) << "\",\n";
        outFile << "      \"exec_count\": " << edge.exec_count << ",\n";
        outFile << "      \"edge_type\": \"" << edge.edge_type << "\"\n";
        outFile << "    }";
    }

    outFile << "\n  ],\n";

    // 统计信息
    outFile << "  \"statistics\": {\n";
    outFile << "    \"total_bbls\": " << bbl_count << ",\n";
    outFile << "    \"total_edges\": " << edge_count << ",\n";
    outFile << "    \"total_bbl_executions\": " << g_total_bbl_executed << ",\n";
    outFile << "    \"total_inst_executions\": " << g_total_inst_executed << "\n";
    outFile << "  }\n";

    outFile << "}\n";

    outFile.close();

    cerr << "[BBL Profiler] Results written to: " << KnobOutputFile.Value() << endl;
    cerr << "[BBL Profiler] Total BBLs analyzed: " << bbl_count << endl;
    cerr << "[BBL Profiler] Total edges recorded: " << edge_count << endl;
}

// ========== Main 函数 ==========

int main(int argc, char *argv[]) {
    // 初始化Pin符号处理
    PIN_InitSymbols();

    // 初始化Pin
    if (PIN_Init(argc, argv)) {
        cerr << "Usage: pin -t bbl_profiler.so [options] -- <program> [program args]" << endl;
        cerr << "Options:" << endl;
        cerr << "  -o <file>    输出JSON文件路径 (默认: bbl_profile.json)" << endl;
        return 1;
    }

    // 初始化锁
    PIN_InitLock(&g_lock);

    // 注册回调
    IMG_AddInstrumentFunction(ImageLoad, 0);
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddFiniFunction(Fini, 0);

    cerr << "[BBL Profiler] Starting instrumentation..." << endl;

    // 启动程序
    PIN_StartProgram();

    return 0;
}
