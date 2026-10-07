
#include "megaduck_audio047.h"
#include <limits.h>

uint32_t __attribute__((noinline, used)) megaduck_resample047_step(
    megaduck_resampler047_t *state, uint32_t frames, uint32_t level, uint32_t target)
{
    if (!frames) return state->step;
    int32_t correction = ((int32_t)target - (int32_t)level - (int32_t)frames) / 8;
    int32_t output = (int32_t)frames + correction;
    if (output < (int32_t)((frames + 1u) / 2u)) output = (frames + 1u) / 2u;
    if (output > (int32_t)(frames * 4u)) output = frames * 4u;
    uint32_t goal = (frames << 16) / (uint32_t)output;
    if (!state->step) state->step = 65536u;
    int32_t difference = (int32_t)goal - (int32_t)state->step;
    int32_t delta = difference / 8;
    if (difference && !delta) delta = difference > 0 ? 1 : -1;
    if (delta > 512) delta = 512;
    if (delta < -512) delta = -512;
    state->step = (uint32_t)((int32_t)state->step + delta);
    return state->step;
}

uint32_t __attribute__((noinline, used)) megaduck_resample047_block(
    megaduck_resampler047_t *state, const int16_t *input,
    uint32_t frames, uint32_t capacity, megaduck_emit047_t emit)
{
    if (!frames || !input || !emit) return 0;
    if (!state->primed) {
        state->left = input[0]; state->right = input[1];
        state->phase = 0; state->primed = true;
    }
    uint32_t span = frames << 16, position = state->phase;
    uint32_t step = state->step ? state->step : 65536u;
    if (capacity && position < span) {
        uint32_t minimum = (span - position + capacity - 1u) / capacity;
        if (step < minimum) step = minimum;
    }
    uint32_t output = 0;
    while (position < span && output < capacity) {
        uint32_t index = position >> 16, fraction = position & 65535u;
        int32_t al = index ? input[(index - 1u) * 2u] : state->left;
        int32_t ar = index ? input[(index - 1u) * 2u + 1u] : state->right;
        int32_t bl = input[index * 2u], br = input[index * 2u + 1u];

        int16_t left = (int16_t)(al + (((int64_t)bl - al) * fraction >> 16));
        int16_t right = (int16_t)(ar + (((int64_t)br - ar) * fraction >> 16));
        emit((uint16_t)left | ((uint32_t)(uint16_t)right << 16));
        ++output; position += step;
    }
    state->phase = position >= span ? position - span : 0;
    state->left = input[(frames - 1u) * 2u];
    state->right = input[(frames - 1u) * 2u + 1u];
    return output;
}

void __attribute__((noinline, used)) megaduck_latency047_request(
    megaduck_latency047_t *state, bool enabled)
{
    state->requested = enabled;
    state->stable_us = 0;
    state->active = false;
    state->fallback = false;
    state->observed = false;
}

void __attribute__((noinline, used)) megaduck_latency047_tick(
    megaduck_latency047_t *state, uint32_t padded, uint32_t interval_us)
{
    if (!state->target) state->target = 2940u;
    bool missing = state->observed && padded != state->last_padded;
    state->observed = true;
    state->last_padded = padded;
    if (missing) {
        state->stable_us = 0;
        if (state->active) {
            state->fallback = true;
            state->active = false;
            ++state->fallbacks;
        }
    } else if (!state->active && state->requested && !state->fallback) {
        if (!interval_us || interval_us > 25000u) state->stable_us = 0;
        else if (state->stable_us < 10000000u) state->stable_us += interval_us;
        if (state->stable_us >= 10000000u) state->active = true;
    }
    uint32_t goal = state->requested && state->active && !state->fallback ? 2205u : 2940u;
    if (state->target > goal) {
        uint32_t difference = state->target - goal;
        state->target -= difference < 4u ? difference : 4u;
    } else if (state->target < goal) {
        uint32_t difference = goal - state->target;
        state->target += difference < 16u ? difference : 16u;
    }
}
