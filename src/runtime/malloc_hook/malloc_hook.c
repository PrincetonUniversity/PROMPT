#include "malloc.h"
#include <stdbool.h>

// #include <dlfcn.h>
// #include <stddef.h>
// #include <sys/mman.h>

#include "malloc_hook.h"
bool hook_enabled = false;

void __attribute__((weak)) malloc_callback(void *ptr, size_t size) {}

void __attribute__((weak)) free_callback(void *ptr) {}

void __attribute__((weak))
realloc_callback(void *new_ptr, void *ptr, size_t size) {}

void __attribute__((weak))
memalign_callback(void *ptr, size_t alignment, size_t size) {}

void __attribute__((weak))
calloc_callback(void *ptr, size_t nmemb, size_t size) {}

// // Function pointer for the original mmap
// static void *(*original_mmap)(void *, size_t, int, int, int, off_t) = NULL;

// #ifndef RTLD_NEXT
// #define RTLD_NEXT ((void *)-1l)
// #endif

// // Initialize the original mmap function pointer
// static void init_original_mmap() {
//   if (!original_mmap) {
//     original_mmap = (void *(*)(void *, size_t, int, int, int, off_t))dlsym(
//         RTLD_NEXT, "mmap");
//   }
// }

#ifdef USE_MALLOC_HOOK
static void *(*old_malloc_hook)(size_t, const void *);
static void (*old_free_hook)(void *, const void *);
static void *(*old_memalign_hook)(size_t, size_t, const void *);
static void *(*old_realloc_hook)(void *, size_t, const void *);

#define TURN_OFF_CUSTOM_MALLOC                                                 \
  do {                                                                         \
    __malloc_hook = old_malloc_hook;                                           \
    __free_hook = old_free_hook;                                               \
    __realloc_hook = old_realloc_hook;                                         \
    __memalign_hook = old_memalign_hook;                                       \
  } while (false);

#define TURN_ON_CUSTOM_MALLOC                                                  \
  do {                                                                         \
    __malloc_hook = SLAMP_malloc_hook;                                         \
    __free_hook = SLAMP_free_hook;                                             \
    __realloc_hook = SLAMP_realloc_hook;                                       \
    __memalign_hook = SLAMP_memalign_hook;                                     \
  } while (false);

void turn_on_malloc_hook() { TURN_ON_CUSTOM_MALLOC; }

/* wrappers */
static void *SLAMP_malloc_hook(size_t size, const void *caller) {
  void *result;
  TURN_OFF_CUSTOM_MALLOC;
  result = malloc(size);
  // printf("malloc_hook: %p\n", result);
  if (hook_enabled) {
    malloc_callback(result, size);
  }
  TURN_ON_CUSTOM_MALLOC;
  return result;
}
static void SLAMP_free_hook(void *ptr, const void *caller) {
  TURN_OFF_CUSTOM_MALLOC;
  free(ptr);
  if (hook_enabled) {
    free_callback(ptr);
  }
  TURN_ON_CUSTOM_MALLOC;
}
static void *SLAMP_memalign_hook(size_t alignment, size_t size,
                                 const void *caller) {
  void *result;
  TURN_OFF_CUSTOM_MALLOC;
  result = memalign(alignment, size);
  if (hook_enabled) {
    memalign_callback(result, alignment, size);
  }
  TURN_ON_CUSTOM_MALLOC;

  return result;
}

static void *SLAMP_realloc_hook(void *ptr, size_t size, const void *caller) {
  TURN_OFF_CUSTOM_MALLOC;
  void *new_ptr = realloc(ptr, size);
  if (hook_enabled) {
    realloc_callback(new_ptr, ptr, size);
  }
  TURN_ON_CUSTOM_MALLOC;
  return new_ptr;
}
#else

extern void *__libc_malloc(size_t size);
extern void __libc_free(void *ptr);
extern void *__libc_realloc(void *ptr, size_t size);
extern void *__libc_memalign(size_t alignment, size_t size);
extern void *__libc_calloc(size_t nmemb, size_t size);

// FIXME: there are also valloc, pvalloc, posix_memalign (maybe more) that can
// allocate memory

void *malloc(size_t size) {
  void *ptr = __libc_malloc(size);
  if (hook_enabled) {
    malloc_callback(ptr, size);
  }
  return ptr;
}

void free(void *ptr) {
  __libc_free(ptr);
  if (hook_enabled) {
    free_callback(ptr);
  }
}

void *calloc(size_t nmemb, size_t size) {
  void *ptr = __libc_calloc(nmemb, size);
  if (hook_enabled) {
    calloc_callback(ptr, nmemb, size);
  }
  return ptr;
}

void *realloc(void *ptr, size_t size) {
  void *new_ptr = __libc_realloc(ptr, size);
  if (hook_enabled) {
    realloc_callback(new_ptr, ptr, size);
  }
  return new_ptr;
}

void *memalign(size_t alignment, size_t size) {
  void *ptr = __libc_memalign(alignment, size);
  if (hook_enabled) {
    memalign_callback(ptr, alignment, size);
  }
  return ptr;
}

// void *mmap(void *addr, size_t length, int prot, int flags, int fd,
//            off_t offset) {
//   init_original_mmap();
//   void *ptr = original_mmap(addr, length, prot, flags, fd, offset);

//   printf("mmap: %p\n", ptr);
//   if (hook_enabled) {
//     malloc_callback(ptr, length);
//   }
//   return ptr;
// }

#endif