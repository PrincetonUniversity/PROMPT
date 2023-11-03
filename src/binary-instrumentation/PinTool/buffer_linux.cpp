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
#include "types_base.PH"
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

extern "C" {

struct MEMREF {
  // ADDRINT pc;
  ADDRINT ea;
  UINT32 size;
  BOOL read;
};

struct CUSTOM_BUFFER {
  MEMREF *buf;
  uint32_t num_elements;
  uint32_t capacity;
};
}

CUSTOM_BUFFER buffer;
bool PIN_ENABLED = false;
VOID *ext_push_funptr;
VOID *ext_pop_funptr;
VOID *event_conversion_funptr;

// Replace SLAMP_ext_push with ext_push_wrapper
//   - first call SLAMP_ext_push
//   - Then set profiling to On
VOID ExternalStartWrapper(CONTEXT *ctxt, const uint32_t id) {
  std::cerr << "external start " << id << std::endl;
  PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
                              AFUNPTR(ext_push_funptr), NULL, PIN_PARG(void),
                              PIN_PARG(uint32_t), id, PIN_PARG_END());
  PIN_ENABLED = true;
}

// Replace SLAMP_ext_pop with ext_pop_wrapper
//   - First turn profiling to Off
//   - Call the SLAMP function to convert events
//   - Then call SLAMP_ext_pop
VOID ExternalStopWrapper(CONTEXT *ctxt, const uint32_t id) {
  PIN_ENABLED = false;

  std::cerr << "external stop " << id << std::endl;

  // convert the events
  PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
                              AFUNPTR(event_conversion_funptr), NULL,
                              PIN_PARG(void), PIN_PARG(CUSTOM_BUFFER *),
                              &buffer, PIN_PARG_END());

  PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
                              AFUNPTR(ext_pop_funptr), NULL, PIN_PARG(void),
                              PIN_PARG(uint32_t), id, PIN_PARG_END());
}

VOID InsertReadToBuffer(ADDRINT memAddr, uint32_t size, INS *ins) {
  // append to the buffer, increment the buffer size

  std::cerr << "read " << memAddr << " " << size << " at " << ins << std::endl;
  MEMREF ref{memAddr, size, true};
  buffer.buf[buffer.num_elements++] = ref;
  // FIXME: check for capacity
}

VOID InsertWriteToBuffer(ADDRINT memaddr, uint32_t size, INS *ins) {

  std::cerr << "write " << memaddr << " " << size << " at " << ins << std::endl;
  // append to the buffer, increment the buffer size
  MEMREF ref{memaddr, size, false};
  buffer.buf[buffer.num_elements++] = ref;
  // FIXME: check for capacity
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

      std::cerr << "Instruction " << INS_Disassemble(ins)
                << " has memory operands " << memoryOperands << std::endl;

      for (UINT32 memOp = 0; memOp < memoryOperands; memOp++) {
        UINT32 refSize = INS_MemoryOperandSize(ins, memOp);

        // Note that if the operand is both read and written we log it once
        // for each.
        if (INS_MemoryOperandIsRead(ins, memOp)) {
          // can't use the buffer
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InsertReadToBuffer,
                         IARG_MEMORYOP_EA, memOp, IARG_UINT32, refSize,
                         IARG_INST_PTR, IARG_END);
        }

        if (INS_MemoryOperandIsWritten(ins, memOp)) {
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InsertWriteToBuffer,
                         IARG_MEMORYOP_EA, memOp, IARG_UINT32, refSize,
                         IARG_INST_PTR, IARG_END);
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
  // Replace SLAMP_ext_push with ExternalStartWrapper
  RTN external_start_Rtn = RTN_FindByName(img, "SLAMP_ext_push");
  if (RTN_Valid(external_start_Rtn)) {
    std::cerr << "Found the ExternalStart function at "
              << RTN_Address(external_start_Rtn) << std::endl;

    PROTO proto =
        PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "SLAMP_ext_push",
                       PIN_PARG(uint32_t), PIN_PARG_END());
    ext_push_funptr = (VOID *)RTN_Address(external_start_Rtn);

    RTN_ReplaceSignature(external_start_Rtn, AFUNPTR(ExternalStartWrapper),
                         IARG_PROTOTYPE, proto, IARG_CONTEXT,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);

    PROTO_Free(proto);
  } else {
    std::cerr << "Could not find the ExternalStart function" << std::endl;
  }

  // Replace SLAMP_ext_pop with ExternalStopWrapper
  RTN external_stop_Rtn = RTN_FindByName(img, "SLAMP_ext_pop");
  if (RTN_Valid(external_stop_Rtn)) {
    std::cerr << "Found the ExternalStop function at "
              << RTN_Address(external_stop_Rtn) << std::endl;

    PROTO proto =
        PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT, "SLAMP_ext_pop",
                       PIN_PARG(uint32_t), PIN_PARG_END());

    ext_pop_funptr = (VOID *)RTN_Address(external_stop_Rtn);

    RTN_ReplaceSignature(external_stop_Rtn, AFUNPTR(ExternalStopWrapper),
                         IARG_PROTOTYPE, proto, IARG_CONTEXT,
                         IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
  }

  // Find SLAMP_PIN_event_conversion
  RTN event_conversion_Rtn = RTN_FindByName(img, "SLAMP_PIN_event_conversion");

  if (RTN_Valid(event_conversion_Rtn)) {
    std::cerr << "Found the event conversion function at "
              << RTN_Address(event_conversion_Rtn) << std::endl;

    event_conversion_funptr = (VOID *)RTN_Address(event_conversion_Rtn);
  }
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

  PIN_InitSymbols();
  // Initialize PIN library. Print help message if -h(elp) is specified
  // in the command line or the command line is invalid
  if (PIN_Init(argc, argv)) {
    return Usage();
  }

  // Initialize the memory reference buffer;
  // set up the callback to process the buffer.

  // TODO: need to make sure this is PIN's memory allocation

  const uint32_t INITIAL_SIZE = 1024 * 1024;
  buffer.buf = (MEMREF *)malloc(INITIAL_SIZE * sizeof(MEMREF));
  buffer.capacity = INITIAL_SIZE;
  buffer.num_elements = 0;

  // add an instrumentation function
  TRACE_AddInstrumentFunction(Trace, 0);
  IMG_AddInstrumentFunction(Image, 0);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
