#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t batches;
    uint32_t guest_cycles;
    uint32_t wide_batches;
    uint32_t max_batch;
    uint32_t apu_fast_calls;
    uint32_t lfsr_steps;
    uint32_t timer_fast_calls;
    uint32_t timer_ticks;
#ifdef JACK_MEGADUCK_PPU_DEADLINE022
    uint32_t ppu_batches;
    uint32_t ppu_cycles;
#endif
} jack_megaduck_halt017_stats_t;

extern volatile uint8_t jack_megaduck_halt017_enabled;
extern uint32_t jack_megaduck_halt017_apu_fast_calls;
extern uint32_t jack_megaduck_halt017_lfsr_steps;
extern uint32_t jack_megaduck_halt017_timer_fast_calls;
extern uint32_t jack_megaduck_halt017_timer_ticks;
#ifdef JACK_MEGADUCK_NATIVE_REPLAY
extern uint64_t jack_megaduck_halt017_native_batches;
extern uint64_t jack_megaduck_halt017_native_cycles;
extern uint64_t jack_megaduck_halt017_native_fallbacks;
extern uint64_t jack_megaduck_halt017_native_wide_batches;
extern uint64_t jack_megaduck_halt017_native_apu_fast_calls;
extern uint64_t jack_megaduck_halt017_native_lfsr_steps;
extern uint64_t jack_megaduck_halt017_native_timer_fast_calls;
extern uint64_t jack_megaduck_halt017_native_timer_ticks;
#endif
void jack_megaduck_halt017_take(jack_megaduck_halt017_stats_t *out);

#ifdef __cplusplus
}
#endif
