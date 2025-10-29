//
// Created by Bo Fang on 2016-05-27.
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

KNOB<UINT64> randint(KNOB_MODE_WRITEONCE, "pintool",
                     "randinst","0", "random instruction");

static UINT64 iterations = 0;

static UINT64 randominst = 0;

VOID countIteration(VOID *ip){
    if (randominst < randint.Value()){
        iterations++;
    }
}

VOID docount(){randominst++;}

//VOID printip(void *ip){
//    if (static_cast<std::string>(ip) == pc.Value())
 //       interations ++;
//}
// Pin calls this function every time a new instruction is encountered
VOID CountInst(INS ins, VOID *v)
{
    stringstream ss;
    ss << INS_Address(ins);
    if (pc.Value() == ss.str()){
        //cout << pc.Value() << " " << ss.str() << endl;
        INS_InsertCall(ins,IPOINT_BEFORE,(AFUNPTR)countIteration,IARG_INST_PTR,IARG_END);
    }

    INS_InsertCall(ins,IPOINT_BEFORE,(AFUNPTR)docount,IARG_END);
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
    ofstream OutFile;
    OutFile.open("iteration");
    OutFile.setf(ios::showbase);
    OutFile << iterations << endl;
    OutFile.close();

    std::ofstream logfile;
    logfile.open("pintool_mylog", std::ios_base::app);  // 追加模式打开文件
    if (logfile.is_open()) {
        logfile << "determineInst content:\t" <<"\n";
        logfile << "iteration:\t" << iterations <<"\n";
        
        logfile.close();
    } else {
        std::cerr << "Unable to open randomInst_log file." << std::endl;
    }
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
