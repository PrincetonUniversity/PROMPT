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

static volatile bool PIN_ENABLED = false;

// Global variable to count memory related instructions
static UINT64 memInstReadCount = 0;
static UINT64 memInstWriteCount = 0;

static ADDRINT external_start_addr = 0;
static ADDRINT external_stop_addr = 0;

static VOID *ext_load_fcn_ptr; // void (*ext_load_fcn_ptr)(uint64_t);
static VOID *ext_store_fcn_ptr; //void (*ext_store_fcn_ptr)(uint64_t);

// This function is called before every instruction is executed
VOID MemReportRead(THREADID tid, const CONTEXT* context, ADDRINT memAddr) {
  if (!PIN_ENABLED) {
    return;
  }
  memInstReadCount++;
  // std::cerr << "MemReportRead "  << memAddr << std::endl;
  // call application function (SLAMP_ext_load_1(memAddr))

  CONTEXT writableContext, *ctxt;
  PIN_SaveContext(context, &writableContext);
  ctxt = &writableContext;
  ADDRINT curSp          = PIN_GetContextReg(ctxt, REG_RSP);
  // move curSp to 100 bytes below and make it 16-byte aligned
  curSp = (curSp - 100) & ~0xF;
  PIN_SetContextReg(ctxt, REG_RSP, curSp);

  PIN_ENABLED = 0;
  PIN_CallApplicationFunction(ctxt, tid, CALLINGSTD_DEFAULT, AFUNPTR(ext_load_fcn_ptr), NULL, PIN_PARG(void), PIN_PARG(uint64_t), memAddr, PIN_PARG_END());
  PIN_ENABLED = 1;
}

VOID MemReportWrite(THREADID tid, const CONTEXT* context, ADDRINT memAddr) {
  if (!PIN_ENABLED) {
    return;
  }
  memInstWriteCount++;
  // std::cerr << "MemReportWrite " << memAddr << std::endl;
  CONTEXT writableContext, *ctxt;
  PIN_SaveContext(context, &writableContext);
  ctxt = &writableContext;
  ADDRINT curSp          = PIN_GetContextReg(ctxt, REG_RSP);
  // move curSp to 100 bytes below and make it 16-byte aligned
  curSp = (curSp - 100) & ~0xF;
  PIN_SetContextReg(ctxt, REG_RSP, curSp);

  // call application function (SLAMP_ext_store_1(memAddr))
  PIN_ENABLED = 0;
  PIN_CallApplicationFunction(ctxt, tid, CALLINGSTD_DEFAULT, AFUNPTR(ext_store_fcn_ptr), NULL, PIN_PARG(void), PIN_PARG(uint64_t), memAddr, PIN_PARG_END());
  PIN_ENABLED = 1;
}

VOID ExternalStart()
{
  // std::cerr << "ExternalStart" << std::endl;
  // std::cerr << "ExternalStart" << std::endl;
  PIN_ENABLED = true;
}

VOID ExternalStop()
{
  // std::cerr << "ExternalStop" << std::endl;
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

    RTN ext_load_Rtn = RTN_FindByName(img, "SLAMP_ext_load_1");
    if (RTN_Valid(ext_load_Rtn))
    {
      ext_load_fcn_ptr = reinterpret_cast< VOID* >(RTN_Address(ext_load_Rtn));
      std::cerr << "Found the ext_load function at " << ext_load_fcn_ptr << std::endl;
    }

    RTN ext_store_Rtn = RTN_FindByName(img, "SLAMP_ext_store_1");
    if (RTN_Valid(ext_store_Rtn))
    {
      ext_store_fcn_ptr = reinterpret_cast< VOID* >(RTN_Address(ext_store_Rtn));

      std::cerr << "Found the ext_store function at " << ext_store_fcn_ptr << std::endl;
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
          IARG_THREAD_ID,
          IARG_CONTEXT,
          IARG_MEMORYREAD_EA,
          IARG_END);
    }
    if (INS_IsMemoryWrite(ins)){
      // std::cerr << "Instrumenting write: " << INS_Disassemble(ins) << std::endl;
      INS_InsertCall(
          ins, IPOINT_BEFORE, (AFUNPTR)MemReportWrite,
          IARG_THREAD_ID,
          IARG_CONTEXT,
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

