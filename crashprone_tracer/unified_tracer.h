/*
 * unified_tracer.h - 统一易崩溃指令识别与数据流溯源
 *
 * 功能：在遇到易崩溃指令时，立即对"可能导致崩溃的寄存器"进行溯源
 * 输出：每个寄存器独立的溯源链
 */

#ifndef UNIFIED_TRACER_H
#define UNIFIED_TRACER_H

#include "pin.H"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <set>
#include <vector>

// ============ 配置参数 ============
struct UnifiedConfig {
    static const uint64_t WINDOW_SIZE = 10000;  // 环形缓冲区大小
    static const int MAX_DEPTH = 10;             // 默认最大溯源深度
    static const int MAX_CRASH_REGS = 4;        // 最大崩溃寄存器数
};

// ============ 易崩溃指令类型 ============
enum CrashProneType {
    CP_NONE = 0,
    CP_MEM_WRITE = 1,      // 内存写
    CP_MEM_READ = 2,       // 内存读
    CP_INDIRECT_CF = 3,    // 间接跳转/调用
    CP_DIV = 4,            // 除法指令
    CP_INDEX_ACCESS = 5    // 带索引的内存访问
};

// 类型字符串转换
inline const char* cp_type_to_string(uint8_t cp_type) {
    switch (cp_type) {
        case CP_MEM_WRITE: return "mem_write";
        case CP_MEM_READ: return "mem_read";
        case CP_INDIRECT_CF: return "indirect_cf";
        case CP_DIV: return "div";
        case CP_INDEX_ACCESS: return "index_access";
        default: return "none";
    }
}

// ============ 环形缓冲区节点 ============
struct DynNode {
    uint64_t dyn_id;               // 动态指令 ID
    ADDRINT ip;                    // 指令地址(运行时)
    ADDRINT offset;                // 相对偏移

    REG read_regs[8];              // 读寄存器(最多8个)
    REG write_regs[4];             // 写寄存器(最多4个)
    uint8_t num_reads;
    uint8_t num_writes;

    DynNode() : dyn_id(0), ip(0), offset(0), num_reads(0), num_writes(0) {
        memset(read_regs, 0, sizeof(read_regs));
        memset(write_regs, 0, sizeof(write_regs));
    }
};

// ============ 每线程状态 (TLS) ============
struct ThreadState {
    DynNode ring_buffer[UnifiedConfig::WINDOW_SIZE];  // 环形缓冲区
    uint64_t dyn_id;                                   // 当前动态指令 ID
    std::map<REG, uint64_t> shadow_regs;              // REG -> 最后写该寄存器的 dyn_id

    ThreadState() : dyn_id(0) {}
};

// ============ 溯源源指令条目 ============
struct SourceEntry {
    ADDRINT offset;           // 源指令偏移
    std::string disasm;       // 源指令反汇编
    int depth;                // 溯源深度
    uint64_t hit_count;       // 命中次数

    SourceEntry() : offset(0), depth(0), hit_count(0) {}
};

// ============ 单个崩溃寄存器的溯源结果 ============
struct RegisterTrace {
    std::string reg_name;                    // 寄存器名称
    std::vector<SourceEntry> sources;        // 溯源到的源指令列表
};

// ============ 易崩溃指令的完整记录 ============
struct CrashProneRecord {
    ADDRINT offset;                          // 指令偏移
    std::string disasm;                      // 反汇编字符串
    uint8_t cp_type;                         // 易崩溃类型
    uint64_t exec_count;                     // 执行次数

    // 崩溃寄存器列表（用于寻址/跳转目标/除数的寄存器）
    std::vector<std::string> crash_regs;

    // 每个崩溃寄存器的溯源结果
    std::map<std::string, RegisterTrace> register_traces;

    CrashProneRecord() : offset(0), cp_type(CP_NONE), exec_count(0) {}
};

// ============ 指令静态信息（插桩时提取）============
struct InstInfo {
    ADDRINT runtime_addr;                    // 运行时地址
    uint8_t cp_type;                         // 易崩溃类型

    // 崩溃寄存器（归一化后的）
    REG crash_regs[UnifiedConfig::MAX_CRASH_REGS];
    uint8_t num_crash_regs;

    // 所有读寄存器（用于环形缓冲区）
    REG read_regs[8];
    uint8_t num_reads;

    // 所有写寄存器（用于shadow_regs更新）
    REG write_regs[4];
    uint8_t num_writes;

    InstInfo() : runtime_addr(0), cp_type(CP_NONE), num_crash_regs(0),
                 num_reads(0), num_writes(0) {
        memset(crash_regs, 0, sizeof(crash_regs));
        memset(read_regs, 0, sizeof(read_regs));
        memset(write_regs, 0, sizeof(write_regs));
    }
};

#endif // UNIFIED_TRACER_H
