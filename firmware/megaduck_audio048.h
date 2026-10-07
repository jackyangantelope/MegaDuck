
#pragma once
#include "megaduck_audio047.h"

typedef struct {
    uint32_t refills, added, limited;
} megaduck_recovery048_t;

uint32_t megaduck_audio048_supply(megaduck_resampler047_t *state,
    const int16_t *input, uint32_t frames, uint32_t level, uint32_t target,
    uint32_t capacity, megaduck_emit047_t emit, megaduck_recovery048_t *stats);
