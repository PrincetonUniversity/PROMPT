#ifndef MALLOC_HOOK_H
#define MALLOC_HOOK_H

#include <stddef.h>

// #define USE_MALLOC_HOOK

#ifdef __cplusplus
extern "C" {
#endif
void malloc_callback(void *ptr, size_t size);
void free_callback(void *ptr);
void realloc_callback(void *new_ptr, void *ptr, size_t size);
void memalign_callback(void *ptr, size_t alignment, size_t size);
void calloc_callback(void *ptr, size_t nmemb, size_t size);

#ifdef USE_MALLOC_HOOK
void turn_on_malloc_hook();
static void *SLAMP_malloc_hook(size_t size, const void *caller);
static void SLAMP_free_hook(void *ptr, const void *caller);
static void *SLAMP_memalign_hook(size_t alignment, size_t size,
                                 const void *caller);
static void *SLAMP_realloc_hook(void *ptr, size_t size, const void *caller);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MALLOC_HOOK_H */