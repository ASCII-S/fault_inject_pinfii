/**
 * @file app_profiler.cpp
 * @brief 应用维度算法特性剖析工具 - 主程序
 *
 * 基于Intel Pin的应用级特征剖析工具，用于分析：
 * - 数值敏感性：浮点运算分类、精度分布
 * - 误差吸收能力：比较指令、饱和运算等
 * - 外部库调用：库函数分类统计
 *
 * 使用方法：
 *   pin -t app_profiler.so [-o output.json] -- <program> [args]
 */

#include "app_profiler.h"
#include "lib_classifier.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace std;

// ============================================================================
// 全局变量
// ============================================================================

AppProfile g_app_profile;
vector<ImageInfo> g_loaded_images;
PIN_LOCK g_lock;

// ============================================================================
// 命令行参数
// ============================================================================

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "app_profile.json", "输出JSON文件路径");

KNOB<BOOL> KnobVerbose(KNOB_MODE_WRITEONCE, "pintool",
    "v", "0", "详细输出模式");

// ============================================================================
// 前向声明
// ============================================================================

bool IsFloatInstruction(INS ins);
bool IsIntArithInstruction(INS ins);
bool IsMemoryInstruction(INS ins);
bool IsControlFlowInstruction(INS ins);
bool IsLogicInstruction(INS ins);
bool IsMovInstruction(INS ins);
bool IsSIMDInstruction(INS ins);

// ============================================================================
// 辅助函数：指令分类
// ============================================================================

/**
 * @brief 判断是否为整数算术指令
 */
bool IsIntArithInstruction(INS ins) {
    OPCODE opcode = INS_Opcode(ins);
    switch (opcode) {
        case XED_ICLASS_ADD:
        case XED_ICLASS_ADC:
        case XED_ICLASS_INC:
        case XED_ICLASS_SUB:
        case XED_ICLASS_SBB:
        case XED_ICLASS_DEC:
        case XED_ICLASS_NEG:
        case XED_ICLASS_MUL:
        case XED_ICLASS_IMUL:
        case XED_ICLASS_DIV:
        case XED_ICLASS_IDIV:
            return true;
        default:
            return false;
    }
}

/**
 * @brief 获取整数算术指令子类型: 0=add, 1=sub, 2=mul, 3=div
 */
UINT32 GetIntArithSubtype(INS ins) {
    OPCODE opcode = INS_Opcode(ins);
    switch (opcode) {
        case XED_ICLASS_ADD:
        case XED_ICLASS_ADC:
        case XED_ICLASS_INC:
            return 0;  // add
        case XED_ICLASS_SUB:
        case XED_ICLASS_SBB:
        case XED_ICLASS_DEC:
        case XED_ICLASS_NEG:
            return 1;  // sub
        case XED_ICLASS_MUL:
        case XED_ICLASS_IMUL:
            return 2;  // mul
        case XED_ICLASS_DIV:
        case XED_ICLASS_IDIV:
            return 3;  // div
        default:
            return 0;
    }
}

/**
 * @brief 判断是否为内存访问指令
 */
bool IsMemoryInstruction(INS ins) {
    return INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins) ||
           INS_Opcode(ins) == XED_ICLASS_LEA ||
           INS_Opcode(ins) == XED_ICLASS_PUSH ||
           INS_Opcode(ins) == XED_ICLASS_POP;
}

/**
 * @brief 获取内存访问指令子类型: 0=load, 1=store, 2=stack
 */
UINT32 GetMemorySubtype(INS ins) {
    OPCODE opcode = INS_Opcode(ins);
    if (opcode == XED_ICLASS_PUSH || opcode == XED_ICLASS_POP) {
        return 2;  // stack
    }
    if (INS_IsMemoryWrite(ins)) {
        return 1;  // store
    }
    return 0;  // load (including LEA)
}

/**
 * @brief 判断是否为控制流指令
 */
bool IsControlFlowInstruction(INS ins) {
    return INS_IsBranch(ins) || INS_IsCall(ins) || INS_IsRet(ins) ||
           INS_Opcode(ins) == XED_ICLASS_LOOP ||
           INS_Opcode(ins) == XED_ICLASS_LOOPE ||
           INS_Opcode(ins) == XED_ICLASS_LOOPNE;
}

/**
 * @brief 获取控制流指令子类型: 0=jmp, 1=jcc, 2=call, 3=ret
 */
UINT32 GetControlFlowSubtype(INS ins) {
    if (INS_IsCall(ins)) {
        return 2;  // call
    }
    if (INS_IsRet(ins)) {
        return 3;  // ret
    }
    if (INS_IsBranch(ins)) {
        // 区分条件跳转和无条件跳转
        OPCODE opcode = INS_Opcode(ins);
        if (opcode == XED_ICLASS_JMP || opcode == XED_ICLASS_JMP_FAR) {
            return 0;  // jmp (无条件)
        }
        return 1;  // jcc (条件跳转)
    }
    return 0;  // loop等归为jmp
}

/**
 * @brief 判断是否为逻辑运算指令
 */
bool IsLogicInstruction(INS ins) {
    OPCODE opcode = INS_Opcode(ins);
    switch (opcode) {
        // 位运算
        case XED_ICLASS_AND:
        case XED_ICLASS_OR:
        case XED_ICLASS_XOR:
        case XED_ICLASS_NOT:
        // 移位运算
        case XED_ICLASS_SHL:
        case XED_ICLASS_SHR:
        case XED_ICLASS_SAR:
        case XED_ICLASS_ROL:
        case XED_ICLASS_ROR:
        case XED_ICLASS_RCL:
        case XED_ICLASS_RCR:
        case XED_ICLASS_SHLD:
        case XED_ICLASS_SHRD:
            return true;
        default:
            return false;
    }
}

/**
 * @brief 获取逻辑运算指令子类型: 0=bitwise, 1=shift
 */
UINT32 GetLogicSubtype(INS ins) {
    OPCODE opcode = INS_Opcode(ins);
    switch (opcode) {
        case XED_ICLASS_AND:
        case XED_ICLASS_OR:
        case XED_ICLASS_XOR:
        case XED_ICLASS_NOT:
            return 0;  // bitwise
        default:
            return 1;  // shift
    }
}

/**
 * @brief 判断是否为数据移动指令
 */
bool IsMovInstruction(INS ins) {
    OPCODE opcode = INS_Opcode(ins);
    switch (opcode) {
        case XED_ICLASS_MOV:
        case XED_ICLASS_MOVSX:
        case XED_ICLASS_MOVSXD:
        case XED_ICLASS_MOVZX:
        case XED_ICLASS_XCHG:
        case XED_ICLASS_CMOVB:
        case XED_ICLASS_CMOVBE:
        case XED_ICLASS_CMOVL:
        case XED_ICLASS_CMOVLE:
        case XED_ICLASS_CMOVNB:
        case XED_ICLASS_CMOVNBE:
        case XED_ICLASS_CMOVNL:
        case XED_ICLASS_CMOVNLE:
        case XED_ICLASS_CMOVNO:
        case XED_ICLASS_CMOVNP:
        case XED_ICLASS_CMOVNS:
        case XED_ICLASS_CMOVNZ:
        case XED_ICLASS_CMOVO:
        case XED_ICLASS_CMOVP:
        case XED_ICLASS_CMOVS:
        case XED_ICLASS_CMOVZ:
            return true;
        default:
            return false;
    }
}

/**
 * @brief 判断是否为SIMD指令（非浮点）
 */
bool IsSIMDInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);

    // 检查是否使用XMM/YMM/ZMM寄存器
    bool uses_simd_reg = false;
    for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
        REG reg = INS_RegR(ins, i);
        if (REG_is_xmm(reg) || REG_is_ymm(reg) || REG_is_zmm(reg)) {
            uses_simd_reg = true;
            break;
        }
    }
    if (!uses_simd_reg) {
        for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
            REG reg = INS_RegW(ins, i);
            if (REG_is_xmm(reg) || REG_is_ymm(reg) || REG_is_zmm(reg)) {
                uses_simd_reg = true;
                break;
            }
        }
    }

    return uses_simd_reg;
}

/**
 * @brief 获取SIMD指令子类型: 0=sse, 1=avx, 2=avx512
 */
UINT32 GetSIMDSubtype(INS ins) {
    string mnemonic = INS_Mnemonic(ins);

    // 检查是否使用ZMM寄存器 (AVX-512)
    for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
        REG reg = INS_RegR(ins, i);
        if (REG_is_zmm(reg)) return 2;  // avx512
    }
    for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
        REG reg = INS_RegW(ins, i);
        if (REG_is_zmm(reg)) return 2;  // avx512
    }

    // 检查是否使用YMM寄存器 (AVX/AVX2)
    for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
        REG reg = INS_RegR(ins, i);
        if (REG_is_ymm(reg)) return 1;  // avx
    }
    for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
        REG reg = INS_RegW(ins, i);
        if (REG_is_ymm(reg)) return 1;  // avx
    }

    // V前缀的指令也是AVX
    if (mnemonic[0] == 'V') {
        return 1;  // avx
    }

    return 0;  // sse
}

/**
 * @brief 获取指令大类
 */
InstCategory GetInstCategory(INS ins) {
    // 按优先级判断（SIMD和浮点可能重叠，浮点优先）
    if (IsFloatInstruction(ins)) {
        return INST_FLOAT;
    }
    if (IsSIMDInstruction(ins)) {
        return INST_SIMD;
    }
    if (IsIntArithInstruction(ins)) {
        return INST_INT_ARITH;
    }
    if (IsControlFlowInstruction(ins)) {
        return INST_CONTROL;
    }
    if (IsLogicInstruction(ins)) {
        return INST_LOGIC;
    }
    if (IsMovInstruction(ins)) {
        return INST_MOV;
    }
    if (IsMemoryInstruction(ins)) {
        return INST_MEMORY;
    }
    return INST_OTHER;
}

/**
 * @brief 判断是否为浮点指令
 */
bool IsFloatInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);

    // x87 浮点指令 (F开头)
    if (mnemonic[0] == 'F' && mnemonic.length() > 1) {
        // 排除一些非浮点的F开头指令
        if (mnemonic == "FXSAVE" || mnemonic == "FXRSTOR" ||
            mnemonic == "FXSAVE64" || mnemonic == "FXRSTOR64") {
            return false;
        }
        return true;
    }

    // SSE/AVX 浮点指令
    // 单精度: SS后缀 (Scalar Single), PS后缀 (Packed Single)
    // 双精度: SD后缀 (Scalar Double), PD后缀 (Packed Double)
    size_t len = mnemonic.length();
    if (len >= 2) {
        string suffix = mnemonic.substr(len - 2);
        if (suffix == "SS" || suffix == "SD" || suffix == "PS" || suffix == "PD") {
            return true;
        }
    }

    // 特殊浮点指令
    if (mnemonic.find("CVTSI2S") != string::npos ||  // 整数转浮点
        mnemonic.find("CVTS") != string::npos ||     // 浮点转换
        mnemonic.find("CVTT") != string::npos) {     // 截断转换
        return true;
    }

    return false;
}

/**
 * @brief 获取浮点运算类型
 */
FloatOpType GetFloatOpType(INS ins) {
    string mnemonic = INS_Mnemonic(ins);

    // 加减运算
    if (mnemonic.find("ADD") != string::npos ||
        mnemonic.find("SUB") != string::npos ||
        mnemonic.find("FADD") != string::npos ||
        mnemonic.find("FSUB") != string::npos) {
        // 排除FMA中的ADD
        if (mnemonic.find("FMA") == string::npos &&
            mnemonic.find("FMADD") == string::npos &&
            mnemonic.find("FMSUB") == string::npos) {
            return FLOAT_ADD_SUB;
        }
    }

    // 乘法运算
    if (mnemonic.find("MUL") != string::npos ||
        mnemonic.find("FMUL") != string::npos) {
        // 排除FMA
        if (mnemonic.find("FMA") == string::npos) {
            return FLOAT_MUL;
        }
    }

    // 除法运算（敏感运算）
    if (mnemonic.find("DIV") != string::npos ||
        mnemonic.find("FDIV") != string::npos) {
        return FLOAT_DIV;
    }

    // 开方运算（敏感运算）
    if (mnemonic.find("SQRT") != string::npos ||
        mnemonic.find("FSQRT") != string::npos) {
        return FLOAT_SQRT;
    }

    // FMA (Fused Multiply-Add)
    if (mnemonic.find("FMA") != string::npos ||
        mnemonic.find("VFMADD") != string::npos ||
        mnemonic.find("VFMSUB") != string::npos ||
        mnemonic.find("VFNMADD") != string::npos ||
        mnemonic.find("VFNMSUB") != string::npos) {
        return FLOAT_FMA;
    }

    // 浮点比较
    if (mnemonic.find("CMP") != string::npos ||
        mnemonic.find("COM") != string::npos ||
        mnemonic.find("FCOM") != string::npos ||
        mnemonic.find("FUCOM") != string::npos ||
        mnemonic.find("FCOMI") != string::npos) {
        return FLOAT_CMP;
    }

    // 精度转换
    if (mnemonic.find("CVT") != string::npos) {
        return FLOAT_CVT;
    }

    return FLOAT_OTHER;
}

/**
 * @brief 获取浮点精度类型
 */
FloatPrecision GetFloatPrecision(INS ins) {
    string mnemonic = INS_Mnemonic(ins);

    // x87 扩展精度
    if (mnemonic[0] == 'F' && mnemonic.length() > 1) {
        return PREC_EXTENDED;
    }

    size_t len = mnemonic.length();
    if (len >= 2) {
        string suffix = mnemonic.substr(len - 2);
        // 单精度
        if (suffix == "SS" || suffix == "PS") {
            return PREC_SINGLE;
        }
        // 双精度
        if (suffix == "SD" || suffix == "PD") {
            return PREC_DOUBLE;
        }
    }

    return PREC_UNKNOWN;
}

/**
 * @brief 判断是否为SIMD浮点指令
 */
bool IsSIMDFloatInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    size_t len = mnemonic.length();

    if (len >= 2) {
        string suffix = mnemonic.substr(len - 2);
        // Packed 指令是SIMD
        if (suffix == "PS" || suffix == "PD") {
            return true;
        }
    }

    // AVX 256/512位指令
    if (mnemonic[0] == 'V') {
        // 检查是否使用YMM/ZMM寄存器
        for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
            REG reg = INS_RegR(ins, i);
            if (REG_is_ymm(reg) || REG_is_zmm(reg)) {
                return true;
            }
        }
        for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
            REG reg = INS_RegW(ins, i);
            if (REG_is_ymm(reg) || REG_is_zmm(reg)) {
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief 判断是否为比较指令
 */
bool IsCompareInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    return (mnemonic == "CMP" || mnemonic == "CMPB" ||
            mnemonic == "CMPW" || mnemonic == "CMPL" ||
            mnemonic == "CMPQ" || mnemonic.find("CMP") == 0);
}

/**
 * @brief 判断是否为TEST指令
 */
bool IsTestInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    return (mnemonic == "TEST" || mnemonic == "TESTB" ||
            mnemonic == "TESTW" || mnemonic == "TESTL" ||
            mnemonic == "TESTQ");
}

/**
 * @brief 判断是否为饱和运算指令
 */
bool IsSaturateInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    // SSE 饱和运算
    return (mnemonic.find("PADDS") != string::npos ||   // 饱和加
            mnemonic.find("PSUBS") != string::npos ||   // 饱和减
            mnemonic.find("PADDUS") != string::npos ||  // 无符号饱和加
            mnemonic.find("PSUBUS") != string::npos ||  // 无符号饱和减
            mnemonic.find("PACKSS") != string::npos ||  // 饱和打包
            mnemonic.find("PACKUS") != string::npos);   // 无符号饱和打包
}

/**
 * @brief 判断是否为MIN/MAX指令
 */
bool IsMinMaxInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    return (mnemonic.find("MIN") != string::npos ||
            mnemonic.find("MAX") != string::npos);
}

/**
 * @brief 判断是否为绝对值指令
 */
bool IsAbsInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    return (mnemonic == "FABS" ||
            mnemonic.find("PABS") != string::npos ||    // SSSE3 整数绝对值
            mnemonic.find("ANDPS") != string::npos ||   // 常用于实现fabs
            mnemonic.find("ANDPD") != string::npos);
}

/**
 * @brief 判断是否为舍入指令
 */
bool IsRoundInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    return (mnemonic.find("ROUND") != string::npos ||   // SSE4.1 舍入
            mnemonic == "FRNDINT" ||                     // x87 舍入
            mnemonic.find("CVTT") != string::npos);     // 截断转换
}

// ============================================================================
// 分析回调函数
// ============================================================================

/**
 * @brief 指令执行计数回调
 */
VOID CountInstruction(VOID* v) {
    PIN_GetLock(&g_lock, 1);
    g_app_profile.total_inst_exec++;
    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief 浮点指令执行回调
 */
VOID CountFloatInstruction(FloatOpType op_type, FloatPrecision precision, BOOL is_simd) {
    PIN_GetLock(&g_lock, 1);

    g_app_profile.float_inst_exec++;

    // 运算类型统计
    switch (op_type) {
        case FLOAT_ADD_SUB: g_app_profile.float_add_sub_exec++; break;
        case FLOAT_MUL:     g_app_profile.float_mul_exec++; break;
        case FLOAT_DIV:     g_app_profile.float_div_exec++; break;
        case FLOAT_SQRT:    g_app_profile.float_sqrt_exec++; break;
        case FLOAT_FMA:     g_app_profile.float_fma_exec++; break;
        case FLOAT_CMP:     g_app_profile.float_cmp_exec++; break;
        case FLOAT_CVT:     g_app_profile.float_cvt_exec++; break;
        default: break;
    }

    // 精度统计
    switch (precision) {
        case PREC_SINGLE:   g_app_profile.single_precision_exec++; break;
        case PREC_DOUBLE:   g_app_profile.double_precision_exec++; break;
        case PREC_EXTENDED: g_app_profile.x87_exec++; break;
        default: break;
    }

    // SIMD统计
    if (is_simd) {
        g_app_profile.simd_float_exec++;
    }

    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief 比较指令执行回调
 */
VOID CountCompareInstruction(VOID* v) {
    PIN_GetLock(&g_lock, 1);
    g_app_profile.cmp_inst_exec++;
    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief TEST指令执行回调
 */
VOID CountTestInstruction(VOID* v) {
    PIN_GetLock(&g_lock, 1);
    g_app_profile.test_inst_exec++;
    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief 饱和运算执行回调
 */
VOID CountSaturateInstruction(VOID* v) {
    PIN_GetLock(&g_lock, 1);
    g_app_profile.saturate_inst_exec++;
    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief MIN/MAX指令执行回调
 */
VOID CountMinMaxInstruction(VOID* v) {
    PIN_GetLock(&g_lock, 1);
    g_app_profile.minmax_inst_exec++;
    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief 绝对值指令执行回调
 */
VOID CountAbsInstruction(VOID* v) {
    PIN_GetLock(&g_lock, 1);
    g_app_profile.abs_inst_exec++;
    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief 舍入指令执行回调
 */
VOID CountRoundInstruction(VOID* v) {
    PIN_GetLock(&g_lock, 1);
    g_app_profile.round_inst_exec++;
    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief 指令类型分布计数回调
 */
VOID CountInstCategory(InstCategory category, UINT32 subtype) {
    PIN_GetLock(&g_lock, 1);

    g_app_profile.inst_category[category].exec_count++;

    // 细分类型统计
    switch (category) {
        case INST_INT_ARITH:
            switch (subtype) {
                case 0: g_app_profile.int_add.exec_count++; break;
                case 1: g_app_profile.int_sub.exec_count++; break;
                case 2: g_app_profile.int_mul.exec_count++; break;
                case 3: g_app_profile.int_div.exec_count++; break;
            }
            break;
        case INST_MEMORY:
            switch (subtype) {
                case 0: g_app_profile.mem_load.exec_count++; break;
                case 1: g_app_profile.mem_store.exec_count++; break;
                case 2: g_app_profile.mem_stack.exec_count++; break;
            }
            break;
        case INST_CONTROL:
            switch (subtype) {
                case 0: g_app_profile.ctrl_jmp.exec_count++; break;
                case 1: g_app_profile.ctrl_jcc.exec_count++; break;
                case 2: g_app_profile.ctrl_call.exec_count++; break;
                case 3: g_app_profile.ctrl_ret.exec_count++; break;
            }
            break;
        case INST_LOGIC:
            switch (subtype) {
                case 0: g_app_profile.logic_bitwise.exec_count++; break;
                case 1: g_app_profile.logic_shift.exec_count++; break;
            }
            break;
        case INST_SIMD:
            switch (subtype) {
                case 0: g_app_profile.simd_sse.exec_count++; break;
                case 1: g_app_profile.simd_avx.exec_count++; break;
                case 2: g_app_profile.simd_avx512.exec_count++; break;
            }
            break;
        default:
            break;
    }

    PIN_ReleaseLock(&g_lock);
}

/**
 * @brief 函数调用回调
 */
VOID CountFunctionCall(const string* func_name, BOOL is_lib_func, LibCategory category) {
    PIN_GetLock(&g_lock, 1);

    g_app_profile.total_func_calls++;

    if (is_lib_func) {
        g_app_profile.total_lib_calls++;
        g_app_profile.lib_stats[category].call_count++;
        g_app_profile.lib_stats[category].unique_funcs.insert(*func_name);
    } else {
        g_app_profile.user_func_calls++;
        g_app_profile.lib_stats[LIB_USER].call_count++;
        g_app_profile.lib_stats[LIB_USER].unique_funcs.insert(*func_name);
    }

    PIN_ReleaseLock(&g_lock);
}

// ============================================================================
// 插桩函数
// ============================================================================

/**
 * @brief 指令级插桩
 */
VOID InstrumentInstruction(INS ins, VOID* v) {
    // 总指令计数（静态和动态）
    g_app_profile.total_inst_static++;
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountInstruction,
                   IARG_PTR, NULL, IARG_END);

    // ========== D类：指令类型分布统计 ==========
    InstCategory category = GetInstCategory(ins);
    UINT32 subtype = 0;

    // 更新静态计数
    g_app_profile.inst_category[category].static_count++;

    // 获取子类型并更新细分静态计数
    switch (category) {
        case INST_INT_ARITH:
            subtype = GetIntArithSubtype(ins);
            switch (subtype) {
                case 0: g_app_profile.int_add.static_count++; break;
                case 1: g_app_profile.int_sub.static_count++; break;
                case 2: g_app_profile.int_mul.static_count++; break;
                case 3: g_app_profile.int_div.static_count++; break;
            }
            break;
        case INST_MEMORY:
            subtype = GetMemorySubtype(ins);
            switch (subtype) {
                case 0: g_app_profile.mem_load.static_count++; break;
                case 1: g_app_profile.mem_store.static_count++; break;
                case 2: g_app_profile.mem_stack.static_count++; break;
            }
            break;
        case INST_CONTROL:
            subtype = GetControlFlowSubtype(ins);
            switch (subtype) {
                case 0: g_app_profile.ctrl_jmp.static_count++; break;
                case 1: g_app_profile.ctrl_jcc.static_count++; break;
                case 2: g_app_profile.ctrl_call.static_count++; break;
                case 3: g_app_profile.ctrl_ret.static_count++; break;
            }
            break;
        case INST_LOGIC:
            subtype = GetLogicSubtype(ins);
            switch (subtype) {
                case 0: g_app_profile.logic_bitwise.static_count++; break;
                case 1: g_app_profile.logic_shift.static_count++; break;
            }
            break;
        case INST_SIMD:
            subtype = GetSIMDSubtype(ins);
            switch (subtype) {
                case 0: g_app_profile.simd_sse.static_count++; break;
                case 1: g_app_profile.simd_avx.static_count++; break;
                case 2: g_app_profile.simd_avx512.static_count++; break;
            }
            break;
        default:
            break;
    }

    // 插入动态计数回调
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountInstCategory,
                   IARG_UINT32, category,
                   IARG_UINT32, subtype,
                   IARG_END);

    // ========== A类：浮点指令分析 ==========
    if (category == INST_FLOAT) {
        g_app_profile.float_inst_static++;

        FloatOpType op_type = GetFloatOpType(ins);
        FloatPrecision precision = GetFloatPrecision(ins);
        BOOL is_simd = IsSIMDFloatInstruction(ins);

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountFloatInstruction,
                       IARG_UINT32, op_type,
                       IARG_UINT32, precision,
                       IARG_BOOL, is_simd,
                       IARG_END);
    }

    // ========== B类：误差吸收能力相关指令 ==========
    if (IsCompareInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountCompareInstruction,
                       IARG_PTR, NULL, IARG_END);
    }

    if (IsTestInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountTestInstruction,
                       IARG_PTR, NULL, IARG_END);
    }

    if (IsSaturateInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountSaturateInstruction,
                       IARG_PTR, NULL, IARG_END);
    }

    if (IsMinMaxInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountMinMaxInstruction,
                       IARG_PTR, NULL, IARG_END);
    }

    if (IsAbsInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountAbsInstruction,
                       IARG_PTR, NULL, IARG_END);
    }

    if (IsRoundInstruction(ins)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CountRoundInstruction,
                       IARG_PTR, NULL, IARG_END);
    }
}

/**
 * @brief 函数级插桩
 */
VOID InstrumentRoutine(RTN rtn, VOID* v) {
    RTN_Open(rtn);

    string func_name = RTN_Name(rtn);
    IMG img = SEC_Img(RTN_Sec(rtn));

    // 判断是否为库函数
    BOOL is_lib_func = !IMG_IsMainExecutable(img);

    // 分类库函数
    LibCategory category = LIB_USER;
    if (is_lib_func) {
        category = ClassifyLibFunction(func_name);
    }

    // 为函数名创建持久化存储
    string* persistent_name = new string(func_name);

    // 在函数入口插桩
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CountFunctionCall,
                   IARG_PTR, persistent_name,
                   IARG_BOOL, is_lib_func,
                   IARG_UINT32, category,
                   IARG_END);

    RTN_Close(rtn);
}

/**
 * @brief 镜像加载回调
 */
VOID ImageLoad(IMG img, VOID* v) {
    string img_name = IMG_Name(img);
    ADDRINT low = IMG_LowAddress(img);
    ADDRINT high = IMG_HighAddress(img);
    BOOL is_main = IMG_IsMainExecutable(img);

    // 记录镜像信息
    g_loaded_images.push_back(ImageInfo(low, high, img_name, is_main));

    // 记录主程序名称
    if (is_main) {
        // 提取文件名
        size_t pos = img_name.find_last_of("/\\");
        if (pos != string::npos) {
            g_app_profile.main_image_name = img_name.substr(pos + 1);
        } else {
            g_app_profile.main_image_name = img_name;
        }
    }

    if (KnobVerbose.Value()) {
        cerr << "[AppProfiler] Loaded: " << img_name
             << " [" << hex << low << "-" << high << "]"
             << (is_main ? " (main)" : " (lib)") << dec << endl;
    }
}

// ============================================================================
// JSON输出
// ============================================================================

/**
 * @brief 转义JSON字符串
 */
string EscapeJsonString(const string& str) {
    string result;
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

/**
 * @brief 输出JSON结果
 */
VOID OutputJSON(const string& filename) {
    ofstream out(filename.c_str());
    if (!out.is_open()) {
        cerr << "[AppProfiler] Error: Cannot open output file: " << filename << endl;
        return;
    }

    out << "{\n";

    // 工具信息
    out << "  \"tool_info\": {\n";
    out << "    \"name\": \"Application Profiler\",\n";
    out << "    \"version\": \"2.0\",\n";
    out << "    \"main_image\": \"" << EscapeJsonString(g_app_profile.main_image_name) << "\"\n";
    out << "  },\n";

    // D类：指令类型分布指标
    out << "  \"instruction_distribution\": {\n";
    out << "    \"total\": {\n";
    out << "      \"static_count\": " << g_app_profile.total_inst_static << ",\n";
    out << "      \"exec_count\": " << g_app_profile.total_inst_exec << "\n";
    out << "    },\n";

    // 指令大类分布
    out << "    \"by_category\": {\n";
    for (int i = 0; i < INST_CATEGORY_COUNT; i++) {
        out << "      \"" << InstCategoryNames[i] << "\": {\n";
        out << "        \"static_count\": " << g_app_profile.inst_category[i].static_count << ",\n";
        out << "        \"exec_count\": " << g_app_profile.inst_category[i].exec_count << "\n";
        out << "      }";
        if (i < INST_CATEGORY_COUNT - 1) out << ",";
        out << "\n";
    }
    out << "    },\n";

    // 整数算术指令细分
    out << "    \"int_arithmetic_details\": {\n";
    out << "      \"add\": { \"static_count\": " << g_app_profile.int_add.static_count
        << ", \"exec_count\": " << g_app_profile.int_add.exec_count << " },\n";
    out << "      \"sub\": { \"static_count\": " << g_app_profile.int_sub.static_count
        << ", \"exec_count\": " << g_app_profile.int_sub.exec_count << " },\n";
    out << "      \"mul\": { \"static_count\": " << g_app_profile.int_mul.static_count
        << ", \"exec_count\": " << g_app_profile.int_mul.exec_count << " },\n";
    out << "      \"div\": { \"static_count\": " << g_app_profile.int_div.static_count
        << ", \"exec_count\": " << g_app_profile.int_div.exec_count << " }\n";
    out << "    },\n";

    // 内存访问指令细分
    out << "    \"memory_details\": {\n";
    out << "      \"load\": { \"static_count\": " << g_app_profile.mem_load.static_count
        << ", \"exec_count\": " << g_app_profile.mem_load.exec_count << " },\n";
    out << "      \"store\": { \"static_count\": " << g_app_profile.mem_store.static_count
        << ", \"exec_count\": " << g_app_profile.mem_store.exec_count << " },\n";
    out << "      \"stack\": { \"static_count\": " << g_app_profile.mem_stack.static_count
        << ", \"exec_count\": " << g_app_profile.mem_stack.exec_count << " }\n";
    out << "    },\n";

    // 控制流指令细分
    out << "    \"control_flow_details\": {\n";
    out << "      \"jmp\": { \"static_count\": " << g_app_profile.ctrl_jmp.static_count
        << ", \"exec_count\": " << g_app_profile.ctrl_jmp.exec_count << " },\n";
    out << "      \"jcc\": { \"static_count\": " << g_app_profile.ctrl_jcc.static_count
        << ", \"exec_count\": " << g_app_profile.ctrl_jcc.exec_count << " },\n";
    out << "      \"call\": { \"static_count\": " << g_app_profile.ctrl_call.static_count
        << ", \"exec_count\": " << g_app_profile.ctrl_call.exec_count << " },\n";
    out << "      \"ret\": { \"static_count\": " << g_app_profile.ctrl_ret.static_count
        << ", \"exec_count\": " << g_app_profile.ctrl_ret.exec_count << " }\n";
    out << "    },\n";

    // 逻辑运算指令细分
    out << "    \"logic_details\": {\n";
    out << "      \"bitwise\": { \"static_count\": " << g_app_profile.logic_bitwise.static_count
        << ", \"exec_count\": " << g_app_profile.logic_bitwise.exec_count << " },\n";
    out << "      \"shift\": { \"static_count\": " << g_app_profile.logic_shift.static_count
        << ", \"exec_count\": " << g_app_profile.logic_shift.exec_count << " }\n";
    out << "    },\n";

    // SIMD指令细分
    out << "    \"simd_details\": {\n";
    out << "      \"sse\": { \"static_count\": " << g_app_profile.simd_sse.static_count
        << ", \"exec_count\": " << g_app_profile.simd_sse.exec_count << " },\n";
    out << "      \"avx\": { \"static_count\": " << g_app_profile.simd_avx.static_count
        << ", \"exec_count\": " << g_app_profile.simd_avx.exec_count << " },\n";
    out << "      \"avx512\": { \"static_count\": " << g_app_profile.simd_avx512.static_count
        << ", \"exec_count\": " << g_app_profile.simd_avx512.exec_count << " }\n";
    out << "    }\n";
    out << "  },\n";

    // A类：数值敏感性指标
    out << "  \"numeric_sensitivity\": {\n";
    out << "    \"float_inst_static\": " << g_app_profile.float_inst_static << ",\n";
    out << "    \"float_inst_exec\": " << g_app_profile.float_inst_exec << ",\n";
    out << "    \"operation_distribution\": {\n";
    out << "      \"add_sub\": " << g_app_profile.float_add_sub_exec << ",\n";
    out << "      \"mul\": " << g_app_profile.float_mul_exec << ",\n";
    out << "      \"div\": " << g_app_profile.float_div_exec << ",\n";
    out << "      \"sqrt\": " << g_app_profile.float_sqrt_exec << ",\n";
    out << "      \"fma\": " << g_app_profile.float_fma_exec << ",\n";
    out << "      \"cmp\": " << g_app_profile.float_cmp_exec << ",\n";
    out << "      \"cvt\": " << g_app_profile.float_cvt_exec << "\n";
    out << "    },\n";
    out << "    \"precision_distribution\": {\n";
    out << "      \"single\": " << g_app_profile.single_precision_exec << ",\n";
    out << "      \"double\": " << g_app_profile.double_precision_exec << ",\n";
    out << "      \"x87\": " << g_app_profile.x87_exec << ",\n";
    out << "      \"simd\": " << g_app_profile.simd_float_exec << "\n";
    out << "    }\n";
    out << "  },\n";

    // B类：误差吸收能力指标
    out << "  \"error_absorption\": {\n";
    out << "    \"cmp_inst_exec\": " << g_app_profile.cmp_inst_exec << ",\n";
    out << "    \"test_inst_exec\": " << g_app_profile.test_inst_exec << ",\n";
    out << "    \"saturate_inst_exec\": " << g_app_profile.saturate_inst_exec << ",\n";
    out << "    \"minmax_inst_exec\": " << g_app_profile.minmax_inst_exec << ",\n";
    out << "    \"abs_inst_exec\": " << g_app_profile.abs_inst_exec << ",\n";
    out << "    \"round_inst_exec\": " << g_app_profile.round_inst_exec << "\n";
    out << "  },\n";

    // C类：库调用指标
    out << "  \"library_calls\": {\n";
    out << "    \"total_lib_calls\": " << g_app_profile.total_lib_calls << ",\n";
    out << "    \"user_func_calls\": " << g_app_profile.user_func_calls << ",\n";
    out << "    \"by_category\": {\n";

    // 输出各分类统计（排除LIB_USER）
    bool first_category = true;
    for (int i = 0; i < LIB_USER; i++) {
        LibCallStats& stats = g_app_profile.lib_stats[i];
        if (stats.call_count > 0) {
            if (!first_category) out << ",\n";
            first_category = false;

            out << "      \"" << LibCategoryNames[i] << "\": {\n";
            out << "        \"call_count\": " << stats.call_count << ",\n";
            out << "        \"unique_funcs\": " << stats.unique_funcs.size() << ",\n";
            out << "        \"functions\": [";

            bool first_func = true;
            for (const string& func : stats.unique_funcs) {
                if (!first_func) out << ", ";
                first_func = false;
                out << "\"" << EscapeJsonString(func) << "\"";
            }
            out << "]\n";
            out << "      }";
        }
    }
    out << "\n    }\n";
    out << "  },\n";

    // 全局统计
    out << "  \"global_stats\": {\n";
    out << "    \"total_inst_static\": " << g_app_profile.total_inst_static << ",\n";
    out << "    \"total_inst_exec\": " << g_app_profile.total_inst_exec << ",\n";
    out << "    \"total_func_calls\": " << g_app_profile.total_func_calls << "\n";
    out << "  }\n";

    out << "}\n";

    out.close();

    if (KnobVerbose.Value()) {
        cerr << "[AppProfiler] Results written to: " << filename << endl;
    }
}

/**
 * @brief 程序结束回调
 */
VOID Fini(INT32 code, VOID* v) {
    OutputJSON(KnobOutputFile.Value());

    // 打印摘要
    cerr << "\n========== Application Profile Summary ==========\n";
    cerr << "Main Image: " << g_app_profile.main_image_name << "\n";
    cerr << "\n--- Instruction Distribution ---\n";
    cerr << "Total Instructions: " << g_app_profile.total_inst_exec
         << " (static: " << g_app_profile.total_inst_static << ")\n";
    for (int i = 0; i < INST_CATEGORY_COUNT; i++) {
        if (g_app_profile.inst_category[i].exec_count > 0) {
            cerr << "  - " << InstCategoryNames[i] << ": "
                 << g_app_profile.inst_category[i].exec_count
                 << " (static: " << g_app_profile.inst_category[i].static_count << ")\n";
        }
    }
    cerr << "\n--- Float Instructions ---\n";
    cerr << "Float Instructions: " << g_app_profile.float_inst_exec
         << " (static: " << g_app_profile.float_inst_static << ")\n";
    cerr << "  - Add/Sub: " << g_app_profile.float_add_sub_exec << "\n";
    cerr << "  - Mul: " << g_app_profile.float_mul_exec << "\n";
    cerr << "  - Div: " << g_app_profile.float_div_exec << "\n";
    cerr << "  - Sqrt: " << g_app_profile.float_sqrt_exec << "\n";
    cerr << "  - FMA: " << g_app_profile.float_fma_exec << "\n";
    cerr << "\n--- Function Calls ---\n";
    cerr << "Total Function Calls: " << g_app_profile.total_func_calls << "\n";
    cerr << "  - Library Calls: " << g_app_profile.total_lib_calls << "\n";
    cerr << "  - User Calls: " << g_app_profile.user_func_calls << "\n";
    cerr << "\nOutput: " << KnobOutputFile.Value() << "\n";
    cerr << "=================================================\n";
}

/**
 * @brief 打印使用帮助
 */
INT32 Usage() {
    cerr << "Application Profiler - 应用维度算法特性剖析工具\n\n";
    cerr << "用法: pin -t app_profiler.so [options] -- <program> [args]\n\n";
    cerr << "选项:\n";
    cerr << "  -o <file>    输出JSON文件路径 (默认: app_profile.json)\n";
    cerr << "  -v           详细输出模式\n";
    cerr << "\n";
    return -1;
}

/**
 * @brief 主函数
 */
int main(int argc, char* argv[]) {
    // 初始化Pin
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    // 初始化锁
    PIN_InitLock(&g_lock);

    // 注册回调
    IMG_AddInstrumentFunction(ImageLoad, NULL);
    INS_AddInstrumentFunction(InstrumentInstruction, NULL);
    RTN_AddInstrumentFunction(InstrumentRoutine, NULL);
    PIN_AddFiniFunction(Fini, NULL);

    cerr << "[AppProfiler] Starting analysis...\n";

    // 开始执行
    PIN_StartProgram();

    return 0;
}
