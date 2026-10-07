
#ifndef MEGADUCK_DIAG040_H
#define MEGADUCK_DIAG040_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { MEGADUCK_DMA040_PHASES = 3, MEGADUCK_DMA040_SAMPLE_INTERVAL = 127 };

typedef struct {
    uint32_t samples, max_us, ge24, ge32, entry_bad, entry_both, exit_pending;
} megaduck_dma040_phase_t;
typedef struct {
    uint32_t count, time_us, line, phase, body_us, expected, pending_in, pending_out;
    uint32_t ctrl0, ctrl1, remaining0, remaining1, fifo;
} megaduck_dma040_event_t;
typedef struct {
    megaduck_dma040_phase_t phase[MEGADUCK_DMA040_PHASES];
    megaduck_dma040_event_t event;
} megaduck_dma040_t;
typedef struct {
    uint32_t pcm, di, source, output, encoded_frames, padded, under, wait, resync;
} megaduck_queue040_t;

bool megaduck_dma040_snapshot(megaduck_dma040_t *out);
void megaduck_queue040_snapshot(megaduck_queue040_t *out);
void megaduck_diag040_init(void);
void megaduck_diag040_poll(void);
void megaduck_diag040_frame_done(void);
#ifdef __cplusplus
}
#endif
#endif
