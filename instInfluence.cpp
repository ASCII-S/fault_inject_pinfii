/**
 * instInfluence.cpp - 指令影响力分析 Pin 工具
 *
 * 功能：静态分析程序中每条指令的数据流和控制流影响力
 * 核心指标：
 *   - DefUseDistance: 定义到首次使用的距离
 *   - UseCount: 被使用次数
 *   - IsDeadDef: 是否为死定义
 *   - LiveRange: 活跃范围
 *   - IsInLoop: 是否在循环内
 *   - InfluenceScore: 综合影响力分数
 *
 * 输出格式: CSV
 *   pc,mnemonic,regmm,reg,def_use_dist,use_count,is_dead,live_range,is_in_loop,influence_score
 */

#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>

#include "pin.H"
#include "utils.h"

using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::set;
using std::vector;

// ============ KNOB 参数定义 ============
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "influence_catalog.csv", "Output file for influence analysis");

KNOB<ADDRINT> KnobPCStart(KNOB_MODE_WRITEONCE, "pintool",
    "pc_start", "0", "Start PC address for analysis (hex)");

KNOB<ADDRINT> KnobPCEnd(KNOB_MODE_WRITEONCE, "pintool",
    "pc_end", "0xffffffffffffffff", "End PC address for analysis (hex)");

KNOB<int> KnobAnalysisWindow(KNOB_MODE_WRITEONCE, "pintool",
    "window", "500", "Max instructions to track for def-use analysis");

KNOB<int> KnobOnlyMemory(KNOB_MODE_WRITEONCE, "pintool",
    "only_memory", "0", "Only analyze memory operations (1=yes, 0=no)");

// ============ 数据结构 ============

// 寄存器定义信息
struct DefInfo {
    ADDRINT def_pc;           // 定义点 PC
    UINT64 def_seq;           // 定义时的指令序号
    int use_count;            // 使用次数
    ADDRINT first_use_pc;     // 首次使用的 PC
    ADDRINT last_use_pc;      // 最后使用的 PC
    bool is_dead;             // 是否死定义

    DefInfo() : def_pc(0), def_seq(0), use_count(0),
                first_use_pc(0), last_use_pc(0), is_dead(true) {}
};

// 指令影响力信息
struct InstInfluence {
    ADDRINT pc;
    string mnemonic;
    string regmm;             // 内存操作寄存器
    string reg;               // 普通寄存器

    // 数据流指标
    int def_use_distance;     // 定义到首次使用距离（指令数）
    int use_count;            // 被使用次数
    bool is_dead_def;         // 是否死定义
    int live_range;           // 活跃范围（指令数）

    // 控制流指标
    bool is_in_loop;          // 是否在循环内
    bool is_control_flow;     // 是否控制流指令
    bool is_memory_write;     // 是否内存写操作

    // 综合分数
    float influence_score;

    InstInfluence() : pc(0), def_use_distance(0), use_count(0),
                      is_dead_def(false), live_range(0), is_in_loop(false),
                      is_control_flow(false), is_memory_write(false),
                      influence_score(0.0f) {}
};

// ============ 全局变量 ============
map<REG, DefInfo> reg_def_table;              // 寄存器 -> 当前活跃定义
map<ADDRINT, InstInfluence> inst_table;       // PC -> 指令影响力信息
set<ADDRINT> loop_body_addrs;                 // 循环体内的指令地址
vector<ADDRINT> back_edge_targets;            // 回边目标地址

UINT64 inst_seq = 0;                          // 全局指令序号
std::ofstream outFile;

// ============ 辅助函数 ============

// 判断寄存器是否需要追踪（排除标志寄存器等）
bool ShouldTrackReg(REG reg) {
    if (!REG_valid(reg)) return false;
    if (reg == REG_RFLAGS || reg == REG_FLAGS || reg == REG_EFLAGS) return false;
    if (REG_is_seg(reg)) return false;  // 段寄存器
    return true;
}

// 检测是否为控制流指令
bool IsControlFlowMnemonic(const string& mnemonic) {
    static set<string> cf_mnemonics = {
        "JMP", "JZ", "JNZ", "JE", "JNE", "JL", "JLE", "JG", "JGE",
        "JA", "JAE", "JB", "JBE", "JO", "JNO", "JS", "JNS", "JP", "JNP",
        "JCXZ", "JECXZ", "JRCXZ", "LOOP", "LOOPE", "LOOPNE",
        "CALL", "RET", "CALL_NEAR", "RET_NEAR"
    };
    return cf_mnemonics.find(mnemonic) != cf_mnemonics.end();
}

// 计算综合影响力分数
float CalculateInfluenceScore(const InstInfluence& info) {
    float score = 0.0f;

    // 死指令得分为 0
    if (info.is_dead_def) {
        return 0.0f;
    }

    // 1. 使用次数权重 (max 10分)
    score += std::min(info.use_count * 2.0f, 10.0f);

    // 2. 活跃范围权重 (max 5分)
    score += std::min(info.live_range / 10.0f, 5.0f);

    // 3. 定义-使用距离越近，影响越直接 (max 3分)
    if (info.def_use_distance > 0 && info.def_use_distance < 10) {
        score += 3.0f;
    } else if (info.def_use_distance < 50) {
        score += 1.5f;
    }

    // 4. 控制流指令加成 (bonus 5分)
    if (info.is_control_flow) {
        score += 5.0f;
    }

    // 5. 内存写操作加成 (bonus 3分)
    if (info.is_memory_write) {
        score += 3.0f;
    }

    // 6. 循环内指令加成 (乘以 2)
    if (info.is_in_loop) {
        score *= 2.0f;
    }

    return std::min(score, 30.0f);  // 最高 30 分
}

// 标记循环体内的指令
void MarkLoopBody(ADDRINT loop_start, ADDRINT loop_end) {
    // 简化处理：将 [loop_start, loop_end] 范围内的所有已知指令标记为循环内
    for (auto& pair : inst_table) {
        if (pair.first >= loop_start && pair.first <= loop_end) {
            pair.second.is_in_loop = true;
        }
    }
}

// ============ 分析回调函数 ============

// 静态指令分析（在插桩时调用一次）
VOID AnalyzeInstruction(INS ins, VOID* v) {
    ADDRINT pc = INS_Address(ins);

    // PC 范围过滤
    if (pc < KnobPCStart.Value() || pc > KnobPCEnd.Value()) {
        return;
    }

    // 只分析内存操作（可选）
    if (KnobOnlyMemory.Value() == 1) {
        if (!INS_IsMemoryRead(ins) && !INS_IsMemoryWrite(ins)) {
            return;
        }
    }

    // 创建指令信息
    InstInfluence info;
    info.pc = pc;
    info.mnemonic = INS_Mnemonic(ins);
    info.is_control_flow = IsControlFlowMnemonic(info.mnemonic);
    info.is_memory_write = INS_IsMemoryWrite(ins);

    // 获取寄存器信息
    // 优先获取内存操作的基址寄存器
    if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        REG base_reg = INS_MemoryBaseReg(ins);
        if (REG_valid(base_reg)) {
            info.regmm = REG_StringShort(base_reg);
        } else {
            REG index_reg = INS_MemoryIndexReg(ins);
            if (REG_valid(index_reg)) {
                info.regmm = REG_StringShort(index_reg);
            }
        }
    }

    // 获取第一个写寄存器
    if (INS_MaxNumWRegs(ins) > 0) {
        REG wreg = INS_RegW(ins, 0);
        if (ShouldTrackReg(wreg)) {
            info.reg = REG_StringShort(wreg);
        }
    }

    // 检测回边（循环检测）
    if (INS_IsControlFlow(ins)) {
        if (INS_IsDirectControlFlow(ins)) {
            ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
            if (target < pc) {
                // 回边：跳转到更小地址，标记为循环
                back_edge_targets.push_back(target);
                // 标记循环体
                MarkLoopBody(target, pc);
                info.is_in_loop = true;
            }
        }
    }

    // 检查是否在已知循环体内
    for (const auto& target : back_edge_targets) {
        if (pc >= target) {
            // 可能在某个循环体内
            info.is_in_loop = true;
            break;
        }
    }

    inst_table[pc] = info;
}

// 动态分析：追踪寄存器读操作（use）
VOID RecordRegRead(ADDRINT pc, UINT32 reg_id) {
    REG reg = static_cast<REG>(reg_id);
    if (!ShouldTrackReg(reg)) return;

    // 查找该寄存器的定义点
    auto it = reg_def_table.find(reg);
    if (it != reg_def_table.end()) {
        DefInfo& def_info = it->second;

        // 更新使用信息
        def_info.use_count++;
        def_info.is_dead = false;

        if (def_info.use_count == 1) {
            def_info.first_use_pc = pc;
        }
        def_info.last_use_pc = pc;

        // 更新对应定义指令的信息
        auto inst_it = inst_table.find(def_info.def_pc);
        if (inst_it != inst_table.end()) {
            InstInfluence& info = inst_it->second;
            info.use_count = def_info.use_count;
            info.is_dead_def = false;

            // 计算 def-use 距离（使用指令序号差）
            if (info.def_use_distance == 0) {
                info.def_use_distance = inst_seq - def_info.def_seq;
            }

            // 更新活跃范围
            info.live_range = inst_seq - def_info.def_seq;
        }
    }
}

// 动态分析：追踪寄存器写操作（def）
VOID RecordRegWrite(ADDRINT pc, UINT32 reg_id) {
    REG reg = static_cast<REG>(reg_id);
    if (!ShouldTrackReg(reg)) return;

    // 检查之前的定义是否变成死定义
    auto it = reg_def_table.find(reg);
    if (it != reg_def_table.end()) {
        DefInfo& old_def = it->second;
        if (old_def.use_count == 0) {
            // 之前的定义从未被使用，标记为死定义
            auto inst_it = inst_table.find(old_def.def_pc);
            if (inst_it != inst_table.end()) {
                inst_it->second.is_dead_def = true;
            }
        }
    }

    // 记录新的定义
    DefInfo new_def;
    new_def.def_pc = pc;
    new_def.def_seq = inst_seq;
    new_def.use_count = 0;
    new_def.is_dead = true;
    reg_def_table[reg] = new_def;

    inst_seq++;
}

// 动态指令计数
VOID CountInst(ADDRINT pc) {
    inst_seq++;
}

// 指令插桩
VOID Instruction(INS ins, VOID* v) {
    ADDRINT pc = INS_Address(ins);

    // PC 范围过滤
    if (pc < KnobPCStart.Value() || pc > KnobPCEnd.Value()) {
        return;
    }

    // 静态分析
    AnalyzeInstruction(ins, v);

    // 动态分析插桩

    // 插桩读寄存器（use）
    for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
        REG reg = INS_RegR(ins, i);
        if (ShouldTrackReg(reg)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordRegRead,
                           IARG_INST_PTR,
                           IARG_UINT32, static_cast<UINT32>(reg),
                           IARG_END);
        }
    }

    // 插桩写寄存器（def）
    for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
        REG reg = INS_RegW(ins, i);
        if (ShouldTrackReg(reg)) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordRegWrite,
                           IARG_INST_PTR,
                           IARG_UINT32, static_cast<UINT32>(reg),
                           IARG_END);
        }
    }
}

// 程序结束时输出结果
VOID Fini(INT32 code, VOID* v) {
    // 计算所有指令的影响力分数
    for (auto& pair : inst_table) {
        pair.second.influence_score = CalculateInfluenceScore(pair.second);
    }

    // 输出 CSV
    outFile.open(KnobOutputFile.Value().c_str());
    if (!outFile.is_open()) {
        cerr << "Error: Cannot open output file " << KnobOutputFile.Value() << endl;
        return;
    }

    // CSV 头
    outFile << "pc,mnemonic,regmm,reg,def_use_dist,use_count,is_dead,live_range,is_in_loop,is_control_flow,is_mem_write,influence_score" << endl;

    // 按 PC 排序输出
    vector<ADDRINT> sorted_pcs;
    for (const auto& pair : inst_table) {
        sorted_pcs.push_back(pair.first);
    }
    std::sort(sorted_pcs.begin(), sorted_pcs.end());

    for (ADDRINT pc : sorted_pcs) {
        const InstInfluence& info = inst_table[pc];
        outFile << pc << ","
                << info.mnemonic << ","
                << info.regmm << ","
                << info.reg << ","
                << info.def_use_distance << ","
                << info.use_count << ","
                << (info.is_dead_def ? 1 : 0) << ","
                << info.live_range << ","
                << (info.is_in_loop ? 1 : 0) << ","
                << (info.is_control_flow ? 1 : 0) << ","
                << (info.is_memory_write ? 1 : 0) << ","
                << info.influence_score << endl;
    }

    outFile.close();

    // 统计信息
    int total = inst_table.size();
    int dead_count = 0;
    int loop_count = 0;
    float avg_score = 0;

    for (const auto& pair : inst_table) {
        if (pair.second.is_dead_def) dead_count++;
        if (pair.second.is_in_loop) loop_count++;
        avg_score += pair.second.influence_score;
    }

    if (total > 0) {
        avg_score /= total;
    }

    cerr << "=== Instruction Influence Analysis Complete ===" << endl;
    cerr << "Total instructions analyzed: " << total << endl;
    cerr << "Dead definitions: " << dead_count << " (" << (100.0 * dead_count / total) << "%)" << endl;
    cerr << "Instructions in loops: " << loop_count << " (" << (100.0 * loop_count / total) << "%)" << endl;
    cerr << "Average influence score: " << avg_score << endl;
    cerr << "Output saved to: " << KnobOutputFile.Value() << endl;
}

// ============ 主函数 ============

INT32 Usage() {
    cerr << "This tool analyzes instruction influence using data flow and control flow analysis." << endl;
    cerr << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char* argv[]) {
    // 初始化 Pin
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    cerr << "=== Instruction Influence Analyzer ===" << endl;
    cerr << "Output file: " << KnobOutputFile.Value() << endl;
    cerr << "PC range: 0x" << std::hex << KnobPCStart.Value()
         << " - 0x" << KnobPCEnd.Value() << std::dec << endl;
    cerr << "Analysis window: " << KnobAnalysisWindow.Value() << " instructions" << endl;

    // 注册回调
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // 启动程序
    PIN_StartProgram();

    return 0;
}
