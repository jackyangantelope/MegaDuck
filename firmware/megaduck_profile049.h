
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool megaduck_profile049_init(uint32_t clock_hz);
bool megaduck_profile049_enabled(void);
void megaduck_profile049_enable(bool enabled);
void megaduck_profile049_frame_begin(void);
void megaduck_profile049_frame_stop(void);
void megaduck_profile049_frame_commit(uint32_t run_us);
void megaduck_profile049_poll(uint32_t now);
#ifdef __cplusplus
}
#endif
