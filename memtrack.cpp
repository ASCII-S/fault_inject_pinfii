//
// Created by pwu on 1/26/16.
//


#include <string.h>
#include "memtrack.h"
using namespace std;

VOID *WriteAddr;
map<VOID*,UINT64> MemStore;
UINT64 NMemWriteInstStatic;

VOID RecordMemWriteBefore(ADDRINT ip, VOID *addr) {
    //cout << ip << ":" << addr << ":" << *(UINT32*)addr << endl;
    WriteAddr = addr;
}
VOID RecordMemWriteAfter(ADDRINT ip, VOID *addr, UINT32 size) {
    UINT64 value=0;
    PIN_SafeCopy(&value, WriteAddr, size);
    //memcpy(&value,WriteAddr,size);
    //cout << hex << ip << ":" << WriteAddr << ":"  << size << "\t" << value << endl;
    MemStore[WriteAddr] = value;
}

VOID memtrack(INS ins, VOID *) {
    if (!INS_HasFallThrough(ins)) return;

    UINT32 memop = INS_MemoryOperandCount(ins);

    for (UINT32 i=0; i<memop; i++) {
        if (INS_MemoryOperandIsWritten(ins, i)) {
            //cout << "inserting..." << endl;
            INS_InsertPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR) RecordMemWriteBefore,
                    IARG_INST_PTR, IARG_MEMORYWRITE_EA,
                     IARG_END);
            INS_InsertPredicatedCall(ins, IPOINT_AFTER, (AFUNPTR) RecordMemWriteAfter,
                                     IARG_INST_PTR, IARG_PTR, WriteAddr, IARG_MEMORYWRITE_SIZE,
                                     IARG_END);
        }
    }
}

VOID memtrack2(INS ins, VOID *) {

}
