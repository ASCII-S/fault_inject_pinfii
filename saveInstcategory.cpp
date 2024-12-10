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

#include <unordered_map>

KNOB<string> ins_type(KNOB_MODE_WRITEONCE, "pintool",
    "ins_type", "none", "select instruction type so you can generate csv");
KNOB<string> only_memory(KNOB_MODE_WRITEONCE, "pintool",
    "only_memory", "1", "select instruction type only_memory");
KNOB<string> address_count_file(KNOB_MODE_WRITEONCE, "pintool",
    "address_count_file", "none", "save all instructions ");
KNOB<int> dynamic_analyze(KNOB_MODE_WRITEONCE, "pintool",
    "dynamic_analyze", "0", "enable==1,use dynamic analyze to generate all instructions of a ins_type instead of just catalog");
KNOB<string> inst_type_table_file(KNOB_MODE_WRITEONCE, "pintool",
    "inst_type_table_file", "none", "output file to store instruction and categories");


std::map<std::string, int> iteration_map; // 定义每个类型的合法值
std::unordered_map<std::string, std::set<std::string>> type_conditions;

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
inline void SaveInstructionLogByMflag(const std::string& mnemonic_count_key, const std::string& cate, const std::string& mnemonic, const std::string& addressStr,
                                       const std::string& insDis,const std::string& args_regmm,const std::string& args_reg) {

    if(address_count_file.Value() != "none"){
      iteration_map[mnemonic_count_key]++;
    }

    if (inst_type_table_file.Value() != "none"){
      SaveMsgLog(args_regmm + "," + args_reg + "," + addressStr + "," + mnemonic + "," + insDis);
    }
}

// 内联函数：检查是否为 "mtype" 类型指令且是内存读写操作
inline bool is_ins_type_memory_access(INS ins, const std::string& mnemonic, 
                                       const std::unordered_map<std::string, std::set<std::string>>& type_conditions, 
                                       const std::string& ins_type_value) {
    // 检查指定的指令类型是否在合法指令集合中
    auto it = type_conditions.find(ins_type_value);
    if (it != type_conditions.end()) {
        const auto& valid_mnemonics = it->second;  // 获取指定类型的合法助记符集合

        // 检查该类型的指令是否在合法的指令集合中
        if (valid_mnemonics.find(mnemonic) != valid_mnemonics.end()) {
            // 如果是内存读写操作，则返回 true
            return INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins);
        }
    }
    return false;  // 不是合法的内存读写操作
}
void initialize_instruction_types() {
  
    // 使用 insert 方法初始化 std::set
    type_conditions["stack"].insert("POP");
    type_conditions["stack"].insert("PUSH");
    // 数据传送指令（Data Transfer Instructions）
    type_conditions["data_transfer"].insert("LEA");

    // mov指令
    type_conditions["mov"].insert("MOV");
    type_conditions["mov"].insert("MOVAPD");
    type_conditions["mov"].insert("MOVAPS");
    type_conditions["mov"].insert("MOVD");
    type_conditions["mov"].insert("MOVQ");
    type_conditions["mov"].insert("MOVSD_XMM");
    type_conditions["mov"].insert("MOVSS");
    type_conditions["mov"].insert("MOVSX");
    type_conditions["mov"].insert("MOVSXD");
    type_conditions["mov"].insert("MOVUPD");
    type_conditions["mov"].insert("MOVZX");


    // 算术运算指令（Arithmetic Operation Instructions） - 整数运算（Integer Operations）
    type_conditions["integer"].insert("ADD");
    type_conditions["integer"].insert("SUB");
    type_conditions["integer"].insert("IDIV");
    type_conditions["integer"].insert("IMUL");
    //算术右移和带为减法
    type_conditions["integer"].insert("SAR");
    type_conditions["integer"].insert("SBB");
    //整数位移运算
    type_conditions["integer"].insert("SHL");
    type_conditions["integer"].insert("SHR");


    // 算术运算指令（Arithmetic Operation Instructions） - 浮点数运算（Floating Point Operations）c
    type_conditions["float"].insert("ADDPD");
    type_conditions["float"].insert("ADDSD");
    type_conditions["float"].insert("ADDSS");
    type_conditions["float"].insert("CVTSD2SS");
    type_conditions["float"].insert("CVTSI2SD");
    type_conditions["float"].insert("CVTSI2SS");
    type_conditions["float"].insert("CVTSS2SD");
    type_conditions["float"].insert("CVTTSD2SI");
    type_conditions["float"].insert("DIVSD");
    type_conditions["float"].insert("DIVSS");
    type_conditions["float"].insert("MULPD");
    type_conditions["float"].insert("MULSD");
    type_conditions["float"].insert("MULSS");
    type_conditions["float"].insert("SQRTSD");
    type_conditions["float"].insert("SUBSD");
    type_conditions["float"].insert("SUBSS");

    // 逻辑运算指令（Logical Operation Instructions）
    type_conditions["logical"].insert("AND");
    type_conditions["logical"].insert("ANDNPD");
    type_conditions["logical"].insert("ANDPD");
    type_conditions["logical"].insert("ANDPS");
    type_conditions["logical"].insert("SAR");
    type_conditions["logical"].insert("SBB");
    type_conditions["logical"].insert("SHR");
    type_conditions["logical"].insert("NEG");
    type_conditions["logical"].insert("NOT");
    type_conditions["logical"].insert("OR");
    type_conditions["logical"].insert("ORPD");
    type_conditions["logical"].insert("PXOR");
    type_conditions["logical"].insert("XOR");
    type_conditions["logical"].insert("XORPD");
    type_conditions["logical"].insert("CDQE");

    //程序控制指令 (Program Control Instructions)
    type_conditions["call_ret"].insert("CALL_NEAR");
    type_conditions["call_ret"].insert("RET_NEAR");

    type_conditions["control_flow"].insert("LEAVE");
    type_conditions["control_flow"].insert("JB");
    type_conditions["control_flow"].insert("JBE");
    type_conditions["control_flow"].insert("JL");
    type_conditions["control_flow"].insert("JLE");
    type_conditions["control_flow"].insert("JMP");
    type_conditions["control_flow"].insert("JNB");
    type_conditions["control_flow"].insert("JNBE");
    type_conditions["control_flow"].insert("JNL");
    type_conditions["control_flow"].insert("JNLE");
    type_conditions["control_flow"].insert("JNS");
    type_conditions["control_flow"].insert("JNZ");
    type_conditions["control_flow"].insert("JP");
    type_conditions["control_flow"].insert("JS");
    type_conditions["control_flow"].insert("JZ");

    type_conditions["control_flow"].insert("SETB");
    type_conditions["control_flow"].insert("SETBE");
    type_conditions["control_flow"].insert("SETL");
    type_conditions["control_flow"].insert("SETLE");
    type_conditions["control_flow"].insert("SETNB");
    type_conditions["control_flow"].insert("SETNBE");
    type_conditions["control_flow"].insert("SETNL");
    type_conditions["control_flow"].insert("SETNLE");
    type_conditions["control_flow"].insert("SETNP");
    type_conditions["control_flow"].insert("SETNZ");
    type_conditions["control_flow"].insert("SETZ");
    
    type_conditions["control_flow"].insert("CMOVNZ");
    type_conditions["control_flow"].insert("CMOVNLE");
    type_conditions["control_flow"].insert("CMOVS");
    type_conditions["control_flow"].insert("CMOVZ");
    type_conditions["control_flow"].insert("CMP");
    type_conditions["control_flow"].insert("CMPSD_XMM");
    type_conditions["control_flow"].insert("CQO");

    
    type_conditions["other"].insert("CDQ");
    type_conditions["other"].insert("CDQE");
    type_conditions["other"].insert("REPE_CMPSB");
    type_conditions["other"].insert("REPNE_SCASB");
    type_conditions["other"].insert("UCOMISD");
    type_conditions["other"].insert("UCOMISS");
    type_conditions["other"].insert("UNPCKLPD");

}
bool check_instruction_type(INS ins) {
    // 获取当前指令的助记符
    std::string mnemonic = INS_Mnemonic(ins);

    std::string ins_type_value = ins_type.Value();

    // 检查 "integer" 类型的指令
    if (ins_type_value == "integer") {
        if (type_conditions["integer"].find(mnemonic) != type_conditions["integer"].end()) {
            if (only_memory.Value() != "1"){//不考虑是否内存读写
              return true;
            }
            // 检查是否为内存读或写操作
            if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
                return true;
            }
        }
    }

    // 检查 "float" 类型的指令
    if (ins_type_value == "float") {
        if (type_conditions["float"].find(mnemonic) != type_conditions["float"].end()) {
            if (only_memory.Value() != "1"){
              return true;
            }
            // 检查是否为内存读或写操作
            if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
                return true;
            }
        }
    }

    // 检查 "mov" 类型的指令
    if (ins_type_value == "mov") {
        if (type_conditions["mov"].find(mnemonic) != type_conditions["mov"].end()) {
            if (only_memory.Value() != "1"){
              return true;
            }
            // 检查是否为内存读或写操作
            if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
                return true;
            }
        }
    }

    // 检查其他类型
    if (ins_type_value == "stack" || ins_type_value == "cmp" ||ins_type_value == "div" ) {
      if (type_conditions.find(ins_type_value) != type_conditions.end()) {
          const std::set<std::string>& valid_mnemonics = type_conditions[ins_type_value];
        
          // 对指定类型检查其合法的 mnemonics
          return valid_mnemonics.find(mnemonic) != valid_mnemonics.end();
      }
    }

    if (ins_type_value == "all" ) {
      return true;
    }
    return false;
}


VOID INS_mnemonic_counter(INS ins, const std::string& mnemonic_count_key){
    if(address_count_file.Value() != "none"){
      iteration_map[mnemonic_count_key]++;
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
      //************************************************************************/
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
      //************************************************************************/

  string cate = CATEGORY_StringShort(INS_Category(ins));
  string mnemonic = INS_Mnemonic(ins);
  ADDRINT address = INS_Address(ins);
  string insDis = INS_Disassemble(ins);

  std::stringstream ss;
  ss << std::hex << address;  // 将地址以十六进制输出
  string addressStr = ss.str();  // 获取转换后的字符串

  
  std::string  mnemonic_count_key = args_regmm + "," + args_reg + "," + addressStr + "," + mnemonic;
  
  //StdMsgLog(mnemonic_count_key);
  bool processInstruction = false;
  if (check_instruction_type(ins)){
    processInstruction = true;
  }
  int da = dynamic_analyze.Value();
  if (processInstruction){
    if (da == 0)
      SaveInstructionLogByMflag(mnemonic_count_key, cate, mnemonic, addressStr, insDis, args_regmm, args_reg);
    else if (da == 1)
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)INS_mnemonic_counter,
                    IARG_PTR, ins,  // 传递指令对象 ins
                    IARG_PTR, new std::string(mnemonic_count_key),
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

void PrintTypeConditionsSorted() {
    // 将键提取到一个 vector 中
    std::vector<std::string> keys;
    for (const auto& pair : type_conditions) {
        keys.push_back(pair.first);
    }
    
    // 按字典序排序键
    std::sort(keys.begin(), keys.end());
    
    // 遍历排序后的键
    for (const auto& key : keys) {
        std::cout << "Category: " << key << std::endl;
        std::cout << "Instructions: ";
        for (const auto& instruction : type_conditions[key]) {
            std::cout << instruction << " ";
        }
        std::cout << std::endl;
    }
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
  //inst_type_table_file
  if(address_count_file.Value() != "none")
    {SaveSortedMapToFile(iteration_map,address_count_file);}
  //PrintTypeConditionsSorted();
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
    initialize_instruction_types();
    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(CountInst, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
