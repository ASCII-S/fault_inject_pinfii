/**
 * @file app_profiler.h
 * @brief 应用维度算法特性剖析工具 - 数据结构定义
 *
 * 用于分析应用程序的算法特性，包括：
 * - 数值敏感性：浮点运算分类、精度分布
 * - 误差吸收能力：比较指令、饱和运算等
 * - 外部库调用：库函数分类统计
 */

#ifndef APP_PROFILER_H
#define APP_PROFILER_H

#include "pin.H"
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

// ============================================================================
// 枚举定义
// ============================================================================

/**
 * @brief 指令大类
 */
enum InstCategory {
    INST_INT_ARITH,      // 整数算术运算 (ADD, SUB, MUL, DIV, INC, DEC, NEG, IMUL, IDIV)
    INST_FLOAT,          // 浮点运算 (所有浮点指令)
    INST_MEMORY,         // 内存访问 (LOAD, STORE, LEA, PUSH, POP)
    INST_CONTROL,        // 控制流 (JMP, Jcc, CALL, RET, LOOP)
    INST_LOGIC,          // 逻辑运算 (AND, OR, XOR, NOT, SHL, SHR, SAR, ROL, ROR)
    INST_MOV,            // 数据移动 (MOV, MOVSX, MOVZX, XCHG, CMOVcc)
    INST_SIMD,           // SIMD向量 (SSE, AVX, AVX2, AVX-512)
    INST_OTHER,          // 其他指令 (NOP, CPUID, RDTSC, etc.)
    INST_CATEGORY_COUNT
};

/**
 * @brief 指令大类名称映射
 */
static const char* InstCategoryNames[] = {
    "int_arithmetic",
    "float",
    "memory",
    "control_flow",
    "logic",
    "mov",
    "simd",
    "other"
};

/**
 * @brief 浮点运算类型
 */
enum FloatOpType {
    FLOAT_ADD_SUB,   // 加减运算
    FLOAT_MUL,       // 乘法运算
    FLOAT_DIV,       // 除法运算（敏感运算）
    FLOAT_SQRT,      // 开方运算（敏感运算）
    FLOAT_FMA,       // 融合乘加
    FLOAT_CMP,       // 浮点比较
    FLOAT_CVT,       // 精度转换
    FLOAT_OTHER      // 其他浮点运算
};

/**
 * @brief 浮点精度类型
 */
enum FloatPrecision {
    PREC_SINGLE,     // 单精度 (float, SS/PS后缀)
    PREC_DOUBLE,     // 双精度 (double, SD/PD后缀)
    PREC_EXTENDED,   // 扩展精度 (x87)
    PREC_UNKNOWN     // 未知精度
};

/**
 * @brief 库函数分类
 */
enum LibCategory {
    LIB_MATH,        // 数学库 (sin, cos, exp, log, pow, sqrt...)
    LIB_BLAS,        // BLAS库 (dgemm, daxpy, ddot...)
    LIB_LAPACK,      // LAPACK库 (dgetrf, dgetrs, dpotrf...)
    LIB_MEMORY,      // 内存管理 (malloc, free, memcpy, memset...)
    LIB_IO,          // IO库 (printf, scanf, fopen, fread...)
    LIB_STRING,      // 字符串库 (strlen, strcpy, strcmp...)
    LIB_MPI,         // MPI库 (MPI_Send, MPI_Recv, MPI_Bcast...)
    LIB_OMP,         // OpenMP库 (omp_get_thread_num...)
    LIB_PTHREAD,     // Pthread库 (pthread_create, pthread_mutex...)
    LIB_OTHER,       // 其他库函数
    LIB_USER,        // 用户函数（非库函数）
    LIB_CATEGORY_COUNT
};

/**
 * @brief 库分类名称映射
 */
static const char* LibCategoryNames[] = {
    "math_lib",
    "blas_lib",
    "lapack_lib",
    "memory_lib",
    "io_lib",
    "string_lib",
    "mpi_lib",
    "omp_lib",
    "pthread_lib",
    "other_lib",
    "user_func"
};

// ============================================================================
// 数据结构定义
// ============================================================================

/**
 * @brief 库调用统计
 */
struct LibCallStats {
    UINT64 call_count;           // 调用次数
    set<string> unique_funcs;    // 不同函数集合

    LibCallStats() : call_count(0) {}
};

/**
 * @brief 指令分布统计（静态+动态）
 */
struct InstDistribution {
    UINT32 static_count;         // 静态数量
    UINT64 exec_count;           // 动态执行次数

    InstDistribution() : static_count(0), exec_count(0) {}
};

/**
 * @brief 镜像信息（用于判断地址是否属于库）
 */
struct ImageInfo {
    ADDRINT low_addr;            // 起始地址
    ADDRINT high_addr;           // 结束地址
    string name;                 // 镜像名称
    bool is_main_executable;     // 是否为主程序

    ImageInfo() : low_addr(0), high_addr(0), is_main_executable(false) {}
    ImageInfo(ADDRINT low, ADDRINT high, const string& n, bool is_main)
        : low_addr(low), high_addr(high), name(n), is_main_executable(is_main) {}
};

/**
 * @brief 应用剖析数据
 */
struct AppProfile {
    // ========== D类：指令类型分布指标 ==========

    // 指令大类分布
    InstDistribution inst_category[INST_CATEGORY_COUNT];

    // 整数算术指令细分
    InstDistribution int_add;            // ADD/INC
    InstDistribution int_sub;            // SUB/DEC/NEG
    InstDistribution int_mul;            // MUL/IMUL
    InstDistribution int_div;            // DIV/IDIV

    // 内存访问指令细分
    InstDistribution mem_load;           // LOAD
    InstDistribution mem_store;          // STORE
    InstDistribution mem_stack;          // PUSH/POP

    // 控制流指令细分
    InstDistribution ctrl_jmp;           // JMP (无条件跳转)
    InstDistribution ctrl_jcc;           // Jcc (条件跳转)
    InstDistribution ctrl_call;          // CALL
    InstDistribution ctrl_ret;           // RET

    // 逻辑运算指令细分
    InstDistribution logic_bitwise;      // AND/OR/XOR/NOT
    InstDistribution logic_shift;        // SHL/SHR/SAR/ROL/ROR

    // SIMD指令细分
    InstDistribution simd_sse;           // SSE
    InstDistribution simd_avx;           // AVX/AVX2
    InstDistribution simd_avx512;        // AVX-512

    // ========== A类：数值敏感性指标 ==========

    // 浮点指令统计
    UINT32 float_inst_static;        // 浮点指令静态数量
    UINT64 float_inst_exec;          // 浮点指令执行次数

    // 浮点运算类型分布
    UINT64 float_add_sub_exec;       // 加减执行次数
    UINT64 float_mul_exec;           // 乘法执行次数
    UINT64 float_div_exec;           // 除法执行次数（敏感）
    UINT64 float_sqrt_exec;          // 开方执行次数（敏感）
    UINT64 float_fma_exec;           // FMA执行次数
    UINT64 float_cmp_exec;           // 浮点比较执行次数
    UINT64 float_cvt_exec;           // 精度转换执行次数

    // 精度分布
    UINT64 single_precision_exec;    // 单精度执行次数
    UINT64 double_precision_exec;    // 双精度执行次数
    UINT64 x87_exec;                 // x87扩展精度执行次数
    UINT64 simd_float_exec;          // SIMD浮点执行次数

    // ========== B类：误差吸收能力指标 ==========

    UINT64 cmp_inst_exec;            // 比较指令执行次数
    UINT64 test_inst_exec;           // TEST指令执行次数
    UINT64 saturate_inst_exec;       // 饱和运算执行次数
    UINT64 minmax_inst_exec;         // MIN/MAX指令执行次数
    UINT64 abs_inst_exec;            // 绝对值指令执行次数
    UINT64 round_inst_exec;          // 舍入指令执行次数

    // ========== C类：外部库调用指标 ==========

    UINT64 total_lib_calls;          // 外部库调用总次数
    UINT64 user_func_calls;          // 用户函数调用次数

    // 分类统计
    LibCallStats lib_stats[LIB_CATEGORY_COUNT];

    // ========== 全局统计 ==========

    UINT32 total_inst_static;        // 总指令静态数量
    UINT64 total_inst_exec;          // 总指令执行次数
    UINT64 total_func_calls;         // 总函数调用次数

    // 主程序名称
    string main_image_name;

    /**
     * @brief 构造函数，初始化所有计数器
     */
    AppProfile() :
        // 数值敏感性
        float_inst_static(0),
        float_inst_exec(0),
        float_add_sub_exec(0),
        float_mul_exec(0),
        float_div_exec(0),
        float_sqrt_exec(0),
        float_fma_exec(0),
        float_cmp_exec(0),
        float_cvt_exec(0),
        single_precision_exec(0),
        double_precision_exec(0),
        x87_exec(0),
        simd_float_exec(0),
        // 误差吸收能力
        cmp_inst_exec(0),
        test_inst_exec(0),
        saturate_inst_exec(0),
        minmax_inst_exec(0),
        abs_inst_exec(0),
        round_inst_exec(0),
        // 库调用
        total_lib_calls(0),
        user_func_calls(0),
        // 全局
        total_inst_static(0),
        total_inst_exec(0),
        total_func_calls(0)
    {}
};

// ============================================================================
// 全局变量声明
// ============================================================================

extern AppProfile g_app_profile;
extern vector<ImageInfo> g_loaded_images;
extern PIN_LOCK g_lock;

#endif // APP_PROFILER_H
