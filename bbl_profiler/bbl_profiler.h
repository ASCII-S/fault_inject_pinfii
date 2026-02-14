/*
 * bbl_profiler.h - BBL维度剖析工具头文件
 *
 * 功能：定义BBL剖析的核心数据结构
 * 目的：为学术研究提供BBL级别的执行特性数据，分析BBL弹性/可修复性
 *
 * 版本：1.0
 */

#ifndef BBL_PROFILER_H
#define BBL_PROFILER_H

#include <string>
#include <map>
#include <set>
#include "pin.H"

using std::string;
using std::map;
using std::set;
using std::pair;

/**
 * BBL剖析数据结构
 * 包含六类指标：基本属性、执行统计、控制流、计算特征、数据依赖、边统计
 * 命名规范：_static 表示静态指令数量，_exec 表示动态执行次数
 */
struct BBLProfile {
    // ========== A类: 基本属性（静态） ==========
    ADDRINT bbl_addr;               // BBL起始地址（绝对地址）
    ADDRINT bbl_offset;             // 相对主程序基址的偏移
    ADDRINT inst_addr_start;        // BBL内第一条指令地址
    ADDRINT inst_addr_end;          // BBL内最后一条指令地址
    string function_name;           // 所属函数名
    UINT32 inst_static;             // BBL内静态指令数量
    UINT32 bbl_size_bytes;          // BBL大小（字节）

    // ========== B类: 执行统计（动态） ==========
    UINT64 exec_count;              // BBL执行次数
    UINT64 inst_exec;               // 总指令执行次数

    // ========== C类: 控制流特征 ==========
    UINT32 succ_count;              // 后继BBL数量（1=顺序/无条件，2=条件分支）
    bool is_loop_header;            // 是否是循环头（有回边指向）
    bool is_function_entry;         // 是否是函数入口块
    bool is_function_exit;          // 是否是函数出口块（含RET）
    string terminator_type;         // 终结指令类型
    bool has_indirect_branch;       // 是否含间接跳转

    // 回边目标（用于标记循环头）
    set<ADDRINT> backedge_targets;  // 运行时状态：此BBL的回边目标

    // ========== D类: 计算特征 ==========
    // 静态指标
    UINT32 mem_read_static;         // 内存读指令静态数量
    UINT32 mem_write_static;        // 内存写指令静态数量
    UINT32 mem_inst_static;         // 访存指令静态数量（不重复）
    UINT32 arith_static;            // 算术指令静态数量
    UINT32 logic_static;            // 逻辑指令静态数量
    UINT32 float_static;            // 浮点指令静态数量
    UINT32 simd_static;             // SIMD指令静态数量
    UINT32 data_movement_static;    // 数据移动指令静态数量
    UINT32 pure_compute_static;     // 纯计算指令静态数量

    // 动态指标
    UINT64 mem_read_exec;           // 内存读执行次数
    UINT64 mem_write_exec;          // 内存写执行次数
    UINT64 mem_inst_exec;           // 访存指令执行次数
    UINT64 arith_exec;              // 算术指令执行次数
    UINT64 logic_exec;              // 逻辑指令执行次数
    UINT64 float_exec;              // 浮点指令执行次数
    UINT64 simd_exec;               // SIMD指令执行次数
    UINT64 data_movement_exec;      // 数据移动指令执行次数
    UINT64 pure_compute_exec;       // 纯计算指令执行次数

    // ========== D2类: 内存访问模式（动态） ==========
    // 内存读访问模式
    UINT64 seq_read_exec;           // 连续读次数（地址差 <= 缓存行大小）
    UINT64 stride_read_exec;        // 步长读次数（固定stride）
    UINT64 random_read_exec;        // 随机读次数
    // 内存写访问模式
    UINT64 seq_write_exec;          // 连续写次数
    UINT64 stride_write_exec;       // 步长写次数
    UINT64 random_write_exec;       // 随机写次数
    // 运行时辅助（用于访问模式分析，不输出到JSON）
    ADDRINT last_read_addr;         // 上一次读地址
    ADDRINT last_write_addr;        // 上一次写地址
    INT64 last_read_stride;         // 上一次读stride
    INT64 last_write_stride;        // 上一次写stride
    bool has_last_read;             // 是否有上一次读
    bool has_last_write;            // 是否有上一次写

    // ========== E类: 数据依赖特征（简化版） ==========
    // 静态指标
    UINT32 live_in_count;           // 外部输入寄存器数（定义前使用）
    UINT32 live_out_count;          // 可能输出寄存器数（最后定义）
    UINT32 def_count;               // BBL内寄存器定义总数
    UINT32 use_count;               // BBL内寄存器使用总数
    UINT32 reg_read_static;         // 静态寄存器读操作数
    UINT32 reg_write_static;        // 静态寄存器写操作数

    // 动态指标
    UINT64 reg_read_exec;           // 寄存器读执行次数
    UINT64 reg_write_exec;          // 寄存器写执行次数
    UINT64 mem_to_reg_exec;         // 内存→寄存器传递次数
    UINT64 reg_to_mem_exec;         // 寄存器→内存传递次数

    // 运行时辅助（不输出到JSON）
    set<REG> def_regs;              // BBL内已定义的寄存器
    set<REG> use_before_def;        // 定义前使用的寄存器（live_in）
    set<REG> last_def_regs;         // BBL内最后定义的寄存器（用于计算live_out）
    bool static_analyzed;           // 是否已完成静态分析

    // 构造函数：初始化所有字段
    BBLProfile() :
        // A类: 基本属性
        bbl_addr(0),
        bbl_offset(0),
        inst_addr_start(0),
        inst_addr_end(0),
        inst_static(0),
        bbl_size_bytes(0),
        // B类: 执行统计
        exec_count(0),
        inst_exec(0),
        // C类: 控制流特征
        succ_count(0),
        is_loop_header(false),
        is_function_entry(false),
        is_function_exit(false),
        terminator_type("unknown"),
        has_indirect_branch(false),
        // D类: 计算特征（静态）
        mem_read_static(0),
        mem_write_static(0),
        mem_inst_static(0),
        arith_static(0),
        logic_static(0),
        float_static(0),
        simd_static(0),
        data_movement_static(0),
        pure_compute_static(0),
        // D类: 计算特征（动态）
        mem_read_exec(0),
        mem_write_exec(0),
        mem_inst_exec(0),
        arith_exec(0),
        logic_exec(0),
        float_exec(0),
        simd_exec(0),
        data_movement_exec(0),
        pure_compute_exec(0),
        // D2类: 内存访问模式（动态）
        seq_read_exec(0),
        stride_read_exec(0),
        random_read_exec(0),
        seq_write_exec(0),
        stride_write_exec(0),
        random_write_exec(0),
        last_read_addr(0),
        last_write_addr(0),
        last_read_stride(0),
        last_write_stride(0),
        has_last_read(false),
        has_last_write(false),
        // E类: 数据依赖（静态）
        live_in_count(0),
        live_out_count(0),
        def_count(0),
        use_count(0),
        reg_read_static(0),
        reg_write_static(0),
        // E类: 数据依赖（动态）
        reg_read_exec(0),
        reg_write_exec(0),
        mem_to_reg_exec(0),
        reg_to_mem_exec(0),
        // 运行时辅助
        static_analyzed(false)
    {
    }
};

/**
 * 边剖析数据结构
 * 记录BBL间的控制流转移
 */
struct EdgeProfile {
    ADDRINT from_bbl;               // 源BBL地址
    ADDRINT to_bbl;                 // 目标BBL地址
    UINT64 exec_count;              // 边执行次数
    string edge_type;               // 边类型：fallthrough/taken/indirect/call/ret

    // 构造函数
    EdgeProfile() :
        from_bbl(0),
        to_bbl(0),
        exec_count(0),
        edge_type("unknown")
    {
    }

    EdgeProfile(ADDRINT from, ADDRINT to, const string& type) :
        from_bbl(from),
        to_bbl(to),
        exec_count(0),
        edge_type(type)
    {
    }
};

/**
 * 边的键类型（用于map）
 */
typedef pair<ADDRINT, ADDRINT> EdgeKey;

#endif // BBL_PROFILER_H
