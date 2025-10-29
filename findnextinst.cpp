//
// Created by Bo Fang on 2016-07-06.
//

#include<iostream>
#include<fstream>
#include <sstream>

#include <set>
#include <map>
#include <string>

#include "pin.H"
#include "utils.h"
#include <stdlib.h>
using namespace std;

KNOB<string> pc(KNOB_MODE_WRITEONCE, "pintool",
                "pc","pc", "file name that stores the pc for injection");




//VOID printip(void *ip){
//    if (static_cast<std::string>(ip) == pc.Value())
//       interations ++;
//}
// Pin calls this function every time a new instruction is encountered
VOID CountInst(INS ins, VOID *v)
{
    stringstream ss;
    ss << INS_Address(ins);
    //printf("Next instruction address: %p\n", (void*)INS_NextAddress(ins));

    if (pc.Value() == ss.str()){
        //printf("Next instruction address: %p\n", (void*)INS_NextAddress(ins));

        ofstream OutFile;
        OutFile.open("nextpc");
        OutFile << "thispc:"<< INS_Disassemble(ins) << endl;
        OutFile << "nextpc:"<< INS_NextAddress(ins) << endl;
        int numW = INS_MaxNumWRegs(ins);
        for(int i = 0; i < numW; ++i){
            REG write_reg = INS_RegW(ins,i);
            if (REG_valid(write_reg))
                OutFile << "regw"<< i <<":"<< REG_StringShort(write_reg) << endl;
           
        } 
        if (INS_IsMemoryRead(ins))
            OutFile << "memory-load"<< endl;
        if (INS_IsMemoryWrite(ins))
            OutFile << "memory-write"<<  endl;

        if (INS_IsStackRead(ins)){
            if (REG_valid(INS_MemoryBaseReg(ins))) {
                if (INS_MemoryOperandIsRead(ins, 0))
                    OutFile << "stackr:" << REG_StringShort(INS_MemoryBaseReg(ins)) << endl;
                if (INS_MemoryOperandIsWritten(ins, 0))
                    OutFile << "stackw:" << REG_StringShort(INS_MemoryBaseReg(ins)) << endl;
                // write base, index, scale and displacement
                OutFile << "base:" << REG_StringShort(INS_MemoryBaseReg(ins)) << endl;
                if(REG_valid(INS_MemoryIndexReg(ins)))
                    OutFile << "index:" << REG_StringShort(INS_MemoryIndexReg(ins)) << endl;
                else
                    OutFile << "index:" << "null" << endl;
                OutFile << "displacement:"<<INS_MemoryDisplacement(ins) << endl;
                OutFile << "scale:"<<INS_MemoryScale(ins) << endl;
            }

        }
        else if (INS_IsStackWrite(ins)){
            if (REG_valid(INS_MemoryBaseReg(ins))) {
                if (INS_MemoryOperandIsRead(ins, 0))
                    OutFile << "stackr:" << REG_StringShort(INS_MemoryBaseReg(ins)) << endl;
                if (INS_MemoryOperandIsWritten(ins, 0))
                    OutFile << "stackw:" << REG_StringShort(INS_MemoryBaseReg(ins)) << endl;
                OutFile << "base:" << REG_StringShort(INS_MemoryBaseReg(ins)) << endl;
                if(REG_valid(INS_MemoryIndexReg(ins)))
                    OutFile << "index:" << REG_StringShort(INS_MemoryIndexReg(ins)) << endl;
                else
                    OutFile << "index:" << "null" << endl;
                OutFile << "displacement:"<<INS_MemoryDisplacement(ins) << endl;
                OutFile << "scale:"<<INS_MemoryScale(ins) << endl;
            }
        }
        else{
            OutFile << "nostack" << endl;
        }
        OutFile.close();
        
        ofstream spsize;
        spsize.open("spsize");
        RTN rtn = INS_Rtn(ins);
        if (RTN_Valid(rtn))
        {
            RTN_Open(rtn);
            bool meet_rbp = false;
            for( INS ins1= RTN_InsHead(rtn); INS_Valid(ins1); ins1 = INS_Next(ins1) )
            {
                // 检查是否是 push 指令
                if (OPCODE_StringShort(INS_Opcode(ins1)) == "PUSH")
                {

                    // 如果遇到过push %rbp，则记录以后的push
                    if (meet_rbp)
                    {
                        spsize << INS_Disassemble(ins1) << std::endl;
                    }
                    
                    if (INS_MaxNumRRegs(ins1) > 0)
                    {
                        REG reg = INS_RegR(ins1, 0); // 获取第一个读寄存器
                        if (REG_StringShort(reg) == "rbp")  // 如果是 push %rbp
                        {
                            meet_rbp = true;
                        }
                    }

                }

                if (OPCODE_StringShort(INS_Opcode(ins1)) == "SUB")
                {
                    int numW = INS_MaxNumWRegs(ins1);
                    for (int i = 0; i < numW; i++)
                    {
                        REG reg = INS_RegW(ins1, i);
                        if (REG_StringShort(reg) == "rsp")
                        {
                            spsize << INS_Disassemble(ins1) ;
                        }
                    }
                    break;
                }
            }
            spsize.close();
            RTN_Close(rtn);
        }
    }

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
    //OutFile.open("iteration");
    //OutFile.setf(ios::showbase);
    //OutFile << iterations << endl;
    //OutFile.close();
    //cout << iterations << endl;
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
