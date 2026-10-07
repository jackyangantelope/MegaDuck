
#ifndef MEGADUCK_PACE036_H
#define MEGADUCK_PACE036_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t late_frames;
    uint32_t rebases;
    uint32_t max_late_us;
} megaduck_pace036_stats_t;

void megaduck_pace036_begin(uint32_t now);

uint32_t megaduck_pace036_wait(void);

megaduck_pace036_stats_t megaduck_pace036_take_stats(void);

#ifdef __cplusplus
}
#endif
#endif
