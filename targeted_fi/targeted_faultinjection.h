/*
 * targeted_faultinjection.h - 定向故障注入工具头文件
 *
 * 功能：对指定 PC 的指定寄存器在第 K 次执行时进行精确故障注入
 */

#ifndef TARGETED_FAULTINJECTION_H
#define TARGETED_FAULTINJECTION_H

#include "pin.H"
#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>

// ========== KNOB 参数定义 ==========

KNOB<UINT64> target_pc(KNOB_MODE_WRITEONCE, "pintool",
    "target_pc", "0", "目标指令地址（绝对地址）");

KNOB<std::string> target_reg(KNOB_MODE_WRITEONCE, "pintool",
    "target_reg", "", "目标寄存器名称（如 rax, xmm0）");

KNOB<UINT64> target_kth(KNOB_MODE_WRITEONCE, "pintool",
    "target_kth", "1", "在第 K 次执行时注错（从 1 开始）");

KNOB<INT32> inject_bit(KNOB_MODE_WRITEONCE, "pintool",
    "inject_bit", "-1", "要翻转的比特位置（-1=随机）");

KNOB<BOOL> high_bit_only(KNOB_MODE_WRITEONCE, "pintool",
    "high_bit_only", "0", "是否只在高位注错");

KNOB<std::string> output_file(KNOB_MODE_WRITEONCE, "pintool",
    "o", "inject_info.txt", "注错信息输出文件");

// ========== 寄存器位宽查询 ==========

/**
 * 获取寄存器的位宽
 */
inline UINT32 get_reg_bit_width(REG reg) {
    // 8位寄存器
    if (reg >= REG_AL && reg <= REG_BH) return 8;
    if (reg >= REG_DIL && reg <= REG_SPL) return 8;
    if (reg >= REG_R8B && reg <= REG_R15B) return 8;

    // 16位寄存器
    if (reg >= REG_AX && reg <= REG_FLAGS) return 16;
    if (reg >= REG_R8W && reg <= REG_R15W) return 16;

    // 32位寄存器
    if (reg >= REG_EAX && reg <= REG_EFLAGS) return 32;
    if (reg >= REG_R8D && reg <= REG_R15D) return 32;

    // 64位通用寄存器
    if (reg >= REG_RAX && reg <= REG_RFLAGS) return 64;
    if (reg >= REG_R8 && reg <= REG_R15) return 64;

    // XMM 寄存器 (128位)
    if (REG_is_xmm(reg)) return 128;

    // YMM 寄存器 (256位)
    if (REG_is_ymm(reg)) return 256;

    // MM 寄存器 (64位)
    if (REG_is_mm(reg)) return 64;

    // ST 寄存器 (80位)
    if (REG_is_fr(reg)) return 80;

    return 64;  // 默认64位
}

/**
 * 解析寄存器名称，返回 Pin 的 REG 枚举
 */
REG parse_target_register(const std::string& name);

/**
 * 对通用寄存器进行故障注入
 */
void inject_fault_gpr(CONTEXT* ctxt, REG reg, INT32 bit);

/**
 * 对 XMM 寄存器进行故障注入
 */
void inject_fault_xmm(CONTEXT* ctxt, REG reg, INT32 bit);

/**
 * 对 YMM 寄存器进行故障注入
 */
void inject_fault_ymm(CONTEXT* ctxt, REG reg, INT32 bit);

#endif // TARGETED_FAULTINJECTION_H
