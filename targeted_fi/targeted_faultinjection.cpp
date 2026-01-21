/*
 * targeted_faultinjection.cpp - 定向故障注入工具实现
 *
 * 用法：
 *   pin -t targeted_faultinjection.so \
 *       -target_pc 0x4019c8 \
 *       -target_reg rax \
 *       -target_kth 459 \
 *       -inject_bit -1 \
 *       -o inject_info.txt \
 *       -- ./program args
 */

#include "targeted_faultinjection.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <ctime>
#include <cstring>

using namespace std;

// ========== 全局变量 ==========

UINT64 g_exec_count = 0;                // 目标指令执行计数
bool g_injected = false;                 // 是否已注错
REG g_target_reg_enum = REG_INVALID();   // 目标寄存器枚举
std::string g_target_inst_disasm = "";   // 目标指令反汇编
ADDRINT g_next_pc = 0;                   // 下一条指令地址

// 注错信息
struct InjectionInfo {
    ADDRINT inject_pc;
    std::string inject_inst;
    std::string inject_reg;
    UINT64 inject_kth;
    ADDRINT original_value;
    ADDRINT injected_value;
    INT32 inject_bit;
    ADDRINT next_pc;

    // 内存操作信息
    std::string regw_list;          // 写寄存器列表
    std::string stackw;             // 是否栈写
    std::string base;               // 基址寄存器
    std::string index;              // 索引寄存器
    INT64 displacement;             // 偏移
    UINT32 scale;                   // 缩放因子
} g_inject_info;

// ========== 寄存器名称解析 ==========

REG parse_target_register(const std::string& name) {
    // 转小写
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // 寄存器映射表
    static std::map<std::string, REG> reg_map;
    if (reg_map.empty()) {
        // 64位通用寄存器
        reg_map["rax"] = REG_RAX; reg_map["rbx"] = REG_RBX;
        reg_map["rcx"] = REG_RCX; reg_map["rdx"] = REG_RDX;
        reg_map["rsi"] = REG_RSI; reg_map["rdi"] = REG_RDI;
        reg_map["rbp"] = REG_RBP; reg_map["rsp"] = REG_RSP;
        reg_map["r8"] = REG_R8;   reg_map["r9"] = REG_R9;
        reg_map["r10"] = REG_R10; reg_map["r11"] = REG_R11;
        reg_map["r12"] = REG_R12; reg_map["r13"] = REG_R13;
        reg_map["r14"] = REG_R14; reg_map["r15"] = REG_R15;

        // 32位寄存器（自动归一化到64位）
        reg_map["eax"] = REG_RAX; reg_map["ebx"] = REG_RBX;
        reg_map["ecx"] = REG_RCX; reg_map["edx"] = REG_RDX;
        reg_map["esi"] = REG_RSI; reg_map["edi"] = REG_RDI;
        reg_map["ebp"] = REG_RBP; reg_map["esp"] = REG_RSP;

        // 16位寄存器
        reg_map["ax"] = REG_RAX; reg_map["bx"] = REG_RBX;
        reg_map["cx"] = REG_RCX; reg_map["dx"] = REG_RDX;

        // XMM 寄存器
        for (int i = 0; i < 16; i++) {
            char buf[16];
            sprintf(buf, "xmm%d", i);
            reg_map[buf] = static_cast<REG>(REG_XMM0 + i);
        }

        // YMM 寄存器
        for (int i = 0; i < 16; i++) {
            char buf[16];
            sprintf(buf, "ymm%d", i);
            reg_map[buf] = static_cast<REG>(REG_YMM0 + i);
        }
    }

    auto it = reg_map.find(lower);
    if (it != reg_map.end()) {
        return it->second;
    }

    fprintf(stderr, "[错误] 无法识别寄存器名称: %s\n", name.c_str());
    exit(1);
}

// ========== 故障注入函数 ==========

void inject_fault_gpr(CONTEXT* ctxt, REG reg, INT32 bit) {
    ADDRINT original = PIN_GetContextReg(ctxt, reg);
    ADDRINT injected = original ^ (1UL << bit);
    PIN_SetContextReg(ctxt, reg, injected);

    g_inject_info.original_value = original;
    g_inject_info.injected_value = injected;
    g_inject_info.inject_bit = bit;
}

void inject_fault_xmm(CONTEXT* ctxt, REG reg, INT32 bit) {
    CHAR fpContextSpace[FPSTATE_SIZE];
    FPSTATE* fpContext = reinterpret_cast<FPSTATE*>(fpContextSpace);
    PIN_GetContextFPState(ctxt, fpContext);

    int idx = reg - REG_XMM0;  // XMM 编号 (0-15)
    int vec_idx = bit / 64;     // 选择低64位或高64位
    int bit_in_vec = bit % 64;  // 在该64位内的比特位置

    UINT64 original = fpContext->fxsave_legacy._xmms[idx]._vec64[vec_idx];
    fpContext->fxsave_legacy._xmms[idx]._vec64[vec_idx] ^= (1UL << bit_in_vec);
    UINT64 injected = fpContext->fxsave_legacy._xmms[idx]._vec64[vec_idx];

    PIN_SetContextFPState(ctxt, fpContext);

    g_inject_info.original_value = original;
    g_inject_info.injected_value = injected;
    g_inject_info.inject_bit = bit;
}

void inject_fault_ymm(CONTEXT* ctxt, REG reg, INT32 bit) {
    // YMM 寄存器有 256 位
    // 低128位在 XMM 中，高128位在 YMM 扩展区域中
    CHAR fpContextSpace[FPSTATE_SIZE];
    FPSTATE* fpContext = reinterpret_cast<FPSTATE*>(fpContextSpace);
    PIN_GetContextFPState(ctxt, fpContext);

    int idx = reg - REG_YMM0;  // YMM 编号 (0-15)

    UINT64 original = 0;
    if (bit < 128) {
        // 低128位（XMM部分）
        int vec_idx = bit / 64;
        int bit_in_vec = bit % 64;
        original = fpContext->fxsave_legacy._xmms[idx]._vec64[vec_idx];
        fpContext->fxsave_legacy._xmms[idx]._vec64[vec_idx] ^= (1UL << bit_in_vec);
        g_inject_info.injected_value = fpContext->fxsave_legacy._xmms[idx]._vec64[vec_idx];
    } else {
        // 高128位（YMM扩展部分）
        int upper_bit = bit - 128;
        int byte_idx = (idx * 16) + (upper_bit / 8);  // YMM upper 是字节数组
        int bit_in_byte = upper_bit % 8;

        original = fpContext->_xstate._ymmUpper[byte_idx];
        fpContext->_xstate._ymmUpper[byte_idx] ^= (1U << bit_in_byte);
        g_inject_info.injected_value = fpContext->_xstate._ymmUpper[byte_idx];
    }

    PIN_SetContextFPState(ctxt, fpContext);

    g_inject_info.original_value = original;
    g_inject_info.inject_bit = bit;
}

// ========== 信息输出 ==========

void write_inject_info() {
    FILE* fp = fopen(output_file.Value().c_str(), "w");
    if (!fp) {
        fprintf(stderr, "[错误] 无法创建输出文件: %s\n", output_file.Value().c_str());
        return;
    }

    fprintf(fp, "inject_pc: 0x%lx\n", g_inject_info.inject_pc);
    fprintf(fp, "inject_inst: %s\n", g_inject_info.inject_inst.c_str());
    fprintf(fp, "inject_reg: %s\n", g_inject_info.inject_reg.c_str());
    fprintf(fp, "inject_kth: %lu\n", g_inject_info.inject_kth);
    fprintf(fp, "original_value: 0x%lx\n", g_inject_info.original_value);
    fprintf(fp, "injected_value: 0x%lx\n", g_inject_info.injected_value);
    fprintf(fp, "inject_bit: %d\n", g_inject_info.inject_bit);
    fprintf(fp, "next_pc: 0x%lx\n", g_inject_info.next_pc);
    fprintf(fp, "regw_list: %s\n", g_inject_info.regw_list.c_str());
    fprintf(fp, "stackw: %s\n", g_inject_info.stackw.c_str());
    fprintf(fp, "base: %s\n", g_inject_info.base.c_str());
    fprintf(fp, "index: %s\n", g_inject_info.index.c_str());
    fprintf(fp, "displacement: %ld\n", g_inject_info.displacement);
    fprintf(fp, "scale: %u\n", g_inject_info.scale);

    fclose(fp);
    fprintf(stderr, "[targeted_fi] 注错信息已写入: %s\n", output_file.Value().c_str());
}

// ========== 分析回调 ==========

VOID Analyze_TargetInst(THREADID tid, ADDRINT ip, CONTEXT* ctxt, VOID* v) {
    g_exec_count++;

    // 检查是否达到注错时机
    if (g_exec_count == target_kth.Value() && !g_injected) {
        fprintf(stderr, "[targeted_fi] 第 %lu 次执行目标指令 0x%lx，开始注错\n",
                g_exec_count, ip);

        // 读取寄存器原值
        UINT32 bit_width = get_reg_bit_width(g_target_reg_enum);
        INT32 bit = inject_bit.Value();

        // 确定注入比特位置
        if (bit == -1) {
            // 随机选择比特位
            srand(time(NULL));
            if (high_bit_only.Value()) {
                bit = (bit_width / 2) + (rand() % (bit_width / 2));
            } else {
                bit = rand() % bit_width;
            }
        }

        // 检查比特位置是否合法
        if (bit >= (INT32)bit_width) {
            fprintf(stderr, "[警告] 注入比特 %d 超出寄存器位宽 %u，调整为 %u\n",
                    bit, bit_width, bit_width - 1);
            bit = bit_width - 1;
        }

        // 填充注错信息
        g_inject_info.inject_pc = ip;
        g_inject_info.inject_inst = g_target_inst_disasm;
        g_inject_info.inject_reg = target_reg.Value();
        g_inject_info.inject_kth = g_exec_count;
        g_inject_info.next_pc = g_next_pc;

        // 执行注错
        if (REG_is_xmm(g_target_reg_enum)) {
            inject_fault_xmm(ctxt, g_target_reg_enum, bit);
        } else if (REG_is_ymm(g_target_reg_enum)) {
            inject_fault_ymm(ctxt, g_target_reg_enum, bit);
        } else {
            inject_fault_gpr(ctxt, g_target_reg_enum, bit);
        }

        // 写入注错信息
        write_inject_info();

        g_injected = true;

        fprintf(stderr, "[targeted_fi] 注错完成: %s bit %d (0x%lx -> 0x%lx)\n",
                target_reg.Value().c_str(), bit,
                g_inject_info.original_value, g_inject_info.injected_value);

        // 继续执行
        PIN_ExecuteAt(ctxt);
    }
}

// ========== 插桩函数 ==========

VOID Instruction(INS ins, VOID* v) {
    ADDRINT ip = INS_Address(ins);

    // 检查是否为目标指令
    if (ip == target_pc.Value()) {
        fprintf(stderr, "[targeted_fi] 找到目标指令: 0x%lx: %s\n",
                ip, INS_Disassemble(ins).c_str());

        // 保存指令信息
        g_target_inst_disasm = INS_Disassemble(ins);
        g_next_pc = INS_NextAddress(ins);

        // 提取内存操作信息
        if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
            REG base_reg = INS_MemoryBaseReg(ins);
            REG index_reg = INS_MemoryIndexReg(ins);

            if (REG_valid(base_reg)) {
                g_inject_info.base = REG_StringShort(base_reg);
            } else {
                g_inject_info.base = "none";
            }

            if (REG_valid(index_reg)) {
                g_inject_info.index = REG_StringShort(index_reg);
                g_inject_info.scale = INS_MemoryScale(ins);
            } else {
                g_inject_info.index = "none";
                g_inject_info.scale = 0;
            }

            g_inject_info.displacement = INS_MemoryDisplacement(ins);
        } else {
            g_inject_info.base = "none";
            g_inject_info.index = "none";
            g_inject_info.displacement = 0;
            g_inject_info.scale = 0;
        }

        // 提取写寄存器
        UINT32 max_writes = INS_MaxNumWRegs(ins);
        std::string regw_list = "";
        for (UINT32 i = 0; i < max_writes; i++) {
            REG reg = INS_RegW(ins, i);
            if (REG_valid(reg) && !REG_is_flags(reg)) {
                if (!regw_list.empty()) regw_list += ", ";
                regw_list += REG_StringShort(reg);
            }
        }
        g_inject_info.regw_list = regw_list.empty() ? "none" : regw_list;

        // 检查是否为栈写
        g_inject_info.stackw = INS_IsStackWrite(ins) ? "yes" : "no";

        // 插入分析回调（IPOINT_AFTER）
        INS_InsertCall(
            ins, IPOINT_AFTER, (AFUNPTR)Analyze_TargetInst,
            IARG_THREAD_ID,
            IARG_INST_PTR,
            IARG_CONTEXT,
            IARG_PTR, NULL,
            IARG_END
        );
    }
}

// ========== Fini 回调 ==========

VOID Fini(INT32 code, VOID* v) {
    fprintf(stderr, "[targeted_fi] 程序退出，目标指令共执行 %lu 次\n", g_exec_count);

    if (!g_injected) {
        fprintf(stderr, "[警告] 未执行注错！目标指令执行次数 %lu < 目标次数 %lu\n",
                g_exec_count, target_kth.Value());
    }
}

// ========== 主函数 ==========

INT32 Usage() {
    cerr << "targeted_faultinjection - 定向故障注入工具" << endl;
    cerr << endl;
    cerr << "用法：" << endl;
    cerr << "  pin -t targeted_faultinjection.so \\" << endl;
    cerr << "      -target_pc 0x4019c8 \\" << endl;
    cerr << "      -target_reg rax \\" << endl;
    cerr << "      -target_kth 459 \\" << endl;
    cerr << "      -inject_bit -1 \\" << endl;
    cerr << "      -o inject_info.txt \\" << endl;
    cerr << "      -- ./program args" << endl;
    cerr << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char* argv[]) {
    // 初始化 Pin
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    // 检查必需参数
    if (target_pc.Value() == 0) {
        fprintf(stderr, "[错误] 必须指定 -target_pc 参数\n");
        return Usage();
    }

    if (target_reg.Value().empty()) {
        fprintf(stderr, "[错误] 必须指定 -target_reg 参数\n");
        return Usage();
    }

    // 解析目标寄存器
    g_target_reg_enum = parse_target_register(target_reg.Value());
    fprintf(stderr, "[targeted_fi] 目标寄存器: %s (Pin REG: %s)\n",
            target_reg.Value().c_str(), REG_StringShort(g_target_reg_enum).c_str());

    // 打印配置信息
    fprintf(stderr, "[targeted_fi] 目标 PC: 0x%lx\n", target_pc.Value());
    fprintf(stderr, "[targeted_fi] 目标执行次数: %lu\n", target_kth.Value());
    fprintf(stderr, "[targeted_fi] 注入比特: %d %s\n",
            inject_bit.Value(),
            inject_bit.Value() == -1 ? "(随机)" : "");
    fprintf(stderr, "[targeted_fi] 输出文件: %s\n", output_file.Value().c_str());

    // 注册回调
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // 开始执行
    PIN_StartProgram();

    return 0;
}
