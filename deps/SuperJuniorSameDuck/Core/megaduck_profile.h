#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t ready;
    uint32_t read_cycles;
    uint32_t samples;
    uint32_t early_returns;
    uint32_t head_cycles;
    uint32_t timer_cycles;
    uint32_t bookkeeping_cycles;
    uint32_t joypad_cycles;
    uint32_t apu_cycles;
    uint32_t display_cycles;
    uint32_t tail_cycles;
} GB_megaduck_timing_profile_t;

void GB_megaduck_timing_profile_init(void);
void GB_megaduck_timing_profile_take(GB_megaduck_timing_profile_t *result);

#ifdef __cplusplus
}
#endif
