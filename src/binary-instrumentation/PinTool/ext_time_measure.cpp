/*
 * Copyright (C) 2009-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include "pin.H"
#include <iostream>

/*
 * Record of memory references.  Rather than having two separate
 * buffers for reads and writes, we just use one struct that includes a
 * flag for type.
 */

static inline uint64_t rdtsc() {
  uint64_t a, d;
  __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
  return (d << 32) | a;
}

bool PIN_ENABLED = false;
VOID *ext_push_funptr;
VOID *ext_pop_funptr;

uint64_t ext_rdtsc_start_cycles = 0;
uint64_t beginning_cycles = 0;
uint64_t total_cycles = 0;

uint64_t start_cnt = 0;
uint64_t stop_cnt = 0;

// Replace SLAMP_ext_push with ext_push_wrapper
//   - first call SLAMP_ext_push
//   - Then set profiling to On
VOID ExternalStartWrapper() {
  ext_rdtsc_start_cycles = PIN_ENABLED ? ext_rdtsc_start_cycles : rdtsc();
  PIN_ENABLED = true;
  start_cnt++;
}

// Replace SLAMP_ext_pop with ext_pop_wrapper
//   - First turn profiling to Off
//   - Call the SLAMP function to convert events
//   - Then call SLAMP_ext_pop
VOID ExternalStopWrapper() {
  PIN_ENABLED = false;
  total_cycles += rdtsc() - ext_rdtsc_start_cycles;
  stop_cnt++;
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
                         IARG_PROTOTYPE, proto,
                         //  IARG_CONTEXT,
                         //  IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_END);

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
                         IARG_PROTOTYPE, proto,
                         //  IARG_CONTEXT,
                         //  IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                         IARG_END);
  }
}

void Start() {
  std::cerr << "Starting program" << std::endl;
  beginning_cycles = rdtsc();
}

void Fini(INT32 code, VOID *v) {
  std::cerr << "Start/stop cnt: " << std::endl;
  std::cerr << start_cnt << " " << stop_cnt << std::endl;
  // total time in seconds
  std::cerr << "Total time and total time in external (s)" << std::endl
            << (rdtsc() - beginning_cycles) / 2.6e9 << std::endl
            << total_cycles / 2.6e9 << std::endl;
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
  PIN_Init(argc, argv);

  IMG_AddInstrumentFunction(Image, 0);

  // add a before main function
  PIN_AddApplicationStartFunction(APPLICATION_START_CALLBACK(Start), 0);

  // add a application finish function
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
