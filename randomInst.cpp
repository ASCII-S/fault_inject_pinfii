//
// Created by Bo Fang on 2016-05-18.
//

#include<iostream>
#include<fstream>

#include <set>
#include <map>
//#include <string>

#include "pin.H"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cstddef> // For size_t
#include "config_pintool.h"
using namespace std;

KNOB<UINT64> randInst(KNOB_MODE_WRITEONCE, "pintool",
                      "randinst","0", "random instructions");
KNOB<UINT64> FileNameSeq(KNOB_MODE_WRITEONCE, "pintool",
                      "FileNameSeq","0", "random instructions");

static UINT64 allinst = 0 ;
static UINT32 find_flag = 0;



#define Target_Opecode "MOV"
/*
static UINT64 start_dec_adr =   4198496;
static UINT64 end_dec_adr =     4314860;
*/

//mflag = 1;  表示ins是内存基址或内存索引寄存器
//mflag = -1; 表示ins是无效内存
//mflag = 0;  表示除了二者之外

VOID docount0() { allinst++; }
//ip是每次遇到的指令,regname是在指令ins中随机找到的寄存器名称,mflag表示寄存器的状态
VOID docount(VOID *ip, VOID *reg_name,UINT32 mflag,INS ins) {
    allinst++;
    
    /*if (opcodeStr == Target_Opecode) //调试预期的操作码
            {
                ADDRINT pc = INS_Address(ins);
                cout << "allinst:\t" <<  allinst << endl;
                cout << "Inj ope:\t" << opcodeStr.c_str() << "\tInj ins:\t" <<  INS_Disassemble(ins)<<endl;
                // 打印 PC 值
                std::cout << "PC of target opcode: " << std::hex << pc << std::dec << std::endl;
                }
    */
    //if (randInst.Value() = allinst && find_flag ==0) {   //遇到参数中给定的随机数时执行下文;在randInst之后开始找第一个期望的操作码
    //if (randInst.Value() == allinst) {   //遇到参数中给定的随机数时执行下文;在randInst之后开始找第一个期望的操作码
    if (randInst.Value() >= allinst && randInst.Value() <= allinst+0) {    
        //if (((unsigned long)ip > start_dec_adr) && ((unsigned long)ip < end_dec_adr))  //符合预期的ip值,不要尝试筛选出操作码!!!ip值和汇编代码中不匹配!!!
        //if (((unsigned long)ip > start_dec_adr) && ((unsigned long)ip < end_dec_adr))
            {
            cout << "randnum:\t"<< std::dec<< allinst <<"\t"
                 << "hexpc:\t" << std::hex << ip << std::dec
                 << endl;

            //调试信息
            if (0)
            {
            OPCODE opcode = INS_Opcode(ins);
            std::string opcodeStr = OPCODE_StringShort(opcode);
            std::ofstream logfile;
            logfile.open("pintool_mylog", std::ios_base::app);  // 追加模式打开文件
            if (logfile.is_open()) {
                logfile << "\n\nrandominst content:"<<"\n";
                logfile << "randInst: " << randInst.Value() << "\n";
                logfile << "actual Inst: " << allinst << "\n";
                
                logfile << "Inj ope:\t" << opcodeStr.c_str() << "\tInj ins:\t" <<  INS_Disassemble(ins)<<"\n";
                logfile << "Opecode:\t" << opcodeStr  << "\n";
                logfile << "Find hexpc:\t" << std::dec << ip << std::dec << "\n";
                
                logfile.close();
            } else {
                std::cerr << "Unable to open randomInst_log file." << std::endl;
            }
            }
            
            if(find_flag ==0) //保护不被重复写入
            {
            ofstream OutFile;
            // 使用 stringstream 拼接字符串
            string filename = "instruction";
            if (FileNameSeq.Value() != 0) {
                stringstream ss;
                ss << filename << FileNameSeq.Value();
                filename = ss.str();
            }
            OutFile.open(filename.c_str());
            if (mflag == 1){
                OutFile << "mem:"<<(const char *)reg_name << endl;
            }
            if (mflag == 0){
                OutFile << "reg:"<<(const char*)reg_name << endl;
            }
            if (static_cast<int>(mflag) == -1){
                OutFile << (const char*)reg_name << endl;
            }
            OutFile << "pc:"<<(unsigned long)ip << endl;
            OutFile.close();
            //cout << "done!" << endl;
            find_flag = 1;//找到合适的值了
            }
        }
        /*else{
            return;
            //exit(0);
    }*/

        

    }
}

// Pin calls this function every time a new instruction is encountered
VOID CountInst(INS ins, VOID *v)
{
    //allinst++;
    //cout << "Current is" << allinst << endl;


        /*
        std::cout << "\n\nFound JNE instruction: " << opcodeStr << std::endl;  // 如果前几项字符匹配
        // 使用 strncmp 比较 opcodeStr 的前 target_len 个字符
        cout << "Inj ope:\t" << opcodeStr.c_str() << "\tInj ins:\t" <<  INS_Disassemble(ins)<<endl;
        //cout << "Target ope:\t" << target_ope << endl;
        cout << "Now Inst:\t" << allinst << endl;
        */


//------------------------------------------------------对静态代码的操作,动态代码操作逻辑在回调函数中-------------------------------------------------------//
        //allinst++;

        
           
        {
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
                mflag = -1;
            }
            if (reg == REG_RFLAGS || reg == REG_FLAGS || reg == REG_EFLAGS) {
                string *temp = new string( "REGNOTVALID: inst " + INS_Disassemble(ins));
                reg_name = temp->c_str();
                mflag = -1;
                //OutFile << "REGNOTVALID: inst " + INS_Disassemble(ins) << endl;
            }

            string *temp = new string(REG_StringShort(reg));
            reg_name = temp->c_str();
            //OutFile << "reg:" + REG_StringShort(reg) << endl;
        }
        //if (INS_Valid(INS_Next(ins)))
        //    OutFile<<"next:"<<INS_Address(INS_Next(ins)) << endl;
        //OutFile.close();
        
        INS_InsertCall(ins,IPOINT_BEFORE,(AFUNPTR)docount,
                        IARG_INST_PTR,
                        IARG_PTR,reg_name,
                        IARG_UINT32,mflag,
                        IARG_ADDRINT, INS_Address(ins),
                        IARG_END);
        }
        /*
        else{
            //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)MyCallback, IARG_ADDRINT, INS_Address(ins),IARG_END);
            //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)PrintOpcode, IARG_INST_PTR, IARG_END);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount0, IARG_END);
        }*/

//------------------------------------------------------以上全局if,不是目标操作码就啥也不做--------------------------------------------------------//
    //cout<<"pc:"<<INS_Address(ins) << " " << allinst<< endl;
}

// bool mayChangeControlFlow(INS ins){
// 	REG reg;
// 	if(!INS_HasFallThrough(ins))
// 		return true;
// 	int numW = INS_MaxNumWRegs(ins);
// 	for(int i =0; i < numW; i++){
// 		if(reg == REG_RIP || reg == REG_EIP || reg == REG_IP) // conditional branches
// 			return true;
// 	}
// 	return false;
// }
// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
    //ofstream OutFile;
    //OutFile.open(instcount_file.Value().c_str());
    //OutFile.setf(ios::showbase);
    //OutFile.close();
    if (find_flag == 0){    //用来判断在当前随机数下,是否找到合适的指令,如果没有合适的指令,返回pc:0,请求一个新的随机数
        //cout << "find flag ==0" << endl;
        cout << "allInst:\t" << allinst << endl;
        ofstream OutFile;
        string filename = "instruction";
        if (FileNameSeq.Value() != 0) {
            stringstream ss;
            ss << filename << FileNameSeq.Value();
            filename = ss.str();
        }
        OutFile.open(filename.c_str());
        OutFile << "pc:0" << endl;
        OutFile.close();
    }
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}



int main(int argc, char * argv[])
{
    PIN_InitSymbols();
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(CountInst, 0);

    
    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    

    // Start the program, never returns
    PIN_StartProgram();


    return 0;
}
