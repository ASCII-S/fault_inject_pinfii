/**
 * @file lib_classifier.h
 * @brief 库函数分类器
 *
 * 通过函数名模式匹配识别和分类外部库函数
 * 支持的库类型：math, blas, lapack, memory, io, string, mpi, omp, pthread
 */

#ifndef LIB_CLASSIFIER_H
#define LIB_CLASSIFIER_H

#include "app_profiler.h"
#include <string>
#include <cstring>

using namespace std;

/**
 * @brief 检查字符串是否以指定前缀开头
 */
inline bool StartsWith(const string& str, const char* prefix) {
    return str.compare(0, strlen(prefix), prefix) == 0;
}

/**
 * @brief 检查字符串是否包含指定子串
 */
inline bool Contains(const string& str, const char* substr) {
    return str.find(substr) != string::npos;
}

/**
 * @brief 检查是否为数学库函数
 *
 * 包括：三角函数、指数对数、幂运算、舍入、特殊函数等
 */
inline bool IsMathLibFunc(const string& name) {
    // 基础数学函数
    if (name == "sin" || name == "cos" || name == "tan" ||
        name == "asin" || name == "acos" || name == "atan" || name == "atan2" ||
        name == "sinh" || name == "cosh" || name == "tanh" ||
        name == "asinh" || name == "acosh" || name == "atanh") {
        return true;
    }

    // 指数对数
    if (name == "exp" || name == "exp2" || name == "expm1" ||
        name == "log" || name == "log2" || name == "log10" || name == "log1p") {
        return true;
    }

    // 幂运算和开方
    if (name == "pow" || name == "sqrt" || name == "cbrt" || name == "hypot") {
        return true;
    }

    // 舍入和取整
    if (name == "ceil" || name == "floor" || name == "trunc" ||
        name == "round" || name == "nearbyint" || name == "rint") {
        return true;
    }

    // 绝对值和符号
    if (name == "fabs" || name == "abs" || name == "copysign" || name == "fmod" ||
        name == "remainder" || name == "remquo") {
        return true;
    }

    // 特殊函数
    if (name == "erf" || name == "erfc" || name == "tgamma" || name == "lgamma" ||
        name == "j0" || name == "j1" || name == "jn" ||
        name == "y0" || name == "y1" || name == "yn") {
        return true;
    }

    // 浮点操作
    if (name == "frexp" || name == "ldexp" || name == "modf" ||
        name == "scalbn" || name == "scalbln" || name == "ilogb" || name == "logb" ||
        name == "fma" || name == "fdim" || name == "fmax" || name == "fmin") {
        return true;
    }

    // 浮点分类
    if (name == "fpclassify" || name == "isfinite" || name == "isinf" ||
        name == "isnan" || name == "isnormal" || name == "signbit") {
        return true;
    }

    // 单精度版本 (f后缀)
    if (name == "sinf" || name == "cosf" || name == "tanf" ||
        name == "expf" || name == "logf" || name == "powf" || name == "sqrtf" ||
        name == "fabsf" || name == "ceilf" || name == "floorf" || name == "roundf") {
        return true;
    }

    // 长双精度版本 (l后缀)
    if (name == "sinl" || name == "cosl" || name == "tanl" ||
        name == "expl" || name == "logl" || name == "powl" || name == "sqrtl" ||
        name == "fabsl" || name == "ceill" || name == "floorl" || name == "roundl") {
        return true;
    }

    // IEEE 754 内部实现
    if (StartsWith(name, "__ieee754_") || StartsWith(name, "__libm_")) {
        return true;
    }

    // glibc 内部数学函数实现 (如 sinf64, cosf64, expf64, logf64 等)
    // 格式: <func>f32, <func>f64, <func>f128, <func>f32x, <func>f64x
    if (name.length() > 3) {
        // 检查是否以 f32, f64, f128 等结尾
        size_t pos = name.rfind("f32");
        if (pos == string::npos) pos = name.rfind("f64");
        if (pos == string::npos) pos = name.rfind("f128");
        if (pos != string::npos && pos > 0) {
            string base = name.substr(0, pos);
            // 检查基础函数名是否为数学函数
            if (base == "sin" || base == "cos" || base == "tan" ||
                base == "asin" || base == "acos" || base == "atan" ||
                base == "sinh" || base == "cosh" || base == "tanh" ||
                base == "exp" || base == "exp2" || base == "expm1" ||
                base == "log" || base == "log2" || base == "log10" || base == "log1p" ||
                base == "pow" || base == "sqrt" || base == "cbrt" ||
                base == "ceil" || base == "floor" || base == "trunc" || base == "round" ||
                base == "fabs" || base == "fmod" || base == "fma" ||
                base == "erf" || base == "erfc" || base == "lgamma" || base == "tgamma") {
                return true;
            }
        }
    }

    // __sin, __cos 等内部实现
    if (StartsWith(name, "__sin") || StartsWith(name, "__cos") ||
        StartsWith(name, "__tan") || StartsWith(name, "__exp") ||
        StartsWith(name, "__log") || StartsWith(name, "__pow") ||
        StartsWith(name, "__sqrt") || StartsWith(name, "__fma")) {
        return true;
    }

    return false;
}

/**
 * @brief 检查是否为BLAS库函数
 *
 * 包括：Level 1/2/3 BLAS 操作
 * 注意：Fortran风格函数名通常带下划线后缀
 * BLAS函数名格式：[sdcz]<operation>[_] 或 cblas_<operation>
 */
inline bool IsBlasLibFunc(const string& name) {
    // CBLAS 前缀 (最可靠的匹配)
    if (StartsWith(name, "cblas_")) {
        return true;
    }

    // BLAS函数通常以 s/d/c/z 开头（表示精度），后跟操作名
    // 检查是否符合 BLAS 命名模式
    if (name.length() < 4) return false;

    char prefix = name[0];
    // BLAS 精度前缀: s(单精度), d(双精度), c(复数单精度), z(复数双精度)
    if (prefix != 's' && prefix != 'd' && prefix != 'c' && prefix != 'z') {
        return false;
    }

    // 提取操作名部分（去掉精度前缀和可能的下划线后缀）
    string op = name.substr(1);
    if (!op.empty() && op.back() == '_') {
        op = op.substr(0, op.length() - 1);
    }

    // Level 1 BLAS (向量操作)
    if (op == "axpy" || op == "copy" || op == "scal" || op == "swap" ||
        op == "dot" || op == "dotu" || op == "dotc" ||
        op == "nrm2" || op == "asum" ||
        op == "amax" || op == "iamax" ||
        op == "rotg" || op == "rotmg" || op == "rot" || op == "rotm") {
        return true;
    }

    // Level 2 BLAS (矩阵-向量操作)
    if (op == "gemv" || op == "gbmv" ||
        op == "symv" || op == "sbmv" || op == "spmv" ||
        op == "hemv" || op == "hbmv" || op == "hpmv" ||
        op == "trmv" || op == "tbmv" || op == "tpmv" ||
        op == "trsv" || op == "tbsv" || op == "tpsv" ||
        op == "ger" || op == "geru" || op == "gerc" ||
        op == "syr" || op == "spr" || op == "her" || op == "hpr" ||
        op == "syr2" || op == "spr2" || op == "her2" || op == "hpr2") {
        return true;
    }

    // Level 3 BLAS (矩阵-矩阵操作)
    if (op == "gemm" || op == "symm" || op == "hemm" ||
        op == "syrk" || op == "herk" || op == "syr2k" || op == "her2k" ||
        op == "trmm" || op == "trsm") {
        return true;
    }

    return false;
}

/**
 * @brief 检查是否为LAPACK库函数
 *
 * 包括：线性方程组求解、特征值、奇异值分解等
 * LAPACK函数名格式：[sdcz]<operation>[_] 或 LAPACKE_<operation>
 */
inline bool IsLapackLibFunc(const string& name) {
    // LAPACKE 前缀 (最可靠的匹配)
    if (StartsWith(name, "LAPACKE_") || StartsWith(name, "lapacke_")) {
        return true;
    }

    // LAPACK函数通常以 s/d/c/z 开头（表示精度）
    if (name.length() < 5) return false;

    char prefix = name[0];
    if (prefix != 's' && prefix != 'd' && prefix != 'c' && prefix != 'z') {
        return false;
    }

    // 提取操作名部分
    string op = name.substr(1);
    if (!op.empty() && op.back() == '_') {
        op = op.substr(0, op.length() - 1);
    }

    // LU分解和求解
    if (op == "getrf" || op == "getrs" || op == "getri" || op == "gesv") {
        return true;
    }

    // Cholesky分解
    if (op == "potrf" || op == "potrs" || op == "potri" || op == "posv") {
        return true;
    }

    // QR分解
    if (op == "geqrf" || op == "orgqr" || op == "ormqr" || op == "ungqr") {
        return true;
    }

    // 特征值
    if (op == "syev" || op == "heev" || op == "geev" ||
        op == "syevd" || op == "heevd" || op == "geevx") {
        return true;
    }

    // SVD
    if (op == "gesvd" || op == "gesdd") {
        return true;
    }

    // 最小二乘
    if (op == "gels" || op == "gelss" || op == "gelsd" || op == "gelsy") {
        return true;
    }

    return false;
}

/**
 * @brief 检查是否为内存管理函数
 */
inline bool IsMemoryLibFunc(const string& name) {
    // 标准内存分配
    if (name == "malloc" || name == "calloc" || name == "realloc" ||
        name == "free" || name == "aligned_alloc" || name == "posix_memalign") {
        return true;
    }

    // 内存操作
    if (name == "memcpy" || name == "memmove" || name == "memset" ||
        name == "memcmp" || name == "memchr") {
        return true;
    }

    // C++ 内存管理
    if (StartsWith(name, "_Znw") || StartsWith(name, "_Zna") ||  // operator new
        StartsWith(name, "_Zdl") || StartsWith(name, "_Zda")) {  // operator delete
        return true;
    }

    // mmap相关
    if (name == "mmap" || name == "munmap" || name == "mprotect" ||
        name == "mremap" || name == "brk" || name == "sbrk") {
        return true;
    }

    return false;
}

/**
 * @brief 检查是否为IO库函数
 */
inline bool IsIOLibFunc(const string& name) {
    // 标准输出
    if (name == "printf" || name == "fprintf" || name == "sprintf" ||
        name == "snprintf" || name == "vprintf" || name == "vfprintf" ||
        name == "puts" || name == "fputs" || name == "putchar" || name == "fputc") {
        return true;
    }

    // 标准输入
    if (name == "scanf" || name == "fscanf" || name == "sscanf" ||
        name == "gets" || name == "fgets" || name == "getchar" || name == "fgetc" ||
        name == "getc" || name == "ungetc") {
        return true;
    }

    // 文件操作
    if (name == "fopen" || name == "fclose" || name == "fread" || name == "fwrite" ||
        name == "fseek" || name == "ftell" || name == "rewind" || name == "fflush" ||
        name == "feof" || name == "ferror" || name == "clearerr") {
        return true;
    }

    // 低级IO
    if (name == "open" || name == "close" || name == "read" || name == "write" ||
        name == "lseek" || name == "dup" || name == "dup2" || name == "pipe") {
        return true;
    }

    // 格式化
    if (StartsWith(name, "__printf") || StartsWith(name, "__fprintf") ||
        StartsWith(name, "_IO_")) {
        return true;
    }

    return false;
}

/**
 * @brief 检查是否为字符串库函数
 */
inline bool IsStringLibFunc(const string& name) {
    // 字符串操作
    if (name == "strlen" || name == "strcpy" || name == "strncpy" ||
        name == "strcat" || name == "strncat" || name == "strcmp" ||
        name == "strncmp" || name == "strchr" || name == "strrchr" ||
        name == "strstr" || name == "strtok" || name == "strdup") {
        return true;
    }

    // 字符串转换
    if (name == "atoi" || name == "atol" || name == "atof" ||
        name == "strtol" || name == "strtoll" || name == "strtoul" ||
        name == "strtoull" || name == "strtod" || name == "strtof") {
        return true;
    }

    // 字符分类
    if (name == "isalpha" || name == "isdigit" || name == "isalnum" ||
        name == "isspace" || name == "isupper" || name == "islower" ||
        name == "toupper" || name == "tolower") {
        return true;
    }

    return false;
}

/**
 * @brief 检查是否为MPI库函数
 */
inline bool IsMPILibFunc(const string& name) {
    if (StartsWith(name, "MPI_") || StartsWith(name, "mpi_") ||
        StartsWith(name, "PMPI_") || StartsWith(name, "pmpi_")) {
        return true;
    }
    return false;
}

/**
 * @brief 检查是否为OpenMP库函数
 */
inline bool IsOMPLibFunc(const string& name) {
    if (StartsWith(name, "omp_") || StartsWith(name, "OMP_") ||
        StartsWith(name, "GOMP_") || StartsWith(name, "__kmp")) {
        return true;
    }
    return false;
}

/**
 * @brief 检查是否为Pthread库函数
 */
inline bool IsPthreadLibFunc(const string& name) {
    if (StartsWith(name, "pthread_")) {
        return true;
    }
    // 同步原语
    if (name == "sem_init" || name == "sem_wait" || name == "sem_post" ||
        name == "sem_destroy" || name == "sem_trywait") {
        return true;
    }
    return false;
}

/**
 * @brief 分类库函数
 *
 * @param name 函数名
 * @return LibCategory 库分类
 */
inline LibCategory ClassifyLibFunction(const string& name) {
    // 按优先级检查各类库
    if (IsMathLibFunc(name))     return LIB_MATH;
    if (IsBlasLibFunc(name))     return LIB_BLAS;
    if (IsLapackLibFunc(name))   return LIB_LAPACK;
    if (IsMemoryLibFunc(name))   return LIB_MEMORY;
    if (IsIOLibFunc(name))       return LIB_IO;
    if (IsStringLibFunc(name))   return LIB_STRING;
    if (IsMPILibFunc(name))      return LIB_MPI;
    if (IsOMPLibFunc(name))      return LIB_OMP;
    if (IsPthreadLibFunc(name))  return LIB_PTHREAD;

    return LIB_OTHER;
}

#endif // LIB_CLASSIFIER_H
