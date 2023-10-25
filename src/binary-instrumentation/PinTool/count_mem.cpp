#include <iostream>
#include <fstream>
#include "pin.H"
#include "unistd.h"
#include <set>
using std::cerr;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;
using std::set;

ofstream OutFile;
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "memcount.out", "specify output file name");


std::set<std::string> all_external_fcns;

static bool PIN_ENABLED = false;

// Global variable to count memory related instructions
static UINT64 memInstReadCount = 0;
static UINT64 memInstWriteCount = 0;

static ADDRINT external_start_addr = 0;
static ADDRINT external_stop_addr = 0;

// This function is called before every instruction is executed
VOID MemReportRead(ADDRINT memAddr) {
  memInstReadCount++;
}

VOID MemReportWrite(ADDRINT memAddr) {
  memInstWriteCount++;
}

VOID ExternalStart()
{
  // std::cerr << "ExternalStart" << std::endl;
  PIN_ENABLED = true;
}

VOID ExternalStop()
{
  // std::cerr << "ExternalStop" << std::endl;
  PIN_ENABLED = false;
}

VOID RtnCallPrint(CHAR * rtnName)
{
  if (PIN_ENABLED) {
    all_external_fcns.insert(rtnName);
  }
}


VOID Image(IMG img, VOID* v)
{
    // Instrument the malloc() and free() functions.  Print the input argument
    // of each malloc() or free(), and the return value of malloc().
    //
    //  Find the malloc() function.
    RTN external_start_Rtn = RTN_FindByName(img, "SLAMP_ext_push");
    if (RTN_Valid(external_start_Rtn))
    {
      std::cerr << "Found the ExternalStart function at " << RTN_Address(external_start_Rtn) << std::endl;
      external_start_addr = RTN_Address(external_start_Rtn);
    }

    RTN external_stop_Rtn = RTN_FindByName(img, "SLAMP_ext_pop");
    if (RTN_Valid(external_stop_Rtn))
    {
      std::cerr << "Found the ExternalStop function at " << RTN_Address(external_stop_Rtn) << std::endl;
      external_stop_addr = RTN_Address(external_stop_Rtn);
    }
}

VOID Routine(RTN rtn, VOID *v)
{

  RTN_Open(rtn);

  // Insert a call at the entry point of a routine to increment the call count
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)RtnCallPrint, IARG_ADDRINT, RTN_Name(rtn).c_str(), IARG_END);

  RTN_Close(rtn);
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
  // get current addr
  // ADDRINT addr = INS_Address(ins);
  // if (external_start_addr != 0 && addr == external_start_addr)
  // {
    // std::cerr << "Entering external_start" << std::endl;
    // INS_InsertCall(
        // ins, IPOINT_BEFORE, (AFUNPTR)ExternalStart,
        // IARG_END);
  // }

  // if (external_stop_addr != 0 && addr == external_stop_addr)
  // {
    // std::cerr << "Entering external_stop" << std::endl;
    // INS_InsertCall(
        // ins, IPOINT_BEFORE, (AFUNPTR)ExternalStop,
        // IARG_END);
  // }

  if (INS_IsDirectControlFlow(ins) && INS_IsCall(ins))
 {
   ADDRINT targetAddr = INS_DirectControlFlowTargetAddress(ins);

   // RTN targetRtn = RTN_FindByAddress(targetAddr);

   //std::cerr << "Found a call instruction to " << targetAddr << " " << RTN_Name(targetRtn) << std::endl;
   // Check if the RTN is valid and matches the "external_start" function name.
   // if (RTN_Valid(targetRtn) && RTN_Name(targetRtn) == "external_start")
   if (external_start_addr != 0 && targetAddr == external_start_addr)
   {
     std::cerr << "Found a start " << targetAddr << std::endl;
     INS_InsertCall(
         // FIXME: IPOINT_AFTER doesn't work for call
         ins, IPOINT_BEFORE, (AFUNPTR)ExternalStart,
         IARG_END);
   }

   // Check if the RTN is valid and matches the "external_start" function name.
   if (external_stop_addr != 0 && targetAddr == external_stop_addr)
   {
     std::cerr << "Found a stop " << targetAddr << std::endl;
     INS_InsertCall(
         ins, IPOINT_BEFORE, (AFUNPTR)ExternalStop,
         IARG_END);
   }
 }
  if (PIN_ENABLED)
  {
    if (INS_IsMemoryRead(ins)){
      // std::cerr << "Instrumenting read: " << INS_Disassemble(ins) << std::endl;
      INS_InsertCall(
          ins, IPOINT_BEFORE, (AFUNPTR)MemReportRead,
          IARG_MEMORYREAD_EA,
          IARG_END);
    }
    if (INS_IsMemoryWrite(ins)){
      // std::cerr << "Instrumenting write: " << INS_Disassemble(ins) << std::endl;
      INS_InsertCall(
          ins, IPOINT_BEFORE, (AFUNPTR)MemReportWrite,
          IARG_MEMORYWRITE_EA,
          IARG_END);
    }
  }
}

VOID Fini(INT32 code, VOID* v)
{
    // Write to a file since cout and cerr maybe closed by the application
    OutFile.setf(ios::showbase);
    OutFile << "Read Count " << memInstReadCount << endl;
    OutFile << "Write Count " << memInstWriteCount << endl;

    for (std::set<string>::iterator it = all_external_fcns.begin() ; it != all_external_fcns.end(); ++it)
    {
      OutFile << *it << endl;
    }

    OutFile.close();
}

INT32 main(INT32 argc, CHAR **argv) {
    // Initialize Pin
    PIN_InitSymbols();
    PIN_Init(argc, argv);
    OutFile.open(KnobOutputFile.Value().c_str());

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);
    IMG_AddInstrumentFunction(Image, 0);
    RTN_AddInstrumentFunction(Routine, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program
    PIN_StartProgram();

    return 0;
}

