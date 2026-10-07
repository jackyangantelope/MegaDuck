
#ifndef JACK_PSRAM_HEAP_H
#define JACK_PSRAM_HEAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void jack_psram_heap_reset(void);
void *jack_psram_malloc(size_t size);
void *jack_psram_calloc(size_t count, size_t size);
void *jack_psram_realloc(void *pointer, size_t size);
void jack_psram_free(void *pointer);
size_t jack_psram_heap_used(void);

#ifdef __cplusplus
}
#endif

#endif
