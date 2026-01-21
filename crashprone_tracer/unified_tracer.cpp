/*
 * unified_tracer.cpp - 统一易崩溃指令识别与数据流溯源
 *
 * 功能：在遇到易崩溃指令时，立即对"可能导致崩溃的寄存器"进行溯源
 *       只溯源寻址寄存器(base/index)、跳转目标寄存器、除数寄存器
 *
 * 用法：
 *   pin -t unified_tracer.so -o result.json -depth 5 -- <program> [args]
 */

#include "unified_tracer.h"
#include "../utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <utility>

using namespace std;

// ========== 全局变量 ==========

// TLS key
TLS_KEY g_tls_key;
PIN_LOCK g_lock;

// 易崩溃指令记录表：offset -> CrashProneRecord
std::map<ADDRINT, CrashProneRecord> g_crashprone_records;

// 反汇编缓存：IP -> 反汇编字符串
std::map<ADDRINT, std::string> g_disasm_cache;

// 主程序镜像信息
std::string g_main_img_name = "";
ADDRINT g_main_img_low = 0;
ADDRINT g_main_img_high = 0;

// 统计信息
uint64_t g_total_inst_count = 0;

// ========== KNOB 参数 ==========

KNOB<std::string> output_file(KNOB_MODE_WRITEONCE, "pintool",
    "o", "unified_trace.json", "输出文件路径");

KNOB<uint32_t> max_depth(KNOB_MODE_WRITEONCE, "pintool",
    "depth", "5", "最大溯源深度");

KNOB<uint64_t> min_exec_count(KNOB_MODE_WRITEONCE, "pintool",
    "min_exec", "2", "最小执行次数过滤");

// ========== 辅助函数 ==========

// 判断寄存器是否为有效数据寄存器
bool is_valid_data_reg(REG reg) {
    if (!REG_valid(reg)) return false;
    if (REG_is_flags(reg)) return false;

    // 过滤栈指针和指令指针
    if (reg == REG_RSP || reg == REG_ESP || reg == REG_SP) return false;
    if (reg == REG_RIP || reg == REG_EIP || reg == REG_IP) return false;

    return REG_is_gr(reg) || REG_is_xmm(reg) || REG_is_ymm(reg);
}

// 寄存器归一化
REG normalize_reg(REG reg) {
    return REG_FullRegName(reg);
}

// 检查指令是否在主程序范围内
bool is_in_main_image(ADDRINT ip) {
    if (g_main_img_low == 0) return true;
    return (ip >= g_main_img_low && ip <= g_main_img_high);
}

// 判断指令是否有索引寄存器
bool has_index_register(INS ins) {
    if (INS_MemoryOperandCount(ins) == 0) return false;
    REG index_reg = INS_MemoryIndexReg(ins);
    return REG_valid(index_reg);
}

// JSON 转义
std::string escape_json(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

// 判断指令是否为易崩溃指令
bool is_crash_prone(INS ins, uint8_t& cp_type) {
    std::string mnemonic = INS_Mnemonic(ins);

    // 规则 1: 内存写指令
    if (INS_IsMemoryWrite(ins)) {
        cp_type = has_index_register(ins) ? CP_INDEX_ACCESS : CP_MEM_WRITE;
        return true;
    }

    // 规则 2: 间接跳转/调用
    if (INS_IsIndirectControlFlow(ins)) {
        cp_type = CP_INDIRECT_CF;
        return true;
    }

    // 规则 3: 除法指令
    if (mnemonic == "DIV" || mnemonic == "IDIV" ||
        mnemonic == "DIVSD" || mnemonic == "DIVSS" ||
        mnemonic == "DIVPD" || mnemonic == "DIVPS") {
        cp_type = CP_DIV;
        return true;
    }

    // 规则 4: 带索引的内存读
    if (INS_IsMemoryRead(ins) && has_index_register(ins)) {
        cp_type = CP_INDEX_ACCESS;
        return true;
    }

    // 规则 5: 普通内存读
    if (INS_IsMemoryRead(ins)) {
        cp_type = CP_MEM_READ;
        return true;
    }

    cp_type = CP_NONE;
    return false;
}

// ========== 崩溃寄存器提取 ==========

/*
 * 提取可能导致崩溃的寄存器
 * - 内存操作: base + index 寄存器（用于寻址）
 * - 间接跳转: 目标地址寄存器
 * - 除法: 除数寄存器
 */
void extract_crash_registers(INS ins, uint8_t cp_type, InstInfo* info) {
    info->num_crash_regs = 0;

    switch (cp_type) {
        case CP_MEM_WRITE:
        case CP_MEM_READ:
        case CP_INDEX_ACCESS: {
            // 内存操作：提取 base 和 index 寄存器
            REG base_reg = INS_MemoryBaseReg(ins);
            REG index_reg = INS_MemoryIndexReg(ins);

            if (REG_valid(base_reg) && is_valid_data_reg(base_reg)) {
                REG norm = normalize_reg(base_reg);
                // 排除 RBP（通常用于栈帧）
                if (norm != REG_RSP && norm != REG_RBP) {
                    info->crash_regs[info->num_crash_regs++] = norm;
                }
            }

            if (REG_valid(index_reg) && is_valid_data_reg(index_reg)) {
                REG norm = normalize_reg(index_reg);
                // 检查是否重复
                bool duplicate = false;
                for (int i = 0; i < info->num_crash_regs; i++) {
                    if (info->crash_regs[i] == norm) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate && info->num_crash_regs < UnifiedConfig::MAX_CRASH_REGS) {
                    info->crash_regs[info->num_crash_regs++] = norm;
                }
            }
            break;
        }

        case CP_INDIRECT_CF: {
            // 间接跳转/调用：提取目标地址寄存器
            if (INS_OperandCount(ins) > 0 && INS_OperandIsReg(ins, 0)) {
                REG target_reg = INS_OperandReg(ins, 0);
                if (REG_valid(target_reg) && is_valid_data_reg(target_reg)) {
                    info->crash_regs[info->num_crash_regs++] = normalize_reg(target_reg);
                }
            } else if (INS_IsMemoryRead(ins)) {
                // 间接跳转通过内存（如 jmp [rax]）
                REG base_reg = INS_MemoryBaseReg(ins);
                REG index_reg = INS_MemoryIndexReg(ins);

                if (REG_valid(base_reg) && is_valid_data_reg(base_reg)) {
                    REG norm = normalize_reg(base_reg);
                    if (norm != REG_RSP && norm != REG_RBP) {
                        info->crash_regs[info->num_crash_regs++] = norm;
                    }
                }
                if (REG_valid(index_reg) && is_valid_data_reg(index_reg) &&
                    info->num_crash_regs < UnifiedConfig::MAX_CRASH_REGS) {
                    info->crash_regs[info->num_crash_regs++] = normalize_reg(index_reg);
                }
            }
            break;
        }

        case CP_DIV: {
            // 除法指令：除数寄存器
            std::string mnemonic = INS_Mnemonic(ins);

            if (mnemonic == "DIV" || mnemonic == "IDIV") {
                // 整数除法：操作数0是除数
                if (INS_OperandCount(ins) > 0) {
                    if (INS_OperandIsReg(ins, 0)) {
                        REG divisor = INS_OperandReg(ins, 0);
                        if (REG_valid(divisor)) {
                            info->crash_regs[info->num_crash_regs++] = normalize_reg(divisor);
                        }
                    } else if (INS_OperandIsMemory(ins, 0)) {
                        // 除数在内存中，提取寻址寄存器
                        REG base_reg = INS_MemoryBaseReg(ins);
                        if (REG_valid(base_reg) && is_valid_data_reg(base_reg)) {
                            info->crash_regs[info->num_crash_regs++] = normalize_reg(base_reg);
                        }
                    }
                }
            } else if (mnemonic.find("DIV") != std::string::npos) {
                // 浮点除法：DIVSD, DIVSS 等，第二个操作数是除数
                if (INS_OperandCount(ins) > 1 && INS_OperandIsReg(ins, 1)) {
                    REG divisor = INS_OperandReg(ins, 1);
                    if (REG_valid(divisor)) {
                        info->crash_regs[info->num_crash_regs++] = normalize_reg(divisor);
                    }
                }
            }
            break;
        }
    }
}

// ========== BFS 溯源 ==========

/*
 * 对单个崩溃寄存器进行 BFS 溯源
 */
void trace_single_register(ThreadState* tstate, uint64_t target_id, REG target_reg,
                           int depth_limit, RegisterTrace& result) {
    std::set<uint64_t> visited;
    std::vector<uint64_t> current_layer;
    std::vector<uint64_t> next_layer;
    std::set<ADDRINT> found_offsets;

    result.reg_name = REG_StringShort(target_reg);
    result.sources.clear();

    // 初始化：查找最后定义该寄存器的指令
    auto it = tstate->shadow_regs.find(target_reg);
    if (it != tstate->shadow_regs.end()) {
        uint64_t def_id = it->second;
        if ((target_id > def_id) && (target_id - def_id) < UnifiedConfig::WINDOW_SIZE) {
            current_layer.push_back(def_id);
            visited.insert(def_id);
        }
    }

    // BFS 回溯
    for (int depth = 1; depth <= depth_limit && !current_layer.empty(); depth++) {
        next_layer.clear();

        for (uint64_t def_id : current_layer) {
            uint64_t pos = def_id % UnifiedConfig::WINDOW_SIZE;
            DynNode& node = tstate->ring_buffer[pos];

            // 验证节点有效性
            if (node.dyn_id != def_id) continue;

            // 计算偏移
            ADDRINT offset = node.offset;

            // 添加到结果（避免重复）
            if (found_offsets.find(offset) == found_offsets.end()) {
                found_offsets.insert(offset);

                SourceEntry entry;
                entry.offset = offset;
                entry.depth = depth;
                entry.hit_count = 1;

                // 从缓存获取反汇编
                auto disasm_it = g_disasm_cache.find(node.ip);
                if (disasm_it != g_disasm_cache.end()) {
                    entry.disasm = disasm_it->second;
                }

                result.sources.push_back(entry);
            } else {
                // 已存在，增加命中计数
                for (auto& existing : result.sources) {
                    if (existing.offset == offset) {
                        existing.hit_count++;
                        break;
                    }
                }
            }

            // 继续回溯：该指令的读寄存器
            for (int i = 0; i < node.num_reads && depth < depth_limit; i++) {
                REG read_reg = node.read_regs[i];
                if (!is_valid_data_reg(read_reg)) continue;

                auto parent_it = tstate->shadow_regs.find(read_reg);
                if (parent_it != tstate->shadow_regs.end()) {
                    uint64_t parent_id = parent_it->second;
                    if (visited.find(parent_id) == visited.end() &&
                        parent_id < target_id &&
                        (target_id - parent_id) < UnifiedConfig::WINDOW_SIZE) {
                        next_layer.push_back(parent_id);
                        visited.insert(parent_id);
                    }
                }
            }
        }

        current_layer = std::move(next_layer);
    }
}

/*
 * 对易崩溃指令的所有崩溃寄存器进行溯源
 */
void trace_crashprone_inst(ThreadState* tstate, ADDRINT ip, InstInfo* info,
                           uint64_t current_dyn_id) {
    // 计算 offset
    ADDRINT offset = 0;
    if (g_main_img_low != 0 && ip >= g_main_img_low && ip <= g_main_img_high) {
        offset = ip - g_main_img_low;
    } else {
        return;  // 不在主镜像内
    }

    PIN_GetLock(&g_lock, 0);

    // 获取或创建记录
    auto it = g_crashprone_records.find(offset);
    if (it == g_crashprone_records.end()) {
        CrashProneRecord record;
        record.offset = offset;
        record.cp_type = info->cp_type;
        record.exec_count = 0;

        auto disasm_it = g_disasm_cache.find(ip);
        if (disasm_it != g_disasm_cache.end()) {
            record.disasm = disasm_it->second;
        }

        // 记录崩溃寄存器名称
        for (int i = 0; i < info->num_crash_regs; i++) {
            record.crash_regs.push_back(REG_StringShort(info->crash_regs[i]));
        }

        g_crashprone_records[offset] = record;
        it = g_crashprone_records.find(offset);
    }

    it->second.exec_count++;

    PIN_ReleaseLock(&g_lock);

    // 对每个崩溃寄存器独立溯源
    for (int i = 0; i < info->num_crash_regs; i++) {
        REG crash_reg = info->crash_regs[i];
        std::string reg_name = REG_StringShort(crash_reg);

        RegisterTrace trace;
        trace_single_register(tstate, current_dyn_id, crash_reg, max_depth.Value(), trace);

        // 合并结果
        PIN_GetLock(&g_lock, 0);

        auto& existing_trace = it->second.register_traces[reg_name];
        if (existing_trace.sources.empty()) {
            existing_trace = trace;
        } else {
            // 合并：更新命中计数
            for (const auto& new_source : trace.sources) {
                bool found = false;
                for (auto& existing_source : existing_trace.sources) {
                    if (existing_source.offset == new_source.offset) {
                        existing_source.hit_count += new_source.hit_count;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    existing_trace.sources.push_back(new_source);
                }
            }
        }

        PIN_ReleaseLock(&g_lock);
    }
}

// ========== 分析回调 ==========

/*
 * 运行时分析回调
 */
VOID Analyze_Exec(THREADID tid, ADDRINT ip, InstInfo* info) {
    ThreadState* tstate = static_cast<ThreadState*>(PIN_GetThreadData(g_tls_key, tid));
    if (!tstate) return;

    uint64_t current_id = tstate->dyn_id++;
    g_total_inst_count++;

    // 1. 写入环形缓冲区
    uint64_t pos = current_id % UnifiedConfig::WINDOW_SIZE;
    DynNode& node = tstate->ring_buffer[pos];

    node.dyn_id = current_id;
    node.ip = ip;
    node.num_reads = info->num_reads;
    node.num_writes = info->num_writes;

    if (g_main_img_low != 0 && ip >= g_main_img_low && ip <= g_main_img_high) {
        node.offset = ip - g_main_img_low;
    } else {
        node.offset = ip;
    }

    memcpy(node.read_regs, info->read_regs, info->num_reads * sizeof(REG));
    memcpy(node.write_regs, info->write_regs, info->num_writes * sizeof(REG));

    // 2. 更新 shadow_regs
    for (int i = 0; i < node.num_writes; i++) {
        REG reg = node.write_regs[i];
        if (is_valid_data_reg(reg)) {
            tstate->shadow_regs[reg] = current_id;
        }
    }

    // 3. 如果是易崩溃指令且有崩溃寄存器，执行溯源
    if (info->cp_type != CP_NONE && info->num_crash_regs > 0) {
        trace_crashprone_inst(tstate, ip, info, current_id);
    }
}

// ========== 插桩函数 ==========

VOID Instruction(INS ins, VOID *v) {
    ADDRINT ip = INS_Address(ins);

    // 过滤
    if (!is_in_main_image(ip)) return;
    if (!RTN_Valid(INS_Rtn(ins))) return;
    if (SEC_Name(RTN_Sec(INS_Rtn(ins))) != ".text") return;

    std::string rtnname = RTN_Name(INS_Rtn(ins));
    if (rtnname.find("__libc") == 0 || rtnname.find("_start") == 0 ||
        rtnname.find("call_gmon_start") == 0 || rtnname.find("frame_dummy") == 0 ||
        rtnname.find("__do_global") == 0 || rtnname.find("__stat") == 0) {
        return;
    }

    // 缓存反汇编
    if (g_disasm_cache.find(ip) == g_disasm_cache.end()) {
        g_disasm_cache[ip] = INS_Disassemble(ins);
    }

    // 创建指令信息
    InstInfo* info = new InstInfo();
    info->runtime_addr = ip;

    // 判断是否为易崩溃指令
    uint8_t cp_type = CP_NONE;
    if (is_crash_prone(ins, cp_type)) {
        info->cp_type = cp_type;
        // 提取崩溃寄存器
        extract_crash_registers(ins, cp_type, info);
    }

    // 提取所有读/写寄存器（用于数据流追踪）
    UINT32 max_reads = INS_MaxNumRRegs(ins);
    for (UINT32 i = 0; i < max_reads && info->num_reads < 8; i++) {
        REG reg = INS_RegR(ins, i);
        REG norm = normalize_reg(reg);
        if (is_valid_data_reg(norm)) {
            info->read_regs[info->num_reads++] = norm;
        }
    }

    UINT32 max_writes = INS_MaxNumWRegs(ins);
    for (UINT32 i = 0; i < max_writes && info->num_writes < 4; i++) {
        REG reg = INS_RegW(ins, i);
        REG norm = normalize_reg(reg);
        if (is_valid_data_reg(norm)) {
            info->write_regs[info->num_writes++] = norm;
        }
    }

    // 插入分析调用
    INS_InsertCall(
        ins, IPOINT_BEFORE, (AFUNPTR)Analyze_Exec,
        IARG_THREAD_ID,
        IARG_INST_PTR,
        IARG_PTR, info,
        IARG_END
    );
}

// ========== 镜像加载回调 ==========

VOID ImageLoad(IMG img, VOID *v) {
    if (IMG_IsMainExecutable(img)) {
        g_main_img_name = IMG_Name(img);
        g_main_img_low = IMG_LowAddress(img);
        g_main_img_high = IMG_HighAddress(img);

        fprintf(stderr, "[UnifiedTracer] 主镜像: %s (0x%lx - 0x%lx)\n",
                g_main_img_name.c_str(), g_main_img_low, g_main_img_high);
    }
}

// ========== 线程回调 ==========

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v) {
    ThreadState* tstate = new ThreadState();
    PIN_SetThreadData(g_tls_key, tstate, tid);
}

VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v) {
    ThreadState* tstate = static_cast<ThreadState*>(PIN_GetThreadData(g_tls_key, tid));
    if (tstate) {
        delete tstate;
    }
}

// ========== 输出结果 ==========

VOID Fini(INT32 code, VOID *v) {
    fprintf(stderr, "[UnifiedTracer] 程序执行完毕，共执行 %lu 条指令\n", g_total_inst_count);
    fprintf(stderr, "[UnifiedTracer] 发现 %lu 个易崩溃指令\n", g_crashprone_records.size());

    FILE* fp = fopen(output_file.Value().c_str(), "w");
    if (!fp) {
        fprintf(stderr, "[UnifiedTracer] 无法创建输出文件: %s\n", output_file.Value().c_str());
        return;
    }

    fprintf(fp, "{\n");

    // config
    fprintf(fp, "  \"config\": {\n");
    fprintf(fp, "    \"img_name\": \"%s\",\n", escape_json(g_main_img_name).c_str());
    fprintf(fp, "    \"img_base_addr\": \"0x%lx\",\n", g_main_img_low);
    fprintf(fp, "    \"max_depth\": %u,\n", max_depth.Value());
    fprintf(fp, "    \"min_exec_count\": %lu,\n", min_exec_count.Value());
    fprintf(fp, "    \"window_size\": %lu\n", UnifiedConfig::WINDOW_SIZE);
    fprintf(fp, "  },\n");

    // crashprone_insts
    fprintf(fp, "  \"crashprone_insts\": [\n");

    bool first_inst = true;
    for (const auto& kv : g_crashprone_records) {
        const CrashProneRecord& record = kv.second;

        // 过滤低执行次数
        if (record.exec_count < min_exec_count.Value()) continue;

        if (!first_inst) fprintf(fp, ",\n");
        first_inst = false;

        fprintf(fp, "    {\n");
        fprintf(fp, "      \"offset\": \"0x%lx\",\n", record.offset);
        fprintf(fp, "      \"disasm\": \"%s\",\n", escape_json(record.disasm).c_str());
        fprintf(fp, "      \"type\": \"%s\",\n", cp_type_to_string(record.cp_type));
        fprintf(fp, "      \"exec_count\": %lu,\n", record.exec_count);

        // crash_regs
        fprintf(fp, "      \"crash_regs\": [");
        for (size_t i = 0; i < record.crash_regs.size(); i++) {
            if (i > 0) fprintf(fp, ", ");
            fprintf(fp, "\"%s\"", record.crash_regs[i].c_str());
        }
        fprintf(fp, "],\n");

        // register_traces
        fprintf(fp, "      \"register_traces\": {\n");

        bool first_reg = true;
        for (const auto& trace_kv : record.register_traces) {
            const std::string& reg_name = trace_kv.first;
            const RegisterTrace& trace = trace_kv.second;

            if (!first_reg) fprintf(fp, ",\n");
            first_reg = false;

            fprintf(fp, "        \"%s\": [\n", reg_name.c_str());

            bool first_source = true;
            for (const auto& source : trace.sources) {
                if (!first_source) fprintf(fp, ",\n");
                first_source = false;

                fprintf(fp, "          {\"offset\": \"0x%lx\", \"disasm\": \"%s\", \"depth\": %d, \"hit_count\": %lu}",
                        source.offset, escape_json(source.disasm).c_str(),
                        source.depth, source.hit_count);
            }

            fprintf(fp, "\n        ]");
        }

        fprintf(fp, "\n      }\n");
        fprintf(fp, "    }");
    }

    fprintf(fp, "\n  ],\n");

    // statistics
    uint64_t total_with_traces = 0;
    uint64_t total_sources = 0;
    for (const auto& kv : g_crashprone_records) {
        if (kv.second.exec_count >= min_exec_count.Value()) {
            if (!kv.second.register_traces.empty()) {
                total_with_traces++;
                for (const auto& trace_kv : kv.second.register_traces) {
                    total_sources += trace_kv.second.sources.size();
                }
            }
        }
    }

    fprintf(fp, "  \"statistics\": {\n");
    fprintf(fp, "    \"total_crashprone_insts\": %lu,\n", g_crashprone_records.size());
    fprintf(fp, "    \"insts_with_traces\": %lu,\n", total_with_traces);
    fprintf(fp, "    \"total_source_entries\": %lu,\n", total_sources);
    fprintf(fp, "    \"total_instructions_executed\": %lu\n", g_total_inst_count);
    fprintf(fp, "  }\n");

    fprintf(fp, "}\n");
    fclose(fp);

    fprintf(stderr, "[UnifiedTracer] 结果已保存到: %s\n", output_file.Value().c_str());
}

// ========== 主函数 ==========

INT32 Usage() {
    cerr << "unified_tracer - 统一易崩溃指令识别与数据流溯源工具" << endl;
    cerr << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char *argv[]) {
    // 初始化 Pin
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    // 初始化符号表
    PIN_InitSymbols();

    // 初始化锁
    PIN_InitLock(&g_lock);

    // 创建 TLS key
    g_tls_key = PIN_CreateThreadDataKey(NULL);

    fprintf(stderr, "[UnifiedTracer] 启动统一溯源工具\n");
    fprintf(stderr, "[UnifiedTracer] 最大溯源深度: %u\n", max_depth.Value());
    fprintf(stderr, "[UnifiedTracer] 输出文件: %s\n", output_file.Value().c_str());

    // 注册回调
    IMG_AddInstrumentFunction(ImageLoad, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(Fini, 0);

    // 开始执行
    PIN_StartProgram();

    return 0;
}
