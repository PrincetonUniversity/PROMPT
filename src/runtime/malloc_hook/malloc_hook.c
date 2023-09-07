#include <stdbool.h>
#include "malloc_hook.h"

bool hook_enabled = false;

extern void* __libc_malloc(size_t size);
extern void __libc_free(void* ptr);
extern void* __libc_realloc(void* ptr, size_t size);
extern void* __libc_memalign(size_t alignment, size_t size);

void __attribute__((weak)) malloc_callback(void* ptr, size_t size) {}

void __attribute__((weak)) free_callback(void* ptr) {}


void __attribute__((weak)) realloc_callback(void* new_ptr, void* ptr, size_t size) {}

void __attribute__((weak)) memalign_callback(void* ptr, size_t alignment, size_t size) {}

void* malloc(size_t size) {
    void* ptr =  __libc_malloc(size);
    if (hook_enabled) {
        malloc_callback(ptr, size);
    }
    return ptr;
}

void free(void* ptr) {
    __libc_free(ptr);
    if (hook_enabled) {
        free_callback(ptr);
    }
}

void* realloc(void* ptr, size_t size) {
    void* new_ptr = __libc_realloc(ptr, size);
    if (hook_enabled) {
        realloc_callback(new_ptr, ptr, size);
    }
    return new_ptr;
}

void* memalign(size_t alignment, size_t size) {
    void* ptr = __libc_memalign(alignment, size);
    if (hook_enabled) {
        memalign_callback(ptr, alignment, size);
    }
    return ptr;
}
