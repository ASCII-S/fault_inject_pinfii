//
// Created by Bo Fang on 2016-02-24.
//

#include "pin.H"
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include "faultinjection.h"
#include <string.h>
#include <vector>

using namespace std;

KNOB<string> libnames(KNOB_MODE_WRITEONCE, "pintool",
                               "libload", "libnames", "to be loaded lib files");
KNOB<BOOL> is_inlib(KNOB_MODE_WRITEONCE, "pintool",
                      "isinlib","", "if we need to only look at instructions in a lib");

vector<string> libs;

UINT32 parseLibNames(string libfilename)
{
    string line;
    ifstream libfile(libfilename.c_str());
    if (!libfile)
    {
        cout << "Error opening lib files!" << endl;
        return -1;
    }
    while (getline(libfile,line))
    {
        libs.push_back(line);
    }
    return 0;
}


const char * stripPath(const char * path)
{
    const char * file = strrchr(path,'/');
    if (file)
        return file+1;
    else
        return path;
}

/*----
 *  Check library images based on user input, and instrument each instruction inside that routine
 */
