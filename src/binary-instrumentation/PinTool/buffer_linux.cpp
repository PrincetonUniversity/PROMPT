/*
 * Copyright (C) 2009-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*
 * Sample buffering tool
 *
 * This tool collects an address trace of instructions that access memory
 * by filling a buffer.  When the buffer overflows,the callback writes all
 * of the collected records to a file.
 *
 */

#include "pin.H"
#include "types_vmapi.PH"
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>
using std::cerr;
using std::endl;
using std::hex;
using std::ofstream;
using std::string;

/*
 * Name of the output file
 */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "buffer.out",
                            "output file");

/*
 * The ID of the buffer
 */
BUFFER_ID bufId;

/*
 * Thread specific data
 */
TLS_KEY mlog_key;

/*
 * Number of OS pages for the buffer
 */
#define NUM_BUF_PAGES 1024

/*
 * Record of memory references.  Rather than having two separate
 * buffers for reads and writes, we just use one struct that includes a
 * flag for type.
 */
struct MEMREF {
  ADDRINT pc;
  ADDRINT ea;
  UINT32 size;
  BOOL read;
};

/*
 * MLOG - thread specific data that is not handled by the buffering API.
 */
class MLOG {
public:
  MLOG(THREADID tid);
  ~MLOG();

  VOID DumpBufferToFile(struct MEMREF *reference, UINT64 numElements,
                        THREADID tid);

private:
  ofstream _ofile;
};

MLOG::MLOG(THREADID tid) {
  const string filename =
      KnobOutputFile.Value() + "." + decstr(getpid()) + "." + decstr(tid);

  _ofile.open(filename.c_str());

  if (!_ofile) {
    cerr << "Error: could not open output file." << endl;
    exit(1);
  }

  _ofile << hex;
}

MLOG::~MLOG() { _ofile.close(); }

VOID MLOG::DumpBufferToFile(struct MEMREF *reference, UINT64 numElements,
                            THREADID tid) {
  for (UINT64 i = 0; i < numElements; i++, reference++) {
    if (reference->ea != 0)
      _ofile << reference->pc << "   " << reference->ea << endl;
  }
}

bool PIN_ENABLED = false;
VOID *ext_push_funptr;
VOID *ext_pop_funptr;
VOID *event_conversion_funptr;

// Replace SLAMP_ext_push with ext_push_wrapper
//   - first call SLAMP_ext_push
//   - Then set profiling to On
VOID ExternalStartWrapper(CONTEXT *ctxt, const uint32_t id) {
  PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
                              AFUNPTR(ext_push_funptr), NULL, PIN_PARG(void),
                              PIN_PARG(uint32_t), id, PIN_PARG_END());
  PIN_ENABLED = true;
}

// Replace SLAMP_ext_pop with ext_pop_wrapper
//   - First turn profiling to Off
//   - Call the SLAMP function to convert events
//   - Then call SLAMP_ext_pop
VOID ExternalEndWrapper(CONTEXT *ctxt, const uint32_t id) {
  PIN_ENABLED = false;
  PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
                              AFUNPTR(event_conversion_funptr), NULL,
                              PIN_PARG(void), PIN_PARG_END());
  PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
                              AFUNPTR(ext_pop_funptr), NULL, PIN_PARG(void),
                              PIN_PARG(uint32_t), id, PIN_PARG_END());
}

/**************************************************************************
 *
 *  Instrumentation routines
 *
 **************************************************************************/

/*
 * Insert code to write data to a thread-specific buffer for instructions
 * that access memory.
 */
VOID Trace(TRACE trace, VOID *v) {
  if (!PIN_ENABLED) {
    return;
  }
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      if (!INS_IsStandardMemop(ins) && !INS_HasMemoryVector(ins)) {
        // We don't know how to treat these instructions
        continue;
      }

      UINT32 memoryOperands = INS_MemoryOperandCount(ins);

      for (UINT32 memOp = 0; memOp < memoryOperands; memOp++) {
        UINT32 refSize = INS_MemoryOperandSize(ins, memOp);

        // Note that if the operand is both read and written we log it once
        // for each.
        if (INS_MemoryOperandIsRead(ins, memOp)) {
          INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId, IARG_INST_PTR,
                               offsetof(struct MEMREF, pc), IARG_MEMORYOP_EA,
                               memOp, offsetof(struct MEMREF, ea), IARG_UINT32,
                               refSize, offsetof(struct MEMREF, size),
                               IARG_BOOL, TRUE, offsetof(struct MEMREF, read),
                               IARG_END);
        }

        if (INS_MemoryOperandIsWritten(ins, memOp)) {
          INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId, IARG_INST_PTR,
                               offsetof(struct MEMREF, pc), IARG_MEMORYOP_EA,
                               memOp, offsetof(struct MEMREF, ea), IARG_UINT32,
                               refSize, offsetof(struct MEMREF, size),
                               IARG_BOOL, FALSE, offsetof(struct MEMREF, read),
                               IARG_END);
        }
      }
    }
  }
}

/**************************************************************************
 *
 *  Callback Routines
 *
 **************************************************************************/

VOID Image(IMG img, VOID *v) {
  // Instrument the malloc() and free() functions.  Print the input argument
  // of each malloc() or free(), and the return value of malloc().
  //
  //  Find the malloc() function.
  RTN external_start_Rtn = RTN_FindByName(img, "SLAMP_ext_push");
  if (RTN_Valid(external_start_Rtn)) {
    std::cerr << "Found the ExternalStart function at "
              << RTN_Address(external_start_Rtn) << std::endl;
    if (!RTN_IsSafeForProbedReplacement(external_start_Rtn)) {
      std::cerr << "Cannot place ExternalStart wrapper function, not safe "
                   "according to RTN_IsSafeForProbedReplacement "
                << std::endl;
      ext_push_funptr = NULL;
    } else {
      ext_push_funptr = (VOID *)RTN_Address(external_start_Rtn);
    }
  }

  RTN external_stop_Rtn = RTN_FindByName(img, "SLAMP_ext_pop");
  if (RTN_Valid(external_stop_Rtn)) {
    std::cerr << "Found the ExternalStop function at "
              << RTN_Address(external_stop_Rtn) << std::endl;
    if (!RTN_IsSafeForProbedReplacement(external_stop_Rtn)) {
      std::cerr << "Cannot place ExternalStop wrapper function, not safe "
                   "according to RTN_IsSafeForProbedReplacement "
                << std::endl;
    } else {
      ext_pop_funptr = (VOID *)RTN_Address(external_stop_Rtn);
    }
  }

  RTN ext_load_Rtn = RTN_FindByName(img, "SLAMP_ext_load_1");
  if (RTN_Valid(ext_load_Rtn)) {
    ext_load_fcn_ptr = reinterpret_cast<VOID *>(RTN_Address(ext_load_Rtn));
    std::cerr << "Found the ext_load function at " << ext_load_fcn_ptr
              << std::endl;
  }

  RTN ext_store_Rtn = RTN_FindByName(img, "SLAMP_ext_store_1");
  if (RTN_Valid(ext_store_Rtn)) {
    ext_store_fcn_ptr = reinterpret_cast<VOID *>(RTN_Address(ext_store_Rtn));

    std::cerr << "Found the ext_store function at " << ext_store_fcn_ptr
              << std::endl;
  }
}

/*!
 * Called when a buffer fills up, or the thread exits, so we can process it or
 * pass it off as we see fit.
 * @param[in] id		buffer handle
 * @param[in] tid		id of owning thread
 * @param[in] ctxt		application context
 * @param[in] buf		actual pointer to buffer
 * @param[in] numElements	number of records
 * @param[in] v			callback value
 * @return  A pointer to the buffer to resume filling.
 */
VOID *BufferFull(BUFFER_ID id, THREADID tid, const CONTEXT *ctxt, VOID *buf,
                 UINT64 numElements, VOID *v) {
  struct MEMREF *reference = (struct MEMREF *)buf;

  MLOG *mlog = static_cast<MLOG *>(PIN_GetThreadData(mlog_key, tid));

  mlog->DumpBufferToFile(reference, numElements, tid);

  return buf;
}

/*
 * Note that opening a file in a callback is only supported on Linux systems.
 * See buffer-win.cpp for how to work around this issue on Windows.
 */
VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v) {
  // There is a new MLOG for every thread.  Opens the output file.
  MLOG *mlog = new MLOG(tid);

  // A thread will need to look up its MLOG, so save pointer in TLS
  PIN_SetThreadData(mlog_key, mlog, tid);
}

VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v) {
  MLOG *mlog = static_cast<MLOG *>(PIN_GetThreadData(mlog_key, tid));

  delete mlog;

  PIN_SetThreadData(mlog_key, 0, tid);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage() {
  cerr << "This tool demonstrates the basic use of the buffering API." << endl;
  cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet
 * started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[]) {
  // Initialize PIN library. Print help message if -h(elp) is specified
  // in the command line or the command line is invalid
  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  // Initialize the memory reference buffer;
  // set up the callback to process the buffer.
  //
  bufId = PIN_DefineTraceBuffer(sizeof(struct MEMREF), NUM_BUF_PAGES,
                                BufferFull, 0);

  if (bufId == BUFFER_ID_INVALID) {
    cerr << "Error: could not allocate initial buffer" << endl;
    return 1;
  }

  // Initialize thread-specific data not handled by buffering api.
  mlog_key = PIN_CreateThreadDataKey(0);

  // add an instrumentation function
  TRACE_AddInstrumentFunction(Trace, 0);

  // add callbacks
  PIN_AddThreadStartFunction(ThreadStart, 0);
  PIN_AddThreadFiniFunction(ThreadFini, 0);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
