#ifndef INSTRUCTION_PROFILER_H
#define INSTRUCTION_PROFILER_H

#include "pin.H"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

// 指令剖析数据结构
struct InstructionProfile {
    // A类：指令标识
    std::string offset;          // 相对于镜像基址的偏移
    std::string mnemonic;        // 指令助记符
    std::string disasm;          // 完整反汇编文本
    UINT32 size;                 // 指令字节长度

    // B类：指令分类
    std::string category;        // XED 指令类别
    bool is_arith;               // 算术指令
    bool is_logic;               // 逻辑指令
    bool is_float;               // 浮点指令
    bool is_simd;                // SIMD 指令
    bool is_data_move;           // 数据移动指令

    // C类：寄存器特征
    std::vector<std::string> explicit_reg_read;   // 显式读寄存器列表
    std::vector<std::string> explicit_reg_write;  // 显式写寄存器列表
    std::vector<std::string> implicit_reg_read;   // 隐式读寄存器列表
    std::vector<std::string> implicit_reg_write;  // 隐式写寄存器列表
    bool uses_flags;             // 是否读/写标志寄存器

    // D类：访存特征
    bool is_mem_read;            // 是否读内存
    bool is_mem_write;           // 是否写内存
    UINT32 mem_operand_count;    // 内存操作数数量
    std::string mem_access_mode; // 寻址模式

    // E类：控制流特征
    bool is_branch;              // 是否分支
    bool is_cond_branch;         // 是否条件分支
    bool is_call;                // 是否调用
    bool is_ret;                 // 是否返回
    bool is_indirect;            // 是否间接跳转/调用

    // F类：故障敏感性
    bool is_crash_prone;         // 是否易崩溃指令
    std::string crash_prone_type; // 易崩溃类型

    // 构造函数
    InstructionProfile() :
        size(0),
        is_arith(false),
        is_logic(false),
        is_float(false),
        is_simd(false),
        is_data_move(false),
        uses_flags(false),
        is_mem_read(false),
        is_mem_write(false),
        mem_operand_count(0),
        mem_access_mode("none"),
        is_branch(false),
        is_cond_branch(false),
        is_call(false),
        is_ret(false),
        is_indirect(false),
        is_crash_prone(false),
        crash_prone_type("none") {}
};

// 辅助函数：将地址转换为十六进制字符串
inline std::string AddrToHex(ADDRINT addr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << addr;
    return oss.str();
}

// 辅助函数：将字符串向量转换为 JSON 数组字符串
inline std::string VectorToJsonArray(const std::vector<std::string>& vec) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        oss << "\"" << vec[i] << "\"";
        if (i < vec.size() - 1) {
            oss << ", ";
        }
    }
    oss << "]";
    return oss.str();
}

// 辅助函数：布尔值转换为 JSON 字符串
inline std::string BoolToJson(bool val) {
    return val ? "true" : "false";
}

// 辅助函数：转义 JSON 字符串中的特殊字符
inline std::string EscapeJsonString(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << c; break;
        }
    }
    return oss.str();
}

// 辅助函数：将 InstructionProfile 转换为 JSON 字符串
inline std::string ProfileToJson(const InstructionProfile& prof, const std::string& indent = "    ") {
    std::ostringstream oss;
    oss << indent << "{\n";

    // A类：指令标识
    oss << indent << "  \"offset\": \"" << prof.offset << "\",\n";
    oss << indent << "  \"mnemonic\": \"" << prof.mnemonic << "\",\n";
    oss << indent << "  \"disasm\": \"" << EscapeJsonString(prof.disasm) << "\",\n";
    oss << indent << "  \"size\": " << prof.size << ",\n";

    // B类：指令分类
    oss << indent << "  \"category\": \"" << prof.category << "\",\n";
    oss << indent << "  \"is_arith\": " << BoolToJson(prof.is_arith) << ",\n";
    oss << indent << "  \"is_logic\": " << BoolToJson(prof.is_logic) << ",\n";
    oss << indent << "  \"is_float\": " << BoolToJson(prof.is_float) << ",\n";
    oss << indent << "  \"is_simd\": " << BoolToJson(prof.is_simd) << ",\n";
    oss << indent << "  \"is_data_move\": " << BoolToJson(prof.is_data_move) << ",\n";

    // C类：寄存器特征
    oss << indent << "  \"explicit_reg_read\": " << VectorToJsonArray(prof.explicit_reg_read) << ",\n";
    oss << indent << "  \"explicit_reg_write\": " << VectorToJsonArray(prof.explicit_reg_write) << ",\n";
    oss << indent << "  \"implicit_reg_read\": " << VectorToJsonArray(prof.implicit_reg_read) << ",\n";
    oss << indent << "  \"implicit_reg_write\": " << VectorToJsonArray(prof.implicit_reg_write) << ",\n";
    oss << indent << "  \"uses_flags\": " << BoolToJson(prof.uses_flags) << ",\n";

    // D类：访存特征
    oss << indent << "  \"is_mem_read\": " << BoolToJson(prof.is_mem_read) << ",\n";
    oss << indent << "  \"is_mem_write\": " << BoolToJson(prof.is_mem_write) << ",\n";
    oss << indent << "  \"mem_operand_count\": " << prof.mem_operand_count << ",\n";
    oss << indent << "  \"mem_access_mode\": \"" << prof.mem_access_mode << "\",\n";

    // E类：控制流特征
    oss << indent << "  \"is_branch\": " << BoolToJson(prof.is_branch) << ",\n";
    oss << indent << "  \"is_cond_branch\": " << BoolToJson(prof.is_cond_branch) << ",\n";
    oss << indent << "  \"is_call\": " << BoolToJson(prof.is_call) << ",\n";
    oss << indent << "  \"is_ret\": " << BoolToJson(prof.is_ret) << ",\n";
    oss << indent << "  \"is_indirect\": " << BoolToJson(prof.is_indirect) << ",\n";

    // F类：故障敏感性
    oss << indent << "  \"is_crash_prone\": " << BoolToJson(prof.is_crash_prone) << ",\n";
    oss << indent << "  \"crash_prone_type\": \"" << prof.crash_prone_type << "\"\n";

    oss << indent << "}";
    return oss.str();
}

#endif // INSTRUCTION_PROFILER_H
