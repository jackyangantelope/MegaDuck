#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t batches;
    uint32_t guest_cycles;
    uint32_t guard_fallbacks;
    uint32_t short_fallbacks;
    uint32_t max_batch;
} jack_megaduck_halt015_stats_t;

extern volatile uint8_t jack_megaduck_halt015_enabled;
#ifdef JACK_MEGADUCK_NATIVE_REPLAY
extern uint64_t jack_megaduck_halt015_native_batches;
extern uint64_t jack_megaduck_halt015_native_cycles;
extern uint64_t jack_megaduck_halt015_native_fallbacks;
#endif
void jack_megaduck_halt015_take(jack_megaduck_halt015_stats_t *out);

#ifdef __cplusplus
}
#endif
