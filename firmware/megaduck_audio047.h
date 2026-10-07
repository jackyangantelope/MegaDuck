
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t phase, step;
    int16_t left, right;
    bool primed;
} megaduck_resampler047_t;
typedef struct {
    uint32_t target, stable_us, last_padded, fallbacks;
    bool requested, active, fallback, observed;
} megaduck_latency047_t;
typedef void (*megaduck_emit047_t)(uint32_t packed);
uint32_t megaduck_resample047_step(megaduck_resampler047_t *state,
    uint32_t frames, uint32_t level, uint32_t target);
uint32_t megaduck_resample047_block(megaduck_resampler047_t *state,
    const int16_t *input, uint32_t frames, uint32_t capacity, megaduck_emit047_t emit);
void megaduck_latency047_request(megaduck_latency047_t *state, bool enabled);
void megaduck_latency047_tick(megaduck_latency047_t *state,
    uint32_t padded, uint32_t interval_us);
