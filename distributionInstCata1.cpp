#include<iostream>
#include<fstream>

#include <set>
#include <map>
#include <string>

#include "pin.H"
#include "utils.h"

//#define INCLUDEALLINST
//#define NOBRANCHES
//#define NOSTACKFRAMEOP
//#define ONLYFP
using std::cerr;
using std::ofstream;
using std::ios;
using std::string;
using std::endl;

#include <fstream>

KNOB<string> inst_type_table_file(KNOB_MODE_WRITEONCE, "pintool",
    "csv_name", "dynamicInst.csv", "output file to store instruction and categories");
KNOB<string> ins_type(KNOB_MODE_WRITEONCE, "pintool",
    "ins_type", "all", "select instruction type so you can generate csv");
KNOB<int> dynamic_analyze(KNOB_MODE_WRITEONCE, "pintool",
    "dynamic_analyze", "0", "enable==1,use dynamic analyze to generate all instructions of a ins_type instead of just catalog");


static std::map<string, std::set<string>* > category_opcode_map;

std::ofstream logFile;  // 用于存储日志的文件流

// 初始化日志文件
void InitLog() {
    string logFilePath = inst_type_table_file.Value();  // 获取指定的输出文件路径
    std::cout << logFilePath <<endl;
    std::remove(logFilePath.c_str());
    logFile.open(logFilePath.c_str(), std::ios::out | std::ios::app);  // 打开文件以进行追加
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logFilePath << std::endl;
    }
}

// 自定义 MYLOG 宏，将日志输出到文件
#define SaveMsgLog(msg)   if (logFile.is_open()) { logFile << msg << std::endl; }  else { std::cerr << "Log file is not open!" << std::endl; }

#define StdMsgLog(msg)   std::cerr << msg << std::endl;

//save one ins as format
inline void SaveInstructionLogByMflag(const std::string& cate, const std::string& mnemonic, const std::string& addressStr,
                                       string insDis, const void* reg_name, UINT32 mflag) {

    // 如果reg_name指向的内容是"*invalid*"，则返回
    const char* reg_str = static_cast<const char*>(reg_name); // 类型转换为const char*
    if (std::strcmp(reg_str, "*invalid*") == 0) {
        return;
    }
    std::ostringstream mflagstr;
    mflagstr << mflag;

    std::string args_regmm;
    std::string args_reg;
    if (mflag == 1) {  // "mem:"
      args_regmm = std::string((const char*)reg_name);
      args_reg = "";
    } else if (mflag == 0) {  // "REG:"
      args_regmm = "";
      args_reg = std::string((const char*)reg_name);
    } else if (mflag == 2) {  // "invalid"
      return;
    }
    SaveMsgLog(args_regmm + "," + args_reg + "," + addressStr + "," + mnemonic + "," + insDis);
}


// 回调函数 docount，用于处理指令信息
VOID docount(INS ins, ADDRINT ip, string cate, string mnemonic, string addressStr, string insDis, VOID *reg_name, UINT32 mflag) {

      SaveInstructionLogByMflag(cate, mnemonic, addressStr, insDis, reg_name, mflag);
}

// Pin calls this function every time a new instruction is encountered
VOID CountInst(INS ins, VOID *v)
{
  if (!isValidInst(ins))
    return;

    
    //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)countAllInst, IARG_END);
#ifdef INCLUDEALLINST
#else

#ifdef NOBRANCHES
  if(INS_IsBranch(ins) || !INS_HasFallThrough(ins)) {
    LOG("instcount: branch/ret inst: " + INS_Disassemble(ins) + "\n");
    //std::cout << INS_Disassemble(ins) <<endl;
		return;
  }
#endif

// NOSTACKFRAMEOP must be used together with NOBRANCHES, IsStackWrite 
// has a bug that does not put pop into the list
#ifdef NOSTACKFRAMEOP
  if(INS_IsStackWrite(ins) || OPCODE_StringShort(INS_Opcode(ins)) == "POP") {
    LOG("instcount: stack frame change inst: " + INS_Disassemble(ins) + "\n");    
    return;
  }
#endif

#ifdef ONLYFP
 	int numW = INS_MaxNumWRegs(ins);
  bool hasfp = false;
  for (int i = 0; i < numW; i++) {
    if (reg_map.isFloatReg(reg)) {
      hasfp = true;
      break;
    }
  }
  if (!hasfp){
    return;  
  }
#endif

#endif

        

        int mflag = 0;
        REG reg;
        const char * reg_name = NULL;
        if (INS_IsMemoryWrite(ins) || INS_IsMemoryRead(ins)) {//内存读写指令
            REG reg = INS_MemoryBaseReg(ins);//获取当前指令的内存基址寄存器
            string *temp = new string(REG_StringShort(reg));
            reg_name = temp->c_str();

            if (!REG_valid(reg)) {
                reg = INS_MemoryIndexReg(ins);//获取给定指令的内存索引寄存器
                string *temp = new string(REG_StringShort(reg));
                reg_name = temp->c_str();
                //OutFile <<"mem:" + REG_StringShort(reg) << endl;//不要打开这些OutFile,除非你仅调试这个工具
            }
            mflag = 1;  //表示ins是内存基址或内存索引寄存器
        }
        else {
            int numW = INS_MaxNumWRegs(ins), randW = 0;
            if (numW > 1)
                randW = rand() % numW;
            else
                randW = 0;

            reg = INS_RegW(ins, randW); //在ins中找随机的写寄存器,排除标志寄存器和无效寄存器
            if (numW > 1 && (reg == REG_RFLAGS || reg == REG_FLAGS || reg == REG_EFLAGS))
                randW = (randW + 1) % numW;
            if (numW > 1 && REG_valid(INS_RegW(ins, randW)))
                reg = INS_RegW(ins, randW);
            else
                reg = INS_RegW(ins, 0);
            if (!REG_valid(reg)) {
                string *temp = new string( "REGNOTVALID: inst " + INS_Disassemble(ins));
                reg_name = temp->c_str();
                //OutFile << "REGNOTVALID: inst " + INS_Disassemble(ins) << endl;
                mflag = 2;
            }
            if (reg == REG_RFLAGS || reg == REG_FLAGS || reg == REG_EFLAGS) {
                string *temp = new string( "REGNOTVALID: inst " + INS_Disassemble(ins));
                reg_name = temp->c_str();
                mflag = 2;
                //OutFile << "REGNOTVALID: inst " + INS_Disassemble(ins) << endl;
            }

            string *temp = new string(REG_StringShort(reg));
            reg_name = temp->c_str();
            //OutFile << "reg:" + REG_StringShort(reg) << endl;
          }
        

  string cate = CATEGORY_StringShort(INS_Category(ins));
  string mnemonic = INS_Mnemonic(ins);
  ADDRINT address = INS_Address(ins);
  string insDis = INS_Disassemble(ins);

  std::stringstream ss;
  ss << std::hex << address;  // 将地址以十六进制输出
  string addressStr = ss.str();  // 获取转换后的字符串
  

string ins_type_value = ins_type.Value();
bool processInstruction = false;
if (ins_type_value == "all" || 
        (ins_type_value == "call_retq" && (cate == "CALL" || cate == "RET")) ||
        (ins_type_value == "stack" && (mnemonic == "PUSH" || mnemonic == "POP")) ||
        (ins_type_value == "mov" && mnemonic == "MOV") ||
        (ins_type_value == "integer" && (
            mnemonic == "ADD" || mnemonic == "SUB"  || mnemonic == "MUL"  || mnemonic == "DIV" || 
            mnemonic == "AND" || mnemonic == "OR"   || mnemonic == "XOR"  || mnemonic == "SHL" || 
            mnemonic == "SHR" || mnemonic == "SAR"  || mnemonic == "TEST" || mnemonic == "INC" || 
            mnemonic == "DEC" )) ||
        (ins_type_value == "float" && ( cate == "SSE" ||
            mnemonic == "ADDPD"   || mnemonic == "ADDPS" || mnemonic == "ADDSD" ||
            mnemonic == "ADDSS"   || mnemonic == "FADD"  || mnemonic == "FADDP" ||
            mnemonic == "MULPD"   || mnemonic == "MULPS" || mnemonic == "MULSD" ||
            mnemonic == "MULSS"   || mnemonic == "FMUL"  || mnemonic == "FMULP" ||
            mnemonic == "DIVPD"   || mnemonic == "DIVPS" || mnemonic == "DIVSD" ||
            mnemonic == "DIVSS"   || mnemonic == "FDIV"  || mnemonic == "FDIVP" ||
            mnemonic == "FDIVR"   || mnemonic == "FDIVRP" || mnemonic == "FSQRT" ||
            mnemonic == "COMISD"  || mnemonic == "COMISS" || mnemonic == "CMPPD" ||
            mnemonic == "CMPPS"   || mnemonic == "CMPSS" || mnemonic == "CMPPS" ||
            mnemonic == "CVTSD2SS"|| mnemonic == "CVTSS2SD" ||
            mnemonic == "VADDPD"   || mnemonic == "VADDPS" || mnemonic == "VADDSD" ||
            mnemonic == "VADDSS"    || mnemonic == "VADDSUBPD" || mnemonic == "VADDSUBPS" ||
            mnemonic == "VAESDEC"   || mnemonic == "VAESDECLAST" || mnemonic == "VAESENC" ||
            mnemonic == "VAESENCLAST" || mnemonic == "VAESIMC" || mnemonic == "VAESKEYGENASSIST" ||
            mnemonic == "VANDNPD"   || mnemonic == "VANDNPS" || mnemonic == "VANDPD" ||
            mnemonic == "VANDPS"    || mnemonic == "VBLENDPD" || mnemonic == "VBLENDPS" ||
            mnemonic == "VBLENDVPD" || mnemonic == "VBLENDVPS" || mnemonic == "VBROADCASTF128" ||
            mnemonic == "VBROADCASTI128" || mnemonic == "VBROADCASTSD" || mnemonic == "VBROADCASTSS" ||
            mnemonic == "VCMPPD"    || mnemonic == "VCMPPS" || mnemonic == "VCMPSD" ||
            mnemonic == "VCMPSS"    || mnemonic == "VCOMISD" || mnemonic == "VCOMISS" ||
            mnemonic == "VCVTDQ2PD" || mnemonic == "VCVTDQ2PS" || mnemonic == "VCVTPD2DQ" ||
            mnemonic == "VCVTPD2PS" || mnemonic == "VCVTPH2PS" || mnemonic == "VCVTPS2DQ" ||
            mnemonic == "VCVTPS2PD" || mnemonic == "VCVTPS2PH" || mnemonic == "VCVTSD2SI" ||
            mnemonic == "VCVTSD2SS" || mnemonic == "VCVTSI2SD" || mnemonic == "VCVTSI2SS" ||
            mnemonic == "VCVTSS2SD" || mnemonic == "VCVTSS2SI" || mnemonic == "VCVTTPD2DQ" ||
            mnemonic == "VCVTTPS2DQ" || mnemonic == "VCVTTSD2SI" || mnemonic == "VCVTTSS2SI" ||
            mnemonic == "VDIVPD"    || mnemonic == "VDIVPS" || mnemonic == "VDIVSD" ||
            mnemonic == "VDIVSS"    || mnemonic == "VDPPD" || mnemonic == "VDPPS" ||
            mnemonic == "VEXTRACTF128" || mnemonic == "VEXTRACTI128" || mnemonic == "VEXTRACTPS" ||
            mnemonic == "VFMADD132PD" || mnemonic == "VFMADD132PS" || mnemonic == "VFMADD132SD" ||
            mnemonic == "VFMADD132SS" || mnemonic == "VFMADD213PD" || mnemonic == "VFMADD213PS" ||
            mnemonic == "VFMADD213SD" || mnemonic == "VFMADD213SS" || mnemonic == "VFMADD231PD" ||
            mnemonic == "VFMADD231PS" || mnemonic == "VFMADD231SD" || mnemonic == "VFMADD231SS" ||
            mnemonic == "VFMADDSUB132PD" || mnemonic == "VFMADDSUB132PS" || mnemonic == "VFMADDSUB213PD" ||
            mnemonic == "VFMADDSUB213PS" || mnemonic == "VFMADDSUB231PD" || mnemonic == "VFMADDSUB231PS" ||
            mnemonic == "VFMSUB132PD" || mnemonic == "VFMSUB132PS" || mnemonic == "VFMSUB132SD" ||
            mnemonic == "VFMSUB132SS" || mnemonic == "VFMSUB213PD" || mnemonic == "VFMSUB213PS" ||
            mnemonic == "VFMSUB213SD" || mnemonic == "VFMSUB213SS" || mnemonic == "VFMSUB231PD" ||
            mnemonic == "VFMSUB231PS" || mnemonic == "VFMSUB231SD" || mnemonic == "VFMSUB231SS" ||
            mnemonic == "VFMADD132PD" || mnemonic == "VFMADD132PS" || mnemonic == "VFMADD132SD" ||
            mnemonic == "VFMADD132SS" || mnemonic == "VFMADD213PD" || mnemonic == "VFMADD213PS" ||
            mnemonic == "VFMADD213SD" || mnemonic == "VFMADD213SS" || mnemonic == "VFMADD231PD" ||
            mnemonic == "VFMADD231PS" || mnemonic == "VFMADD231SD" || mnemonic == "VFMADD231SS" ||
            mnemonic == "VFNMSUB132PD" || mnemonic == "VFNMSUB132PS" || mnemonic == "VFNMSUB132SD" ||
            mnemonic == "VFNMSUB132SS" || mnemonic == "VFNMSUB213PD" || mnemonic == "VFNMSUB213PS" ||
            mnemonic == "VFNMSUB213SD" || mnemonic == "VFNMSUB213SS" || mnemonic == "VFNMSUB231PD" ||
            mnemonic == "VFNMSUB231PS" || mnemonic == "VFNMSUB231SD" || mnemonic == "VFNMSUB231SS" ||
            mnemonic == "VGATHERDPD" || mnemonic == "VGATHERDPS" || mnemonic == "VGATHERQPD" ||
            mnemonic == "VGATHERQPS" || mnemonic == "VHADDPD" || mnemonic == "VHADDPS" ||
            mnemonic == "VHSUBPD"   || mnemonic == "VHSUBPS" || mnemonic == "VINSERTF128" ||
            mnemonic == "VINSERTI128" || mnemonic == "VINSERTPS" || mnemonic == "VLDDQU" ||
            mnemonic == "VLDMXCSR"  || mnemonic == "VMASKMOVDQU" || mnemonic == "VMASKMOVPD" ||
            mnemonic == "VMASKMOVPS" || mnemonic == "VMAXPD" || mnemonic == "VMAXPS" ||
            mnemonic == "VMAXSD"    || mnemonic == "VMAXSS"  ||
            mnemonic == "VPSUBD"   || mnemonic == "VPSUBQ" || mnemonic == "VPSUBSB" ||
            mnemonic == "VPSUBSW"   || mnemonic == "VPSUBUSB" || mnemonic == "VPSUBUSW" ||
            mnemonic == "VPSUBW"    || mnemonic == "VPTEST" || mnemonic == "VPUNPCKHBW" ||
            mnemonic == "VPUNPCKHDQ" || mnemonic == "VPUNPCKHQDQ" || mnemonic == "VPUNPCKHWD" ||
            mnemonic == "VPUNPCKLBW" || mnemonic == "VPUNPCKLDQ" || mnemonic == "VPUNPCKLQDQ" ||
            mnemonic == "VPUNPCKLWD" || mnemonic == "VPXOR" || mnemonic == "VRCPPS" ||
            mnemonic == "VRCPSS"    || mnemonic == "VROUNDPD" || mnemonic == "VROUNDPS" ||
            mnemonic == "VROUNDSD"  || mnemonic == "ROUNDSS" || mnemonic == "VRSQRTPS" ||
            mnemonic == "VRSQRTSS"  || mnemonic == "VSHUFPD" || mnemonic == "VSHUFPS" ||
            mnemonic == "VSQRTPD"   || mnemonic == "VSQRTPS" || mnemonic == "VSQRTSD" ||
            mnemonic == "VSQRTSS"   || mnemonic == "VSTMXCSR" || mnemonic == "VSUBPD" ||
            mnemonic == "VSUBPS"    || mnemonic == "VSUBSD" || mnemonic == "VSUBSS" ||
            mnemonic == "VTESTPD"   || mnemonic == "VTESTPS" || mnemonic == "VUCOMISD" ||
            mnemonic == "VUCOMISS"  || mnemonic == "VUNPCKHPD" || mnemonic == "VUNPCKHPS" ||
            mnemonic == "VUNPCKLPD" || mnemonic == "VUNPCKLPS" || mnemonic == "VXORPD" ||
            mnemonic == "VXORPS"    || mnemonic == "VZEROALL" || mnemonic == "VZEROUPPER" )) ||
        (ins_type_value == "cmp" && mnemonic == "CMP")  ||
        (ins_type_value == "div" && (mnemonic == "DIVSD" ||  mnemonic == "IDIV")  )) {
        
        if (ins_type_value == "mov" || ins_type_value == "integer" || ins_type_value == "float"||ins_type_value == "div") {
            if (INS_IsMemoryWrite(ins) || INS_IsMemoryRead(ins)) {
                processInstruction = true;
            }
        }
        else {
            processInstruction = true; // For other types of instructions
        }
        }
  int da = dynamic_analyze.Value();
  if (processInstruction){
    if (da == 0)
      SaveInstructionLogByMflag(cate, mnemonic, addressStr, insDis, reg_name, mflag);
    else if (da == 1)
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount,
                    IARG_PTR, ins,  // 传递指令对象 ins
                    IARG_INST_PTR,  // 获取指令的地址（IP）
                    IARG_PTR, new string(cate),  // 将类别传递给回调
                    IARG_PTR, new string(mnemonic),  // 将助记符传递给回调
                    IARG_PTR, new string(addressStr),  // 将地址传递给回调
                    IARG_PTR, new string(insDis),  // 将反汇编指令传递给回调
                    IARG_PTR, new string(reg_name),
                    IARG_UINT32, mflag,
                    IARG_END);
    }
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
  
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage() {
    cerr << "This tool collects the instruction categories/opcode in the program" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}


int main(int argc, char * argv[])
{
    
    PIN_InitSymbols();
	  // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();
    
    // 初始化日志文件
    InitLog();

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(CountInst, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
