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
using std::cout;
using std::ofstream;
using std::ios;
using std::string;
using std::endl;
#include<unordered_map>
// 用于统计不同 指令类型 的数量
std::map<std::string, int> mnemonic_count;
// 创建两个 map，用于分别统计写寄存器和读寄存器的次数
std::map<std::string, int> write_register_count;
std::map<std::string, int> read_register_count;

KNOB<string> inst_type_table_file(KNOB_MODE_WRITEONCE, "pintool",
    "inst_type_table_file", "none", "output file to store instruction and categories");
KNOB<string> mnemonic_count_file(KNOB_MODE_WRITEONCE, "pintool",
    "mnemonic_count_file", "none", "select instruction type so you can generate csv");
KNOB<int> dynamic_analyze(KNOB_MODE_WRITEONCE, "pintool",
    "dynamic_analyze", "0", "enable==1,use dynamic analyze to generate all instructions of a ins_type instead of just catalog");
KNOB<string> des_register_count_file(KNOB_MODE_WRITEONCE, "pintool",
    "des_register_count_file", "none", " generate des_register_count_file");
KNOB<string> src_register_count_file(KNOB_MODE_WRITEONCE, "pintool",
    "src_register_count_file", "none", " generate src_register_count_file");

std::ofstream logFile;  // 用于存储日志的文件流

// 初始化日志文件
void InitLog() {
    string logFilePath = inst_type_table_file.Value();  // 获取指定的输出文件路径
    //std::cout << logFilePath <<endl;
    std::remove(logFilePath.c_str());
    logFile.open(logFilePath.c_str(), std::ios::out | std::ios::app);  // 打开文件以进行追加
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file: " << logFilePath << std::endl;
    }
}

// 自定义 MYLOG 宏，将日志输出到文件
#define SaveMsgLog(msg)   if (logFile.is_open()) { logFile << msg << std::endl; }  else { std::cerr << "Log file is not open!" << std::endl; }

//save one ins as format
void SaveInstructionLogByMflag(const std::string& cate, const std::string& mnemonic, const std::string& addressStr,
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
    mnemonic_count[mnemonic]++;
    if (inst_type_table_file.Value() != "none"){
      SaveMsgLog(args_regmm + "," + args_reg + "," + addressStr + "," + mnemonic + ","  + insDis);
    }
}

// 遍历指令的所有操作数并统计寄存器出现次数
void CountRegisters(INS ins) {
    for (UINT32 i = 0; i < INS_OperandCount(ins); i++) {
        if (INS_OperandIsReg(ins, i)) {  // 检查是否是寄存器
            REG reg = INS_OperandReg(ins, i);       // 获取寄存器
            std::string reg_name = REG_StringShort(reg);  // 获取寄存器名称

            // （分析源寄存器）
            if (INS_OperandRead(ins, i) && (src_register_count_file.Value() != "none")) {
                write_register_count[reg_name]++;
            }

            // （分析目寄存器）
            if (!INS_OperandRead(ins, i) && (des_register_count_file.Value() != "none")) {
                read_register_count[reg_name]++;
            }
        }
    }
}

// 回调函数 docount，用于处理指令信息
VOID docount(INS ins, ADDRINT ip, string cate, string mnemonic, string addressStr, string insDis, VOID *reg_name, UINT32 mflag) {
    if (!((src_register_count_file.Value() == "none") && (des_register_count_file.Value() == "none"))) {
      CountRegisters(ins);
    }
  if (mnemonic_count_file.Value() != "none"){
        SaveInstructionLogByMflag(cate, mnemonic, addressStr, insDis, reg_name, mflag);
  }
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
  
  bool processInstruction = true;
  int da = dynamic_analyze.Value();
  if (processInstruction){
    if (da == 0){
      if (!((src_register_count_file.Value() == "none") && (des_register_count_file.Value() == "none"))) {
        CountRegisters(ins);
      }
      if (mnemonic_count_file.Value() != "none"){
        SaveInstructionLogByMflag(cate, mnemonic, addressStr, insDis, reg_name, mflag);
      }
    }
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

void SaveSortedMapToFile(const std::map<std::string, int>& input_map, const std::string& output_filename) {
    // 将 map 转换为 vector
    std::vector<std::pair<std::string, int>> sorted_map(input_map.begin(), input_map.end());

    // 按键升序排序
    std::sort(sorted_map.begin(), sorted_map.end(),
              [](const std::pair<std::string, int>& a, const std::pair<std::string, int>& b) {
                  return a.second > b.second;  // 按键升序排序
              });

    // 打开输出文件
    std::ofstream OutFile;
    OutFile.open(output_filename.c_str());

    if (!OutFile.is_open()) {
        std::cerr << "Error opening output file!" << std::endl;
        return;
    }

    // 设置输出格式
    OutFile.setf(std::ios::showbase);

    // 将排序后的数据写入文件
    for (const auto& entry : sorted_map) {
        OutFile << entry.first << "," << entry.second << std::endl;
    }

    // 关闭文件
    OutFile.close();
    std::cout << "Data saved to " << output_filename << std::endl;
}


// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{   
    if (mnemonic_count_file.Value() != "none")
      SaveSortedMapToFile(mnemonic_count, mnemonic_count_file);
    if (des_register_count_file.Value() != "none")
      SaveSortedMapToFile(write_register_count, des_register_count_file);
    if (src_register_count_file.Value() != "none")
      SaveSortedMapToFile(read_register_count, src_register_count_file);
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
    if (inst_type_table_file.Value() != "none"){
      InitLog();
    }
    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(CountInst, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
