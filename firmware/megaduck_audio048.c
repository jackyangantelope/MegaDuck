
#include "megaduck_audio048.h"

uint32_t __attribute__((noinline, used)) megaduck_audio048_supply(
    megaduck_resampler047_t *state, const int16_t *input, uint32_t frames,
    uint32_t level, uint32_t target, uint32_t capacity,
    megaduck_emit047_t emit, megaduck_recovery048_t *stats)
{
    if (!frames || !input || !emit) return 0;
    const uint32_t span = frames << 16;
    const uint32_t phase = state->primed ? state->phase : 0u;
    const uint32_t remaining = phase < span ? span - phase : 0u;
    const uint32_t smooth = state->step ? state->step : 65536u;
    const uint32_t nominal = (remaining + smooth - 1u) / smooth;

    uint32_t wanted = level < target ? target - level : 0u;
    if (wanted > frames * 4u) wanted = frames * 4u;
    if (wanted > capacity) wanted = capacity;
    if (level < target / 2u && remaining && wanted > nominal) {
        state->step = (remaining + wanted - 1u) / wanted;
        uint32_t output = megaduck_resample047_block(state, input, frames, capacity, emit);
        state->step = smooth;
        if (stats) {
            ++stats->refills;
            if (output > nominal) stats->added += output - nominal;
        }
        return output;
    }

    uint32_t step = megaduck_resample047_step(state, frames, level, target);
    if (stats && (remaining + step - 1u) / step > capacity) ++stats->limited;
    return megaduck_resample047_block(state, input, frames, capacity, emit);
}
