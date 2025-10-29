//
// Created by Bo Fang on 2016-07-24.
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
using namespace std;

VOID dostack(CONTEXT *ctxt, VOID *rtn_name){
    ofstream OutFile;
    OutFile.open("stackinfo",std::fstream::app);
    ADDRINT rbp = (ADDRINT)PIN_GetContextReg( ctxt, REG_STACK_PTR);
    OutFile << (const char*) rtn_name<<":"<< rbp << endl;
    OutFile.close();
}



VOID Routine(RTN rtn, VOID *v)
{
    RTN_Open(rtn);
    for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
    {
        // Insert a call to docount to increment the instruction counter for this rtn
        if (INS_IsStackRead(ins) || INS_IsStackWrite(ins)){
            string *temp = new string(RTN_Name(rtn));
            const char *rtn_name = temp->c_str();
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)dostack, IARG_CONTEXT,IARG_PTR,rtn_name, IARG_END);
            break;
        }


    }
    RTN_Close(rtn);
}

VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
    //ofstream OutFile;
    //OutFile.open(instcount_file.Value().c_str());
    //OutFile.setf(ios::showbase);
    //OutFile.close();
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
    RTN_AddInstrumentFunction(Routine, 0);
    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
