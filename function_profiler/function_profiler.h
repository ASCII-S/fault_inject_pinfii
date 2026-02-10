/*
 * function_profiler.h - 函数维度剖析工具头文件
 *
 * 功能：定义函数剖析的核心数据结构
 * 目的：为学术研究提供函数级别的执行特性数据
 *
 * 作者：基于Pin工具开发
 * 版本：1.0
 */

#ifndef FUNCTION_PROFILER_H
#define FUNCTION_PROFILER_H

#include <string>
#include <map>
#include <set>
#include "pin.H"

using std::string;
using std::map;
using std::set;

/**
 * 函数剖析数据结构
 * 包含三类指标：执行统计、数据流、控制流
 * 命名规范：_static 表示静态指令数量，_exec 表示动态执行次数
 */
struct FunctionProfile {
    // ========== 基本信息 ==========
    string function_name;           // 函数名
    ADDRINT start_addr;             // 起始地址(绝对地址)
    ADDRINT end_addr;               // 结束地址(绝对地址)
    ADDRINT offset_start;           // 起始偏移(相对主程序基址)
    ADDRINT offset_end;             // 结束偏移(相对主程序基址)
    UINT32 function_size_bytes;     // 函数大小(字节)

    // ========== A类: 执行统计 ==========
    UINT64 call_exec;               // 函数调用执行次数(动态)
    UINT64 inst_exec;               // 总指令执行次数(动态)
    UINT32 inst_static;             // 静态指令数量(静态)

    // ========== B1类: 数据流特性 ==========
    UINT64 mem_read_exec;           // 内存读执行次数(动态)
    UINT64 mem_write_exec;          // 内存写执行次数(动态)
    UINT64 mem_inst_exec;           // 访存指令执行次数(动态，不重复计数)
    // B1类新增静态指标
    UINT32 mem_read_static;         // 内存读指令静态数量
    UINT32 mem_write_static;        // 内存写指令静态数量
    UINT32 mem_inst_static;         // 访存指令静态数量（不重复计数）

    // ========== B1.5类: 内存访问模式 ==========
    // 运行时状态（不输出到JSON）
    ADDRINT last_read_addr;         // 上一次读地址
    ADDRINT last_write_addr;        // 上一次写地址
    INT64 last_read_stride;         // 上一次读stride
    INT64 last_write_stride;        // 上一次写stride
    bool has_last_read;             // 是否有上一次读记录
    bool has_last_write;            // 是否有上一次写记录

    // 读访问模式统计
    UINT64 seq_read_exec;           // 连续读执行次数(动态)
    UINT64 stride_read_exec;        // 步长读执行次数(动态)
    UINT64 random_read_exec;        // 随机读执行次数(动态)

    // 写访问模式统计
    UINT64 seq_write_exec;          // 连续写执行次数(动态)
    UINT64 stride_write_exec;       // 步长写执行次数(动态)
    UINT64 random_write_exec;       // 随机写执行次数(动态)

    // ========== B2类: 计算特性 ==========
    UINT32 arith_static;            // 算术指令静态数量: ADD/SUB/MUL/DIV/INC/DEC等
    UINT32 logic_static;            // 逻辑指令静态数量: AND/OR/XOR/NOT/SHL/SHR等
    UINT32 float_static;            // 浮点指令静态数量
    UINT32 simd_static;             // SIMD/向量指令静态数量: SSE/AVX等
    UINT64 arith_exec;              // 算术指令执行次数(动态)
    UINT64 float_exec;              // 浮点指令执行次数(动态)
    // B2类新增动态指标
    UINT64 logic_exec;              // 逻辑指令执行次数(动态)
    UINT64 simd_exec;               // SIMD指令执行次数(动态)
    UINT64 pure_compute_exec;       // 纯计算指令执行次数(动态)
    UINT64 data_movement_exec;      // 数据移动指令执行次数(动态)
    // B2类新增静态指标
    UINT32 pure_compute_static;     // 纯计算指令静态数量（不涉及内存的计算指令）
    UINT32 data_movement_static;    // 数据移动指令静态数量（MOV类）
    // B2类细化指令类型（用于熵计算）
    UINT32 compare_static;          // 比较指令静态数量: CMP/TEST等
    UINT64 compare_exec;            // 比较指令执行次数(动态)
    UINT32 stack_static;            // 栈操作指令静态数量: PUSH/POP等
    UINT64 stack_exec;              // 栈操作指令执行次数(动态)
    UINT32 string_static;           // 字符串指令静态数量: REP/MOVS/STOS等
    UINT64 string_exec;             // 字符串指令执行次数(动态)
    UINT32 nop_static;              // NOP指令静态数量
    UINT64 nop_exec;                // NOP指令执行次数(动态)
    UINT32 other_static;            // 其他未分类指令静态数量
    UINT64 other_exec;              // 其他未分类指令执行次数(动态)

    // ========== C类: 控制流特性 ==========
    UINT32 branch_static;           // 分支指令静态数量
    UINT64 branch_exec;             // 分支指令执行次数(动态)
    UINT32 loop_static;             // 循环静态数量(回边检测)
    UINT32 return_static;           // 返回点静态数量
    UINT32 call_static;             // 函数调用静态数量
    UINT64 call_other_exec;         // 调用其他函数执行次数(动态)
    UINT64 indirect_exec;           // 间接跳转执行次数(动态)

    // ========== I类: 函数间调用图 ==========
    UINT32 fan_in;                  // 入度：有多少不同函数调用本函数(动态)
    UINT32 fan_out;                 // 出度：本函数调用多少不同函数(动态)
    set<ADDRINT> callers_set;       // 运行时状态：调用本函数的函数地址集合(不输出)
    set<ADDRINT> callees_set;       // 运行时状态：本函数调用的函数地址集合(不输出)

    // ========== D类: 寄存器使用 ==========
    UINT64 reg_read_exec;           // 寄存器读取执行次数(动态)
    UINT64 reg_write_exec;          // 寄存器写入执行次数(动态)
    UINT32 reg_read_static;         // 静态寄存器读操作数(静态)
    UINT32 reg_write_static;        // 静态寄存器写操作数(静态)
    UINT32 unique_reg_read;         // 使用的不同读寄存器数(静态)
    UINT32 unique_reg_write;        // 使用的不同写寄存器数(静态)
    set<REG> read_regs_set;         // 运行时状态：读寄存器集合(不输出)
    set<REG> write_regs_set;        // 运行时状态：写寄存器集合(不输出)

    // ========== E类: 控制流细化 ==========
    UINT64 branch_taken_exec;       // 分支跳转执行次数(动态)
    UINT64 branch_not_taken_exec;   // 分支未跳转执行次数(动态)
    UINT32 cond_branch_static;      // 条件分支静态数量(静态)
    UINT32 uncond_branch_static;    // 无条件跳转静态数量(静态)
    UINT64 loop_iter_total;         // 循环总迭代次数(动态)
    UINT32 call_depth_max;          // 最大调用深度(动态)
    UINT32 current_call_depth;      // 运行时状态：当前调用深度(不输出)
    UINT32 loop_depth_max;          // 最大循环嵌套深度(动态)
    UINT32 current_loop_depth;      // 运行时状态：当前循环嵌套深度(不输出)
    set<ADDRINT> active_loops;      // 运行时状态：当前活跃的循环回边地址(不输出)

    // ========== F类: 数据依赖 (可选启用) ==========
    UINT64 def_use_pairs;           // 定义-使用对总数(动态)
    UINT32 reg_dep_chain_max;       // 最长寄存器依赖链(动态)
    UINT64 mem_to_reg_exec;         // 内存→寄存器传递次数(动态)
    UINT64 reg_to_mem_exec;         // 寄存器→内存传递次数(动态)
    // 运行时状态：简化的寄存器跟踪(16个通用寄存器)
    UINT64 reg_last_def_id[16];     // 每个寄存器最后定义的指令ID
    UINT64 current_dyn_id;          // 当前动态指令ID

    // ========== G类: 生命周期 (可选启用) ==========
    UINT64 reg_lifetime_total;      // 寄存器值总存活指令数(动态)
    UINT64 dead_write_exec;         // 死写次数(动态)
    UINT64 first_use_dist_total;    // 定义到首次使用的总距离(动态)
    // 运行时状态
    UINT64 reg_def_id[16];          // 寄存器定义时的指令ID
    bool reg_was_used[16];          // 寄存器是否被使用过

    // ========== H类: 圈复杂度 ==========
    // 静态圈复杂度
    UINT32 bbl_static;              // 静态基本块数量 (N)
    UINT32 edge_static;             // 静态控制流边数量 (E)
    // 动态圈复杂度
    UINT64 bbl_exec;                // 基本块执行次数(动态)
    UINT32 unique_bbl_exec;         // 实际执行的唯一基本块数(动态)
    UINT32 unique_edge_exec;        // 实际执行的唯一边数(动态)
    // 运行时状态(不输出到JSON)
    set<ADDRINT> executed_bbls;                         // 执行过的BBL地址集合
    set<std::pair<ADDRINT, ADDRINT>> executed_edges;    // 执行过的边集合 (src, dst)
    ADDRINT last_bbl_addr;          // 上一个执行的BBL地址

    // 构造函数：初始化所有字段
    FunctionProfile() :
        start_addr(0), end_addr(0),
        offset_start(0), offset_end(0),
        function_size_bytes(0),
        // A类: 执行统计
        call_exec(0),
        inst_exec(0),
        inst_static(0),
        // B1类: 数据流特性
        mem_read_exec(0),
        mem_write_exec(0),
        mem_inst_exec(0),
        mem_read_static(0),
        mem_write_static(0),
        mem_inst_static(0),
        // B1.5类: 内存访问模式
        last_read_addr(0), last_write_addr(0),
        last_read_stride(0), last_write_stride(0),
        has_last_read(false), has_last_write(false),
        seq_read_exec(0), stride_read_exec(0), random_read_exec(0),
        seq_write_exec(0), stride_write_exec(0), random_write_exec(0),
        // B2类: 计算特性
        arith_static(0),
        logic_static(0),
        float_static(0),
        simd_static(0),
        arith_exec(0),
        float_exec(0),
        logic_exec(0),
        simd_exec(0),
        pure_compute_exec(0),
        data_movement_exec(0),
        pure_compute_static(0),
        data_movement_static(0),
        compare_static(0),
        compare_exec(0),
        stack_static(0),
        stack_exec(0),
        string_static(0),
        string_exec(0),
        nop_static(0),
        nop_exec(0),
        other_static(0),
        other_exec(0),
        // C类: 控制流特性
        branch_static(0),
        branch_exec(0),
        loop_static(0),
        return_static(0),
        call_static(0),
        call_other_exec(0),
        indirect_exec(0),
        // I类: 函数间调用图
        fan_in(0),
        fan_out(0),
        // D类: 寄存器使用
        reg_read_exec(0),
        reg_write_exec(0),
        reg_read_static(0),
        reg_write_static(0),
        unique_reg_read(0),
        unique_reg_write(0),
        // E类: 控制流细化
        branch_taken_exec(0),
        branch_not_taken_exec(0),
        cond_branch_static(0),
        uncond_branch_static(0),
        loop_iter_total(0),
        call_depth_max(0),
        current_call_depth(0),
        loop_depth_max(0),
        current_loop_depth(0),
        // F类: 数据依赖
        def_use_pairs(0),
        reg_dep_chain_max(0),
        mem_to_reg_exec(0),
        reg_to_mem_exec(0),
        current_dyn_id(0),
        // G类: 生命周期
        reg_lifetime_total(0),
        dead_write_exec(0),
        first_use_dist_total(0),
        // H类: 圈复杂度
        bbl_static(0),
        edge_static(0),
        bbl_exec(0),
        unique_bbl_exec(0),
        unique_edge_exec(0),
        last_bbl_addr(0)
    {
        // 初始化数组
        for (int i = 0; i < 16; i++) {
            reg_last_def_id[i] = 0;
            reg_def_id[i] = 0;
            reg_was_used[i] = false;
        }
        // set 容器自动初始化为空
    }
};

#endif // FUNCTION_PROFILER_H
