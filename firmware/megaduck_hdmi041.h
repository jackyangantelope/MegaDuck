
#ifndef MEGADUCK_HDMI041_H
#define MEGADUCK_HDMI041_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t faults, kicks, reason, pending, ctrl0, ctrl1, line, phase;
} megaduck_dma041_t;
bool megaduck_dma041_snapshot(megaduck_dma041_t *out);
#ifdef __cplusplus
}
#endif
#endif
