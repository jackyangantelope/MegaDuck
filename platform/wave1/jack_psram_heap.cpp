
#include "jack_psram_heap.h"

#include <stdint.h>
#include <string.h>

#include "hardware/psram.h"

extern "C" char __psram_end__;

namespace {
constexpr uintptr_t kBase = 0x11000000u;
constexpr size_t kCapacity = 8u * 1024u * 1024u;
constexpr uint32_t kMagic = 0x4a505352u;

struct Header {
    uint32_t magic;
    uint32_t size;
};

size_t g_offset = 0;
size_t g_heap_start = 0;

size_t align8(size_t value) { return (value + 7u) & ~size_t(7u); }
}

extern "C" void jack_psram_heap_reset(void)
{

    const uintptr_t static_end = reinterpret_cast<uintptr_t>(&__psram_end__);
    g_heap_start = static_end >= kBase && static_end <= kBase + kCapacity
        ? align8(static_end - kBase) : kCapacity;
    g_offset = g_heap_start;
}

extern "C" void *jack_psram_malloc(size_t size)
{
    if (size == 0 || !psram_is_available() || psram_get_size() < kCapacity) return nullptr;
    if (size > kCapacity - sizeof(Header)) return nullptr;
    const size_t payload = align8(size);
    const size_t total = sizeof(Header) + payload;
    if (g_offset > kCapacity || total > kCapacity - g_offset) return nullptr;
    auto *header = reinterpret_cast<Header *>(kBase + g_offset);
    header->magic = kMagic;
    header->size = static_cast<uint32_t>(payload);
    g_offset += total;
    return header + 1;
}

extern "C" void *jack_psram_calloc(size_t count, size_t size)
{
    if (count != 0 && size > SIZE_MAX / count) return nullptr;
    const size_t bytes = count * size;
    void *pointer = jack_psram_malloc(bytes);
    if (pointer != nullptr) memset(pointer, 0, bytes);
    return pointer;
}

extern "C" void *jack_psram_realloc(void *pointer, size_t size)
{
    if (pointer == nullptr) return jack_psram_malloc(size);
    if (size == 0) return nullptr;
    auto *header = reinterpret_cast<Header *>(pointer) - 1;
    if (header->magic != kMagic) return nullptr;
    if (size <= header->size) return pointer;
    void *replacement = jack_psram_malloc(size);
    if (replacement != nullptr) memcpy(replacement, pointer, header->size);
    return replacement;
}

extern "C" void jack_psram_free(void *pointer)
{

    (void)pointer;
}

extern "C" size_t jack_psram_heap_used(void) { return g_offset - g_heap_start; }
