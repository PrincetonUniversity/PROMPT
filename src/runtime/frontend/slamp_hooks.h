#pragma once
// FIXME: inline tweak actually make things worse in sequential and better with
// 16x, so turn it on at all time before understanding why
#define ATTRIBUTE(x) __attribute__((x))

#include <arpa/inet.h>
#include <bits/stdint-uintn.h>
#include <clocale>
#include <dirent.h>
#include <fcntl.h>
#include <langinfo.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <wait.h>

#ifdef __cplusplus
extern "C" {
#endif

void SLAMP_dbggv(int id);
void SLAMP_dbggvstr(char *str);

// SLAMP measure functions
void SLAMP_measure_init();
void SLAMP_measure_fini();
void SLAMP_measure_load(uint32_t id, uint64_t size);
void SLAMP_measure_store(uint32_t id, uint64_t size);
static void *SLAMP_measure_malloc_hook(size_t size, const void *caller);
static void SLAMP_measure_free_hook(void *ptr, const void *caller);

void SLAMP_init(uint32_t max_insts, uint32_t fn_id, uint32_t loop_id);
void SLAMP_fini(const char *filename);

void SLAMP_allocated(uint64_t addr);
void SLAMP_init_global_vars(const char *name, uint64_t addr, size_t size);
void SLAMP_main_entry(uint32_t argc, char **argv, char **env);

void SLAMP_enter_fcn(uint32_t id);
void SLAMP_exit_fcn(uint32_t id);
void SLAMP_enter_loop(uint32_t id);
void SLAMP_exit_loop(uint32_t id);
void SLAMP_loop_iter_ctx(uint32_t id);
void SLAMP_loop_invocation();
void SLAMP_loop_iteration();
void SLAMP_loop_exit();

void SLAMP_report_base_pointer_arg(uint32_t, uint32_t, void *ptr);
void SLAMP_report_base_pointer_inst(uint32_t, void *ptr);
void SLAMP_callback_stack_alloca(uint64_t, uint64_t, uint32_t, uint64_t);
void SLAMP_callback_stack_free(void);

void SLAMP_ext_push(const uint32_t instr);
void SLAMP_ext_pop();

void SLAMP_push(const uint32_t instr);
void SLAMP_pop();

void SLAMP_load1(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 uint64_t value) ATTRIBUTE(always_inline);
void SLAMP_load2(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 uint64_t value) ATTRIBUTE(always_inline);
void SLAMP_load4(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 uint64_t value) ATTRIBUTE(always_inline);
void SLAMP_load8(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 uint64_t value) ATTRIBUTE(always_inline);
void SLAMP_loadn(uint32_t instr, const uint64_t addr, const uint32_t bare_instr,
                 size_t n) ATTRIBUTE(always_inline);
;

void SLAMP_load1_ext(const uint64_t addr, const uint32_t bare_instr,
                     uint64_t value) ATTRIBUTE(always_inline);
void SLAMP_load2_ext(const uint64_t addr, const uint32_t bare_instr,
                     uint64_t value) ATTRIBUTE(always_inline);
void SLAMP_load4_ext(const uint64_t addr, const uint32_t bare_instr,
                     uint64_t value) ATTRIBUTE(always_inline);
void SLAMP_load8_ext(const uint64_t addr, const uint32_t bare_instr,
                     uint64_t value) ATTRIBUTE(always_inline);
void SLAMP_loadn_ext(const uint64_t addr, const uint32_t bare_instr, size_t n)
    ATTRIBUTE(always_inline);
;

void SLAMP_store1(uint32_t instr, const uint64_t addr) ATTRIBUTE(always_inline);
void SLAMP_store2(uint32_t instr, const uint64_t addr) ATTRIBUTE(always_inline);
void SLAMP_store4(uint32_t instr, const uint64_t addr) ATTRIBUTE(always_inline);
void SLAMP_store8(uint32_t instr, const uint64_t addr) ATTRIBUTE(always_inline);
void SLAMP_storen(uint32_t instr, const uint64_t addr, size_t n)
    ATTRIBUTE(always_inline);
;

void SLAMP_store1_ext(const uint64_t addr, const uint32_t bare_inst)
    ATTRIBUTE(always_inline);
void SLAMP_store2_ext(const uint64_t addr, const uint32_t bare_inst)
    ATTRIBUTE(always_inline);
void SLAMP_store4_ext(const uint64_t addr, const uint32_t bare_inst)
    ATTRIBUTE(always_inline);
void SLAMP_store8_ext(const uint64_t addr, const uint32_t bare_inst)
    ATTRIBUTE(always_inline);
void SLAMP_storen_ext(const uint64_t addr, const uint32_t bare_inst, size_t n)
    ATTRIBUTE(always_inline);
;

/* wrappers */
static void *SLAMP_malloc_hook(size_t size, const void *caller);
static void *SLAMP_realloc_hook(void *ptr, size_t size, const void *caller);
static void SLAMP_free_hook(void *ptr, const void *caller);
static void *SLAMP_memalign_hook(size_t alignment, size_t size,
                                 const void *caller);
void *SLAMP_malloc(size_t size, uint32_t instr = 0, size_t alignment = 16);

void *SLAMP_calloc(size_t nelem, size_t elsize);
void *SLAMP_realloc(void *ptr, size_t size);
void *SLAMP__Znam(size_t size);
void *SLAMP__Znwm(size_t size);
#ifdef __cplusplus
}
#endif
