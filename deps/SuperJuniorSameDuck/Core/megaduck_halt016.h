#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t batches;
    uint32_t guest_cycles;
    uint32_t guard_fallbacks;
    uint32_t short_fallbacks;
    uint32_t wide_batches;
    uint32_t max_batch;
} jack_megaduck_halt016_stats_t;

extern volatile uint8_t jack_megaduck_halt016_enabled;
#ifdef JACK_MEGADUCK_NATIVE_REPLAY
extern uint64_t jack_megaduck_halt016_native_batches;
extern uint64_t jack_megaduck_halt016_native_cycles;
extern uint64_t jack_megaduck_halt016_native_fallbacks;
extern uint64_t jack_megaduck_halt016_native_wide_batches;
#endif
void jack_megaduck_halt016_take(jack_megaduck_halt016_stats_t *out);

#ifdef __cplusplus
}
#endif
