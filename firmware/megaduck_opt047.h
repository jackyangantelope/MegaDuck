
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

void megaduck_log047_begin(void);
void megaduck_log047_write(const char *text);
int megaduck_log047_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
void megaduck_log047_pump(void);
void megaduck_log047_detail(bool enabled);
bool megaduck_log047_detailed(void);
void megaduck_metrics047_sample(uint32_t run_us, uint32_t frame_us);
void megaduck_metrics047_report(void);

bool megaduck_vram047_bind(uint8_t **slot, uint32_t size);
void megaduck_vram047_release(void);
bool megaduck_vram047_set(bool enabled);
bool megaduck_vram047_get(void);
bool megaduck_core047_vram(bool enabled);

void jack_wave1_audio047_resample(bool enabled);
bool jack_wave1_audio047_resampling(void);
void jack_wave1_audio047_low_latency(bool enabled);
bool jack_wave1_audio047_low_requested(void);
#ifdef __cplusplus
}
#endif
