#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include "pin.H"

using std::cerr;
using std::endl;
using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;

// Global variables
static UINT64 jump_count = 0;
static vector<UINT64> random_indices;
static vector<UINT64>::iterator current_index;
static UINT64 icount = 0;
KNOB<string> randnumsfile(KNOB_MODE_WRITEONCE, "pintool", "randnumsfile", "none", "Input file containing random indices");
KNOB<UINT64> total_count(KNOB_MODE_WRITEONCE, "pintool", "totalcount", "0", "Total instruction count");

// Function to check if an instruction is a jump type
bool IsJumpInstruction(INS ins)
{
    return INS_IsBranch(ins) || INS_IsCall(ins) || INS_IsRet(ins);
}

// Function to increment dynamic instruction count and check for jumps
VOID docount(INS ins)
{
    icount++;

    // Check if the current instruction count matches the current index
    if (current_index != random_indices.end() && icount == *current_index)
    {
        // If the instruction is a jump type, increment the jump count
        //if (IsJumpInstruction(ins))
        if(INS_IsBranch(ins) || !INS_HasFallThrough(ins))
        {
            jump_count++;
        }
        // Move to the next index
        ++current_index;
    }
    //std::cout << icount << "and\t" << *current_index << std::endl;
}

// Instrumentation function
VOID Instruction(INS ins, VOID* v)
{
    // Insert a call to docount before every instruction
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, ins, IARG_END);
}

// Function to parse the random numbers file
void ParseRandomNumbersFile(const std::string& filename)
{
    std::ifstream infile(filename.c_str());
    if (!infile.is_open())
    {
        cerr << "Error: Unable to open file " << filename << endl;
        PIN_ExitProcess(1);
    }

    string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        UINT64 value;
        if (iss >> value)
        {
            random_indices.push_back(value);
        }
    }
    infile.close();

    // Sort the random indices in ascending order
    std::sort(random_indices.begin(), random_indices.end());
    current_index = random_indices.begin();
}

// Function called when the application exits
VOID Fini(INT32 code, VOID* v)
{
    // Write the jump count to jmpcount_output.txt
    std::ofstream outfile("jmpcount_output.txt", std::ios::app);
    if (!outfile.is_open())
    {
        cerr << "Error: Unable to write to file jmpcount_output.txt" << endl;
        return;
    }

    outfile << "Jump Count: " << jump_count << endl;
    outfile.close();

    cerr << "Jump instructions counted: " << jump_count << endl;
}

// Main function
int main(int argc, char* argv[])
{
    // Initialize Pin
    if (PIN_Init(argc, argv))
    {
        cerr << KNOB_BASE::StringKnobSummary() << endl;
        return 1;
    }

    // Validate required arguments
    if (randnumsfile.Value() == "none" || total_count.Value() == 0)
    {
        cerr << "Error: Missing required arguments." << endl;
        return 1;
    }

    // Parse the random numbers file
    ParseRandomNumbersFile(randnumsfile.Value());

    // Add instrumentation for each instruction
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini function
    PIN_AddFiniFunction(Fini, 0);

    // Start the program
    PIN_StartProgram();

    return 0;
}
