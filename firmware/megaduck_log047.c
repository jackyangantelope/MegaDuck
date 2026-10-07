
#include "megaduck_opt047.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_CAPACITY 2048u
static char log_queue047[LOG_CAPACITY], log_format047[640];
static uint32_t head047, tail047, dropped047, truncated047, sent047;
static uint32_t cpu_us047, max_call_us047;
static bool active047, detailed047;

static void account(uint32_t start)
{
    uint32_t cost = time_us_32() - start;
    cpu_us047 += cost;
    if (cost > max_call_us047) max_call_us047 = cost;
}

void megaduck_log047_begin(void) { active047 = true; }
void megaduck_log047_detail(bool enabled) { detailed047 = enabled; }
bool megaduck_log047_detailed(void) { return detailed047; }

void __attribute__((noinline, used)) megaduck_log047_write(const char *text)
{
    if (!active047) { printf("%s", text); return; }
    uint32_t start = time_us_32(), count = 0;
    for (const char *s = text; *s; ++s) {
        count += *s == '\n' ? 2u : 1u;
        if (count > LOG_CAPACITY) { ++dropped047; account(start); return; }
    }
    if (count > LOG_CAPACITY - (head047 - tail047)) {
        ++dropped047; account(start); return;
    }
    for (const char *s = text; *s; ++s) {
        if (*s == '\n') log_queue047[head047++ & (LOG_CAPACITY - 1u)] = '\r';
        log_queue047[head047++ & (LOG_CAPACITY - 1u)] = *s;
    }
    account(start);
}

int __attribute__((noinline, used)) megaduck_log047_printf(const char *format, ...)
{
    uint32_t start = time_us_32();
    va_list args;
    va_start(args, format);
    int size = vsnprintf(log_format047, sizeof(log_format047), format, args);
    va_end(args);
    account(start);
    if (size < 0 || (unsigned)size >= sizeof(log_format047)) {
        ++truncated047; return size;
    }
    megaduck_log047_write(log_format047);
    return size;
}

void __attribute__((noinline, used)) megaduck_log047_pump(void)
{
    if (!active047 || head047 == tail047) return;
    uint32_t start = time_us_32();
    for (unsigned n = 0; n < 32u && head047 != tail047; ++n) {
        if (!uart_is_writable(uart0)) break;
        uart_get_hw(uart0)->dr = (uint8_t)log_queue047[tail047++ & (LOG_CAPACITY - 1u)];
        ++sent047;
    }
    account(start);
}

static uint32_t run_hist047[64], frame_hist047[64];
static uint32_t runs047, frames047, run_max047, frame_max047, run_over047;
void __attribute__((noinline, used)) megaduck_metrics047_sample(uint32_t run_us, uint32_t frame_us)
{
    unsigned bucket = run_us / 500u;
    ++run_hist047[bucket < 63u ? bucket : 63u];
    ++runs047;
    if (run_us > run_max047) run_max047 = run_us;
    if (run_us > 16743u) ++run_over047;
    if (frame_us) {
        bucket = frame_us / 500u;
        ++frame_hist047[bucket < 63u ? bucket : 63u];
        ++frames047;
        if (frame_us > frame_max047) frame_max047 = frame_us;
    }
}

uint32_t __attribute__((noinline, used)) megaduck_percentile047(
    const uint32_t *histogram, uint32_t count, uint32_t percent)
{
    if (!count) return 0;
    uint32_t rank = (count * percent + 99u) / 100u, sum = 0;
    for (unsigned i = 0; i < 64u; ++i) {
        sum += histogram[i];
        if (sum >= rank) return i == 63u ? UINT32_MAX : (i + 1u) * 500u;
    }
    return UINT32_MAX;
}

void megaduck_metrics047_report(void)
{
    const uint32_t log_cost = cpu_us047, log_max = max_call_us047;

    cpu_us047 = max_call_us047 = 0;
    megaduck_log047_printf("MEGADUCK_TIME047 n=%lu run_p95_lt=%lu run_p99_lt=%lu run_max=%lu run_over=%lu frame_n=%lu frame_p99_lt=%lu frame_max=%lu log_us=%lu log_max=%lu log_drop=%lu log_trunc=%lu log_pending=%lu sent=%lu\n",
        (unsigned long)runs047,
        (unsigned long)megaduck_percentile047(run_hist047, runs047, 95),
        (unsigned long)megaduck_percentile047(run_hist047, runs047, 99),
        (unsigned long)run_max047, (unsigned long)run_over047, (unsigned long)frames047,
        (unsigned long)megaduck_percentile047(frame_hist047, frames047, 99),
        (unsigned long)frame_max047, (unsigned long)log_cost, (unsigned long)log_max,
        (unsigned long)dropped047, (unsigned long)truncated047,
        (unsigned long)(head047 - tail047), (unsigned long)sent047);
    memset(run_hist047, 0, sizeof(run_hist047));
    memset(frame_hist047, 0, sizeof(frame_hist047));
    runs047 = frames047 = run_max047 = frame_max047 = run_over047 = 0;
}
