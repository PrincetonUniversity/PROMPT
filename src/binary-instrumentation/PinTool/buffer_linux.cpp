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
#include "types_core.PH"
#include "types_vmapi.PH"
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include <smmintrin.h>
#include <xmmintrin.h>
using std::cerr;
using std::endl;
using std::hex;
using std::ofstream;
using std::string;

#define DEBUG_PIN 0

/* ===================================================================== */
/* QUEUE                                                                 */
/* ===================================================================== */

// START OF QUEUE
#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
/*#define CACHELINE_SIZE 64 */
/* Cache line size of glacier, as reported by lmbench. Other tests,
 * namely the smtx ones, vindicate this. */
#endif /* CACHELINE_SIZE */
#define PAD(suffix, size) char padding##suffix[CACHELINE_SIZE - (size)]

#define QTYPE uint32_t
#ifndef QSIZE
#define QSIZE_BYTES                                                            \
  (1 << 20) // 1 << 0 - 1 byte; 1 << 10 1KB; 1 << 20 1MB; 1 << 24 16MB; 1 << 26
// (1 << 27) // 1 << 0 - 1 byte; 1 << 10 1KB; 1 << 20 1MB; 1 << 24 16MB; 1 << 26
// 64MB; 1 << 28 256MB; 1 << 30 1GB
#define QSIZE (QSIZE_BYTES / sizeof(QTYPE))
// #define QSIZE (1 << 23)
#endif /* QSIZE */

static constexpr uint64_t QSIZE_GUARD = QSIZE - 60;
struct UnderlyingQueue {
  volatile bool ready_to_read;
  PAD(1, sizeof(bool));
  volatile bool ready_to_write;
  PAD(2, sizeof(bool));
  uint64_t size;
  PAD(3, sizeof(uint64_t));
  uint32_t *data;

  void init(uint32_t *data) {
    this->ready_to_read = false;
    this->ready_to_write = true;
    this->size = 0;
    this->data = data;
  }
};
using Queue = UnderlyingQueue;
using Queue_p = Queue *;
struct ControlBlock {
  uint64_t dq_index;
  uint32_t *dq_data;
  Queue_p qA, qB, qNow, qOther;
};
volatile ControlBlock *cb; // we need to go between two instrumentations
void swap() {
  if (cb->qNow == cb->qA) {
    cb->qNow = cb->qB;
    cb->qOther = cb->qA;
  } else {
    cb->qNow = cb->qA;
    cb->qOther = cb->qB;
  }
  cb->dq_data = cb->qNow->data;
}

void flush() {
  cb->qNow->size = cb->dq_index;
  cb->qNow->ready_to_read = true;
  cb->qNow->ready_to_write = false;
}

#define ATTRIBUTE(x) __attribute__((x))
void ATTRIBUTE(noinline) produce_wait() {
  flush();
  while (!cb->qOther->ready_to_write) {
    // spin
    usleep(10);
  }
  swap();
  cb->qNow->ready_to_read = false;
  cb->dq_index = 0;
  // total_swapped++;
}

void ATTRIBUTE(noinline) produce_8_32_64(uint8_t x, uint32_t y, uint64_t z) {
  uint32_t x_tmp = x;
  _mm_stream_si32((int *)&cb->dq_data[cb->dq_index], x_tmp);
  _mm_stream_si32((int *)&cb->dq_data[cb->dq_index + 1], y);
  _mm_stream_si64((long long *)&cb->dq_data[cb->dq_index + 2], z);
  cb->dq_index += 4;
  if (cb->dq_index >= QSIZE_GUARD) {
    produce_wait();
  }
}

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

VOID SetControlBlock(void *cb) {
  std::cerr << "set control block " << cb;
  ::cb = (ControlBlock *)cb;

  std::cerr << " dq_index " << ::cb->dq_index << std::endl;
  std::cerr << " dq_data " << ::cb->dq_data << std::endl;
}

static uint32_t ext_id = 0;
// Replace SLAMP_ext_push with ext_push_wrapper
//   - first call SLAMP_ext_push
//   - Then set profiling to On
VOID ExternalStartWrapper(CONTEXT *ctxt, const uint32_t id) {
#if DEBUG_PIN
  std::cerr << "external start " << id << std::endl;
#endif
  ext_id = id;
  // PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
  //                             AFUNPTR(ext_push_funptr), NULL, PIN_PARG(void),
  //                             PIN_PARG(uint32_t), id, PIN_PARG_END());
  PIN_ENABLED = true;
}

// Replace SLAMP_ext_pop with ext_pop_wrapper
//   - First turn profiling to Off
//   - Call the SLAMP function to convert events
//   - Then call SLAMP_ext_pop
VOID ExternalStopWrapper(CONTEXT *ctxt, const uint32_t id) {
  PIN_ENABLED = false;

#if DEBUG_PIN
  std::cerr << "external stop " << ext_id << std::endl;
#endif
  ext_id = -1;

  // // convert the events
  // PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
  //                             AFUNPTR(event_conversion_funptr), NULL,
  //                             PIN_PARG(void), PIN_PARG(CUSTOM_BUFFER *),
  //                             &buffer, PIN_PARG_END());

  // PIN_CallApplicationFunction(ctxt, PIN_ThreadId(), CALLINGSTD_DEFAULT,
  //                             AFUNPTR(ext_pop_funptr), NULL, PIN_PARG(void),
  //                             PIN_PARG(uint32_t), id, PIN_PARG_END());
}

VOID InsertReadToBuffer(ADDRINT memAddr, uint32_t size, INS *ins) {
#if DEBUG_PIN
  std::cerr << "read " << memAddr << " " << ext_id << endl;
#endif
  // std::endl; MEMREF ref{memAddr, size, true};
  // buffer.buf[buffer.num_elements++] = ref;
  // // FIXME: check for capacity
  produce_8_32_64(1, ext_id, memAddr);
}

VOID InsertWriteToBuffer(ADDRINT memAddr, uint32_t size, INS *ins) {
#if DEBUG_PIN
  std::cerr << "write " << memAddr << " " << ext_id << endl;
#endif
  // std::endl;
  // // append to the buffer, increment the buffer size
  // MEMREF ref{memaddr, size, false};
  // buffer.buf[buffer.num_elements++] = ref;
  // // FIXME: check for capacity
  produce_8_32_64(2, ext_id, memAddr);
}

/**************************************************************************
 *
 *  Instrumentation routines
 *
 **************************************************************************/

static uint64_t start_image_addr = 0;
static uint64_t end_image_addr = 0;
/*
 * Insert code to write data to a thread-specific buffer for instructions
 * that access memory.
 */
static uint32_t trace_cnt = 0;
VOID Trace(TRACE trace, VOID *v) {
  trace_cnt++;
  // print the function name
  if (!PIN_ENABLED) {
    return;
  }

  // #if DEBUG_PIN
  std::cerr << "trace " << TRACE_Address(trace) << " "
            << RTN_FindNameByAddress(TRACE_Address(trace)) << std::endl;
  // #endif

  // if from the main image, don't instrument
  if (TRACE_Address(trace) >= start_image_addr &&
      TRACE_Address(trace) <= end_image_addr) {
#if DEBUG_PIN
    std::cerr << "trace in main image" << std::endl;
#endif
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
  // get the starting and ending range of the image if it contains the main
  // function
  RTN main_Rtn = RTN_FindByName(img, "main");
  if (RTN_Valid(main_Rtn)) {
    std::cerr << "Found the main function at " << RTN_Address(main_Rtn)
              << std::endl;
    start_image_addr = IMG_LowAddress(img);
    end_image_addr = IMG_HighAddress(img);
  }

  // find SLAMP_report_control_block(void *)
  RTN report_control_block_Rtn =
      RTN_FindByName(img, "SLAMP_report_control_block");
  if (RTN_Valid(report_control_block_Rtn)) {
    std::cerr << "Found the report control block function at "
              << RTN_Address(report_control_block_Rtn) << std::endl;

    PROTO proto = PROTO_Allocate(PIN_PARG(void), CALLINGSTD_DEFAULT,
                                 "SLAMP_report_control_block", PIN_PARG(void *),
                                 PIN_PARG_END());
    RTN_ReplaceSignature(report_control_block_Rtn, AFUNPTR(SetControlBlock),
                         IARG_PROTOTYPE, proto, IARG_FUNCARG_ENTRYPOINT_VALUE,
                         0, IARG_END);
  }
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

void Fini(INT32 code, VOID *v) {
  std::cerr << "trace_cnt " << trace_cnt << std::endl;
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

  // register Fini to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
