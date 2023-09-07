#ifndef MALLOC_HOOK_H
#define MALLOC_HOOK_H

#include <stddef.h>

void malloc_callback(void* ptr, size_t size);
void free_callback(void* ptr);
void realloc_callback(void* new_ptr, void* ptr, size_t size);
void memalign_callback(void* ptr, size_t alignment, size_t size);

#endif /* MALLOC_HOOK_H */