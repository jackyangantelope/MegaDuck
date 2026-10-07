
#include "megaduck_pace036.h"
#include "jack_wave1_runtime.h"
#include "pico/stdlib.h"

static uint32_t deadline_us, fraction;
static megaduck_pace036_stats_t stats;

static void advance_deadline(void)
{
    fraction += 46288u;
    deadline_us += 16742u + (fraction >> 16);
    fraction &= 65535u;
}

void megaduck_pace036_begin(uint32_t now)
{
    deadline_us = now;
    fraction = 0;
    stats = (megaduck_pace036_stats_t){0};
    advance_deadline();
}

uint32_t megaduck_pace036_wait(void)
{
    const uint32_t start = time_us_32();
    uint32_t now = start, usb_at = start;
    while ((int32_t)(now - deadline_us) < 0) {

        if ((uint32_t)(now - usb_at) >= 1000u) {
            jack_wave1_usb_service();
            usb_at = now;
        }
        tight_loop_contents();
        now = time_us_32();
    }
    const uint32_t late = now - deadline_us;
    if (late > 100u) ++stats.late_frames;
    if (late > stats.max_late_us) stats.max_late_us = late;
    if (late > 4u * 16743u) {

        deadline_us = now;
        fraction = 0;
        ++stats.rebases;
    }
    advance_deadline();
    return now - start;
}

megaduck_pace036_stats_t megaduck_pace036_take_stats(void)
{
    const megaduck_pace036_stats_t result = stats;
    stats = (megaduck_pace036_stats_t){0};
    return result;
}
