/*
 * Instruction Profiler - 指令维度剖析工具
 *
 * 功能：对应用程序的每条静态指令进行语义剖析，输出指令级别的特征指标
 * 用途：通过崩溃时的指令地址匹配到该指令的静态特征
 */

#include "pin.H"
#include "instruction_profiler.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <algorithm>

// 全局变量
static std::vector<InstructionProfile> g_instructions;
static std::string g_main_image_name;
static ADDRINT g_main_image_base = 0;
static ADDRINT g_main_image_low = 0;
static ADDRINT g_main_image_high = 0;

// 命令行参数
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "instruction_profile.json", "输出文件名");

// 判断指令类别是否为算术指令
static bool IsArithCategory(UINT32 category) {
    return category == XED_CATEGORY_BINARY ||
           category == XED_CATEGORY_DECIMAL ||
           category == XED_CATEGORY_AVX2 ||
           category == XED_CATEGORY_AVX ||
           category == XED_CATEGORY_SSE;
}

// 判断指令类别是否为逻辑指令
static bool IsLogicCategory(UINT32 category) {
    return category == XED_CATEGORY_LOGICAL ||
           category == XED_CATEGORY_BITBYTE ||
           category == XED_CATEGORY_SHIFT ||
           category == XED_CATEGORY_ROTATE;
}

// 判断指令类别是否为浮点指令
static bool IsFloatCategory(UINT32 category) {
    return category == XED_CATEGORY_X87_ALU ||
           category == XED_CATEGORY_FCMOV ||
           category == XED_CATEGORY_SSE;
}

// 判断指令类别是否为 SIMD 指令
static bool IsSIMDCategory(UINT32 category) {
    return category == XED_CATEGORY_SSE ||
           category == XED_CATEGORY_AVX ||
           category == XED_CATEGORY_AVX2 ||
           category == XED_CATEGORY_AVX512 ||
           category == XED_CATEGORY_MMX;
}

// 判断指令类别是否为数据移动指令
static bool IsDataMoveCategory(UINT32 category) {
    return category == XED_CATEGORY_DATAXFER ||
           category == XED_CATEGORY_PUSH ||
           category == XED_CATEGORY_POP;
}

// 判断寄存器是否为标志寄存器
static bool IsFlagsReg(REG reg) {
    return reg == REG_GFLAGS || reg == REG_EFLAGS || reg == REG_FLAGS;
}

// 判断寄存器是否为栈指针相关
static bool IsStackReg(REG reg) {
    return reg == REG_RSP || reg == REG_ESP || reg == REG_SP ||
           reg == REG_RBP || reg == REG_EBP || reg == REG_BP;
}

// 分析内存访问模式
static std::string AnalyzeMemAccessMode(INS ins) {
    if (!INS_IsMemoryRead(ins) && !INS_IsMemoryWrite(ins)) {
        return "none";
    }

    UINT32 memOps = INS_MemoryOperandCount(ins);
    if (memOps == 0) {
        return "none";
    }

    // 遍历所有内存操作数，分析寻址模式
    for (UINT32 i = 0; i < memOps; i++) {
        REG baseReg = INS_OperandMemoryBaseReg(ins, INS_MemoryOperandIndexToOperandIndex(ins, i));
        REG indexReg = INS_OperandMemoryIndexReg(ins, INS_MemoryOperandIndexToOperandIndex(ins, i));

        // RIP 相对寻址
        if (baseReg == REG_RIP || baseReg == REG_EIP || baseReg == REG_IP) {
            return "rip_relative";
        }

        // 栈访问
        if (IsStackReg(baseReg) && indexReg == REG_INVALID()) {
            return "stack";
        }

        // 数组式访问 (base + index * scale)
        if (indexReg != REG_INVALID()) {
            return "array";
        }

        // 指针解引用 (base + disp 或仅 base)
        if (baseReg != REG_INVALID()) {
            return "pointer";
        }

        // 绝对地址
        if (baseReg == REG_INVALID() && indexReg == REG_INVALID()) {
            return "absolute";
        }
    }

    return "pointer";
}

// 分析易崩溃类型
static std::string AnalyzeCrashProneType(INS ins) {
    // 除法指令 - 通过助记符判断
    std::string mnemonic = INS_Mnemonic(ins);
    if (mnemonic == "DIV" || mnemonic == "IDIV" ||
        mnemonic == "DIVSS" || mnemonic == "DIVSD" ||
        mnemonic == "DIVPS" || mnemonic == "DIVPD") {
        return "div";
    }

    // 间接控制流
    if (INS_IsIndirectControlFlow(ins)) {
        return "indirect_cf";
    }

    // 内存写
    if (INS_IsMemoryWrite(ins)) {
        return "mem_write";
    }

    // 内存读
    if (INS_IsMemoryRead(ins)) {
        return "mem_read";
    }

    return "none";
}

// 获取显式读寄存器列表
static std::vector<std::string> GetExplicitReadRegs(INS ins) {
    std::vector<std::string> regs;
    std::set<std::string> seen;

    UINT32 numOps = INS_OperandCount(ins);
    for (UINT32 i = 0; i < numOps; i++) {
        if (INS_OperandIsReg(ins, i) && INS_OperandRead(ins, i)) {
            REG reg = INS_OperandReg(ins, i);
            if (REG_valid(reg)) {
                std::string regName = REG_StringShort(reg);
                if (seen.find(regName) == seen.end()) {
                    regs.push_back(regName);
                    seen.insert(regName);
                }
            }
        }
        // 内存操作数中的基址和索引寄存器也是显式读
        if (INS_OperandIsMemory(ins, i)) {
            REG baseReg = INS_OperandMemoryBaseReg(ins, i);
            REG indexReg = INS_OperandMemoryIndexReg(ins, i);
            if (REG_valid(baseReg)) {
                std::string regName = REG_StringShort(baseReg);
                if (seen.find(regName) == seen.end()) {
                    regs.push_back(regName);
                    seen.insert(regName);
                }
            }
            if (REG_valid(indexReg)) {
                std::string regName = REG_StringShort(indexReg);
                if (seen.find(regName) == seen.end()) {
                    regs.push_back(regName);
                    seen.insert(regName);
                }
            }
        }
    }
    return regs;
}

// 获取显式写寄存器列表
static std::vector<std::string> GetExplicitWriteRegs(INS ins) {
    std::vector<std::string> regs;
    std::set<std::string> seen;

    UINT32 numOps = INS_OperandCount(ins);
    for (UINT32 i = 0; i < numOps; i++) {
        if (INS_OperandIsReg(ins, i) && INS_OperandWritten(ins, i)) {
            REG reg = INS_OperandReg(ins, i);
            if (REG_valid(reg)) {
                std::string regName = REG_StringShort(reg);
                if (seen.find(regName) == seen.end()) {
                    regs.push_back(regName);
                    seen.insert(regName);
                }
            }
        }
    }
    return regs;
}

// 获取隐式读寄存器列表
static std::vector<std::string> GetImplicitReadRegs(INS ins) {
    std::vector<std::string> regs;
    std::set<std::string> explicitRegs;
    std::set<std::string> seen;

    // 先收集显式读寄存器
    UINT32 numOps = INS_OperandCount(ins);
    for (UINT32 i = 0; i < numOps; i++) {
        if (INS_OperandIsReg(ins, i) && INS_OperandRead(ins, i)) {
            REG reg = INS_OperandReg(ins, i);
            if (REG_valid(reg)) {
                explicitRegs.insert(REG_StringShort(reg));
            }
        }
        if (INS_OperandIsMemory(ins, i)) {
            REG baseReg = INS_OperandMemoryBaseReg(ins, i);
            REG indexReg = INS_OperandMemoryIndexReg(ins, i);
            if (REG_valid(baseReg)) {
                explicitRegs.insert(REG_StringShort(baseReg));
            }
            if (REG_valid(indexReg)) {
                explicitRegs.insert(REG_StringShort(indexReg));
            }
        }
    }

    // 遍历所有读寄存器，排除显式的
    for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
        REG reg = INS_RegR(ins, i);
        if (REG_valid(reg)) {
            std::string regName = REG_StringShort(reg);
            if (explicitRegs.find(regName) == explicitRegs.end() &&
                seen.find(regName) == seen.end()) {
                regs.push_back(regName);
                seen.insert(regName);
            }
        }
    }
    return regs;
}

// 获取隐式写寄存器列表
static std::vector<std::string> GetImplicitWriteRegs(INS ins) {
    std::vector<std::string> regs;
    std::set<std::string> explicitRegs;
    std::set<std::string> seen;

    // 先收集显式写寄存器
    UINT32 numOps = INS_OperandCount(ins);
    for (UINT32 i = 0; i < numOps; i++) {
        if (INS_OperandIsReg(ins, i) && INS_OperandWritten(ins, i)) {
            REG reg = INS_OperandReg(ins, i);
            if (REG_valid(reg)) {
                explicitRegs.insert(REG_StringShort(reg));
            }
        }
    }

    // 遍历所有写寄存器，排除显式的
    for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
        REG reg = INS_RegW(ins, i);
        if (REG_valid(reg)) {
            std::string regName = REG_StringShort(reg);
            if (explicitRegs.find(regName) == explicitRegs.end() &&
                seen.find(regName) == seen.end()) {
                regs.push_back(regName);
                seen.insert(regName);
            }
        }
    }
    return regs;
}

// 检查是否使用标志寄存器
static bool CheckUsesFlags(INS ins) {
    // 检查读寄存器
    for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); i++) {
        REG reg = INS_RegR(ins, i);
        if (IsFlagsReg(reg)) {
            return true;
        }
    }
    // 检查写寄存器
    for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); i++) {
        REG reg = INS_RegW(ins, i);
        if (IsFlagsReg(reg)) {
            return true;
        }
    }
    return false;
}

// 分析单条指令
static InstructionProfile AnalyzeInstruction(INS ins, ADDRINT imageBase) {
    InstructionProfile prof;

    // A类：指令标识
    ADDRINT addr = INS_Address(ins);
    prof.offset = AddrToHex(addr - imageBase);
    prof.mnemonic = INS_Mnemonic(ins);
    prof.disasm = INS_Disassemble(ins);
    prof.size = INS_Size(ins);

    // B类：指令分类
    UINT32 category = INS_Category(ins);
    prof.category = CATEGORY_StringShort(category);
    prof.is_arith = IsArithCategory(category);
    prof.is_logic = IsLogicCategory(category);
    prof.is_float = IsFloatCategory(category);
    prof.is_simd = IsSIMDCategory(category);
    prof.is_data_move = IsDataMoveCategory(category);

    // C类：寄存器特征
    prof.explicit_reg_read = GetExplicitReadRegs(ins);
    prof.explicit_reg_write = GetExplicitWriteRegs(ins);
    prof.implicit_reg_read = GetImplicitReadRegs(ins);
    prof.implicit_reg_write = GetImplicitWriteRegs(ins);
    prof.uses_flags = CheckUsesFlags(ins);

    // D类：访存特征
    prof.is_mem_read = INS_IsMemoryRead(ins);
    prof.is_mem_write = INS_IsMemoryWrite(ins);
    prof.mem_operand_count = INS_MemoryOperandCount(ins);
    prof.mem_access_mode = AnalyzeMemAccessMode(ins);

    // E类：控制流特征
    prof.is_branch = INS_IsBranch(ins);
    prof.is_cond_branch = INS_IsBranch(ins) && INS_HasFallThrough(ins);
    prof.is_call = INS_IsCall(ins);
    prof.is_ret = INS_IsRet(ins);
    prof.is_indirect = INS_IsIndirectControlFlow(ins);

    // F类：故障敏感性
    prof.crash_prone_type = AnalyzeCrashProneType(ins);
    prof.is_crash_prone = (prof.crash_prone_type != "none");

    return prof;
}

// 镜像加载回调
static VOID ImageLoad(IMG img, VOID* v) {
    // 只分析主程序
    if (!IMG_IsMainExecutable(img)) {
        return;
    }

    g_main_image_name = IMG_Name(img);
    g_main_image_base = IMG_LowAddress(img);
    g_main_image_low = IMG_LowAddress(img);
    g_main_image_high = IMG_HighAddress(img);

    std::cerr << "[Instruction Profiler] Analyzing main image: " << g_main_image_name << std::endl;
    std::cerr << "[Instruction Profiler] Base address: " << AddrToHex(g_main_image_base) << std::endl;

    // 遍历所有段
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        // 只分析可执行段
        if (!SEC_IsExecutable(sec)) {
            continue;
        }

        std::cerr << "[Instruction Profiler] Analyzing section: " << SEC_Name(sec) << std::endl;

        // 遍历段中的所有例程
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);

            // 遍历例程中的所有指令
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                InstructionProfile prof = AnalyzeInstruction(ins, g_main_image_base);
                g_instructions.push_back(prof);
            }

            RTN_Close(rtn);
        }
    }

    std::cerr << "[Instruction Profiler] Total instructions analyzed: " << g_instructions.size() << std::endl;
}

// 程序结束回调
static VOID Fini(INT32 code, VOID* v) {
    std::ofstream outFile(KnobOutputFile.Value().c_str());
    if (!outFile.is_open()) {
        std::cerr << "[Instruction Profiler] Error: Cannot open output file: "
                  << KnobOutputFile.Value() << std::endl;
        return;
    }

    // 输出 JSON
    outFile << "{\n";

    // tool_info
    outFile << "  \"tool_info\": {\n";
    outFile << "    \"name\": \"Instruction Profiler\",\n";
    outFile << "    \"version\": \"1.0\",\n";
    outFile << "    \"main_image\": \"" << EscapeJsonString(g_main_image_name) << "\",\n";
    outFile << "    \"base_address\": \"" << AddrToHex(g_main_image_base) << "\"\n";
    outFile << "  },\n";

    // instructions
    outFile << "  \"instructions\": [\n";
    for (size_t i = 0; i < g_instructions.size(); ++i) {
        outFile << ProfileToJson(g_instructions[i], "    ");
        if (i < g_instructions.size() - 1) {
            outFile << ",";
        }
        outFile << "\n";
    }
    outFile << "  ],\n";

    // statistics
    outFile << "  \"statistics\": {\n";
    outFile << "    \"total_instructions\": " << g_instructions.size() << "\n";
    outFile << "  }\n";

    outFile << "}\n";

    outFile.close();
    std::cerr << "[Instruction Profiler] Output written to: " << KnobOutputFile.Value() << std::endl;
}

// 使用说明
static INT32 Usage() {
    std::cerr << "Instruction Profiler - 指令维度剖析工具\n\n";
    std::cerr << "用法: pin -t instruction_profiler.so [options] -- <application>\n\n";
    std::cerr << "选项:\n";
    std::cerr << "  -o <file>    输出文件名 (默认: instruction_profile.json)\n";
    std::cerr << "\n";
    return -1;
}

// 主函数
int main(int argc, char* argv[]) {
    // 初始化符号表（需要用于 RTN 遍历）
    PIN_InitSymbols();

    // 初始化 Pin
    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    // 注册回调
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddFiniFunction(Fini, 0);

    // 开始执行
    PIN_StartProgram();

    return 0;
}
