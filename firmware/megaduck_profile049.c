
#include "megaduck_profile049.h"
#include "megaduck_opt047.h"
#include "hardware/exception.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/systick.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#ifdef JACK_MEGADUCK_PROFILE050
#include "megaduck_exception050.h"
#define PROFILE_TAG "050"
#define PROFILE_PARTS "17"
#else
#define PROFILE_TAG "049"
#define PROFILE_PARTS "14"
#endif

enum { HZ049 = 250, FRAME049 = 32, BUCKETS049 = 128, SHIFT049 = 8,
       PROBES049 = 16, TOP049 = 16,
#ifdef JACK_MEGADUCK_PROFILE050
       PARTS049 = 17
#else
       PARTS049 = 14
#endif
};
typedef struct { uint32_t pc; uint16_t all, slow; } bucket049_t;
typedef struct {
    uint32_t frames, late, slow_frames, max_us, samples, slow_samples;
    uint32_t hash_drop, slow_hash_drop, frame_drop, worst_n, worst_drop;
    uint32_t fg_us, fg_max;
} counts049_t;
typedef struct {
    uint32_t ticks, outside, irq, invalid, body_us, body_max;
#ifdef JACK_MEGADUCK_PROFILE050
    uint32_t basic, fp, callee, bad_exc, bad_sp, bad_psr, bad_pc;
    uint32_t good_exc, good_sp, good_pc, good_psr;
    uint32_t fail_exc, fail_sp, fail_pc, fail_psr, fail_reason;
#endif
} irq049_t;
typedef struct {
    counts049_t count;
    irq049_t irq;
    uint32_t id, epoch, elapsed, pause_us, occupied;
    bucket049_t top[TOP049], slow_top[TOP049];
    uint32_t worst[FRAME049];
} capture049_t;

static bucket049_t buckets049[BUCKETS049];
static counts049_t count049;
static volatile irq049_t irq049;
static volatile uint32_t frame_pc049[FRAME049], frame_n049, frame_drop049;
static uint32_t worst049[FRAME049];
static capture049_t capture049;
static volatile bool enabled049, scope049;
static bool ready049;
static uint32_t random049, reload049, reload_min049, reload_span049;
static uint32_t epoch049, id049, start049, due049, emit_due049;
static unsigned part049 = PARTS049;
_Static_assert(sizeof(bucket049_t) == 8, "bounded histogram layout");

static void stop049(void)
{
    systick_hw->csr = 0;
    __asm volatile("dsb\n\tisb" ::: "memory");
    scb_hw->icsr = 1u << 25;
    __asm volatile("dsb\n\tisb" ::: "memory");
}

static void start_counter049(void)
{
    systick_hw->rvr = reload049;
    systick_hw->cvr = 0;
    systick_hw->csr = 7u;
}

void __attribute__((noinline, used, section(".time_critical.profile049_record")))
megaduck_profile049_record(const uint32_t *frame, uint32_t exc_return)
{
    if (!enabled049) return;
    uint32_t start = time_us_32();
    ++irq049.ticks;
    random049 = random049 * 1664525u + 1013904223u;
    systick_hw->rvr = reload_min049 + ((random049 >> 24) * reload_span049 >> 8);
    if (!scope049) ++irq049.outside;
#ifdef JACK_MEGADUCK_PROFILE050
    else {
        uint32_t pc = 0, psr = 0;
        unsigned reason = megaduck_exception050_read(frame, exc_return, &pc, &psr);
        if (reason != MEGADUCK_FRAME050_BAD_EXC) {
            if (exc_return & 16u) ++irq049.basic; else ++irq049.fp;
            if (!(exc_return & 32u)) ++irq049.callee;
        }
        if (reason) {
            ++irq049.invalid;
            if (reason == MEGADUCK_FRAME050_BAD_EXC) ++irq049.bad_exc;
            else if (reason == MEGADUCK_FRAME050_BAD_SP) ++irq049.bad_sp;
            else if (reason == MEGADUCK_FRAME050_BAD_PSR) ++irq049.bad_psr;
            else ++irq049.bad_pc;
            if (!irq049.fail_reason) {
                irq049.fail_exc = exc_return;
                irq049.fail_sp = (uintptr_t)frame;
                irq049.fail_pc = pc;
                irq049.fail_psr = psr;
                irq049.fail_reason = reason;
            }
        } else {
            if (!irq049.good_exc) {
                irq049.good_exc = exc_return;
                irq049.good_sp = (uintptr_t)frame;
                irq049.good_pc = pc;
                irq049.good_psr = psr;
            }
            if (psr & 0x1ffu) ++irq049.irq;
            else if (frame_n049 < FRAME049) frame_pc049[frame_n049++] = pc & ~1u;
            else ++frame_drop049;
        }
    }
#else

    else if (!(exc_return & 16u) || (uintptr_t)frame < 0x20000000u ||
             (uintptr_t)frame > 0x20088000u - 32u || ((uintptr_t)frame & 3u)) {
        ++irq049.invalid;
    } else {
        uint32_t pc = frame[6] & ~1u, psr = frame[7];
        if (!(psr & (1u << 24)) ||
            !((pc >= 0x100u && pc < 0x10000u) ||
              (pc >= 0x10080000u && pc < 0x11000000u) ||
              (pc >= 0x20000000u && pc < 0x20088000u))) ++irq049.invalid;
        else if (psr & 0x1ffu) ++irq049.irq;
        else if (frame_n049 < FRAME049) frame_pc049[frame_n049++] = pc;
        else ++frame_drop049;
    }
#endif
    uint32_t cost = time_us_32() - start;
    irq049.body_us += cost;
    if (cost > irq049.body_max) irq049.body_max = cost;
}

void __attribute__((naked, used, section(".time_critical.profile049_systick")))
megaduck_profile049_systick(void)
{
    __asm volatile(
        "mov r1, lr\n"
        "tst r1, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
#ifndef JACK_MEGADUCK_PROFILE050
        "tst r1, #32\n"
        "it eq\n"
        "addeq r0, r0, #40\n"
#endif
        "b megaduck_profile049_record\n");
}

static void account049(uint32_t start)
{
    uint32_t cost = time_us_32() - start;
    count049.fg_us += cost;
    if (cost > count049.fg_max) count049.fg_max = cost;
}

bool megaduck_profile049_enabled(void) { return enabled049; }

void megaduck_profile049_enable(bool enabled)
{
    if (!ready049 || enabled == enabled049) return;
    stop049();
    enabled049 = scope049 = false;
    frame_n049 = frame_drop049 = 0;
    memset(buckets049, 0, sizeof(buckets049));
    memset(&count049, 0, sizeof(count049));
    memset((void *)&irq049, 0, sizeof(irq049));
    start049 = time_us_32();
    due049 = start049 + 7500000u;
    ++epoch049;
    enabled049 = enabled;
    if (enabled) start_counter049();

    megaduck_log047_printf("MEGADUCK_MODE" PROFILE_TAG " sample=%u epoch=%lu partial_window_discarded=1\n",
        enabled ? 1u : 0u, (unsigned long)epoch049);
}

bool megaduck_profile049_init(uint32_t clock_hz)
{
    uint32_t ticks = clock_hz / HZ049;
    if (ready049 || ticks < 8u || ticks + ticks / 8u > 0x01000000u ||
        (systick_hw->csr & 1u)) {
        megaduck_log047_printf("MEGADUCK_PROFILE" PROFILE_TAG " ready=0 reason=clock-or-systick-busy\n");
        return false;
    }
    stop049();
    reload049 = ticks - 1u;
    reload_min049 = ticks - ticks / 8u - 1u;
    reload_span049 = ticks / 4u;
    random049 = 0x49d00du;
    exception_set_exclusive_handler(SYSTICK_EXCEPTION, megaduck_profile049_systick);
    exception_set_priority(SYSTICK_EXCEPTION, PICO_LOWEST_EXCEPTION_PRIORITY);
    ready049 = true;
    megaduck_profile049_enable(true);
    megaduck_log047_printf("MEGADUCK_PROFILE" PROFILE_TAG " ready=1 hz=250 jitter=12pct report_ms=10000 first_ms=7500 bin_bytes=256 buckets=128 frame_cap=32 top=16 slow_ge_us=31500 F9=sample scope=retro-run-including-callbacks\n");
#ifdef JACK_MEGADUCK_PROFILE050
    megaduck_log047_printf("MEGADUCK_FRAME050 init layout=m33-core-first secure_only=1 fpccr=%08lx\n",
        (unsigned long)*(const volatile uint32_t *)0xe000ef34u);
#endif
    return true;
}

void megaduck_profile049_frame_begin(void)
{
    frame_n049 = frame_drop049 = 0;
    __asm volatile("" ::: "memory");
    scope049 = enabled049;
}

void megaduck_profile049_frame_stop(void)
{
    scope049 = false;
    __asm volatile("" ::: "memory");
}

void __attribute__((noinline, used)) megaduck_profile049_frame_commit(uint32_t run_us)
{
    if (!enabled049) return;
    uint32_t start = time_us_32(), n = frame_n049;
    bool slow = run_us >= 31500u;
    ++count049.frames;
    count049.late += run_us > 16743u;
    count049.slow_frames += slow;
    count049.samples += n;
    count049.slow_samples += slow ? n : 0u;
    count049.frame_drop += frame_drop049;
    if (run_us > count049.max_us) {
        count049.max_us = run_us;
        count049.worst_n = n;
        count049.worst_drop = frame_drop049;
        for (unsigned i = 0; i < n; ++i) worst049[i] = frame_pc049[i];
    }
    for (unsigned i = 0; i < n; ++i) {
        uint32_t pc = frame_pc049[i] & ~((1u << SHIFT049) - 1u);
        uint32_t hash = pc >> SHIFT049;
        hash ^= hash >> 7;
        unsigned slot = (hash * 0x9e3779b1u) & (BUCKETS049 - 1u);
        bool stored = false;
        for (unsigned j = 0; j < PROBES049; ++j) {
            bucket049_t *b = &buckets049[slot];
            if (b->pc == pc || !b->pc) {
                if (b->all != UINT16_MAX) {
                    b->pc = pc; ++b->all; b->slow += slow;
                    stored = true;
                }
                break;
            }
            slot = (slot + 1u) & (BUCKETS049 - 1u);
        }
        if (!stored) { ++count049.hash_drop; count049.slow_hash_drop += slow; }
    }

    frame_n049 = frame_drop049 = 0;
    account049(start);
}

static void insert049(bucket049_t *top, bucket049_t value, bool slow)
{
    uint16_t score = slow ? value.slow : value.all;
    if (!score) return;
    for (unsigned i = 0; i < TOP049; ++i) {
        if (score <= (slow ? top[i].slow : top[i].all)) continue;
        for (unsigned j = TOP049 - 1; j > i; --j) top[j] = top[j - 1];
        top[i] = value;
        return;
    }
}

static void snapshot049(uint32_t now)
{
    uint32_t pause_start = time_us_32();
    stop049();
    capture049.irq = irq049;
    memset((void *)&irq049, 0, sizeof(irq049));
    start_counter049();
    capture049.pause_us = time_us_32() - pause_start;
    capture049.count = count049;
    capture049.id = ++id049;
    capture049.epoch = epoch049;
    capture049.elapsed = now - start049;
    capture049.occupied = 0;
    memset(capture049.top, 0, sizeof(capture049.top));
    memset(capture049.slow_top, 0, sizeof(capture049.slow_top));
    memcpy(capture049.worst, worst049, sizeof(worst049));
    for (unsigned i = 0; i < BUCKETS049; ++i) {
        bucket049_t b = buckets049[i];
        capture049.occupied += b.all != 0;
        insert049(capture049.top, b, false);
        insert049(capture049.slow_top, b, true);
    }
    memset(buckets049, 0, sizeof(buckets049));
    memset(&count049, 0, sizeof(count049));
    start049 = now;
    due049 = now + 10000000u;
    part049 = 0;
    emit_due049 = now;
}

void __attribute__((noinline, used)) megaduck_profile049_poll(uint32_t now)
{
    if (!ready049) return;
    uint32_t start = time_us_32();
    if (enabled049 && part049 == PARTS049 && (int32_t)(now - due049) >= 0)
        snapshot049(now);
    if (part049 == PARTS049 || (int32_t)(now - emit_due049) < 0) return;
    const counts049_t *c = &capture049.count;
    unsigned long id = capture049.id;
    if (part049 == 0) {
        megaduck_log047_printf("MEGADUCK_PC" PROFILE_TAG " id=%lu epoch=%lu dt_us=%lu frames=%lu late=%lu slow=%lu max_us=%lu samples=%lu slow_samples=%lu hash_drop=%lu slow_hash_drop=%lu frame_drop=%lu irq=%lu invalid=%lu parts=" PROFILE_PARTS "\n",
            id, (unsigned long)capture049.epoch, (unsigned long)capture049.elapsed,
            (unsigned long)c->frames, (unsigned long)c->late, (unsigned long)c->slow_frames,
            (unsigned long)c->max_us, (unsigned long)c->samples, (unsigned long)c->slow_samples,
            (unsigned long)c->hash_drop, (unsigned long)c->slow_hash_drop, (unsigned long)c->frame_drop,
            (unsigned long)capture049.irq.irq, (unsigned long)capture049.irq.invalid);
    } else if (part049 == 1) {
        megaduck_log047_printf("MEGADUCK_COST" PROFILE_TAG " id=%lu ticks=%lu outside=%lu isr_body_us=%lu isr_body_max=%lu fg_us=%lu fg_max=%lu pause_us=%lu occupied=%lu worst_n=%lu worst_drop=%lu\n",
            id, (unsigned long)capture049.irq.ticks, (unsigned long)capture049.irq.outside,
            (unsigned long)capture049.irq.body_us, (unsigned long)capture049.irq.body_max,
            (unsigned long)c->fg_us, (unsigned long)c->fg_max, (unsigned long)capture049.pause_us,
            (unsigned long)capture049.occupied, (unsigned long)c->worst_n, (unsigned long)c->worst_drop);
#ifdef JACK_MEGADUCK_PROFILE050
    } else if (part049 == 14) {
        const irq049_t *q = &capture049.irq;
        megaduck_log047_printf("MEGADUCK_FRAME050 id=%lu basic=%lu fp=%lu callee=%lu bad_exc=%lu bad_sp=%lu bad_psr=%lu bad_pc=%lu\n",
            id, (unsigned long)q->basic, (unsigned long)q->fp, (unsigned long)q->callee,
            (unsigned long)q->bad_exc, (unsigned long)q->bad_sp,
            (unsigned long)q->bad_psr, (unsigned long)q->bad_pc);
    } else if (part049 >= 15) {
        const irq049_t *q = &capture049.irq;
        bool good = part049 == 15;
        megaduck_log047_printf("MEGADUCK_RAW050 id=%lu kind=%s exc=%08lx sp=%08lx pc=%08lx psr=%08lx reason=%lu\n",
            id, good ? "valid" : "invalid", (unsigned long)(good ? q->good_exc : q->fail_exc),
            (unsigned long)(good ? q->good_sp : q->fail_sp),
            (unsigned long)(good ? q->good_pc : q->fail_pc),
            (unsigned long)(good ? q->good_psr : q->fail_psr),
            (unsigned long)(good ? 0 : q->fail_reason));
#endif
    } else {
        char line[256];
        unsigned chunk = part049 < 6 ? part049 - 2 : part049 < 10 ? part049 - 6 : part049 - 10;
        const char *kind = part049 < 6 ? "all" : part049 < 10 ? "slow" : "worst";
        int used = snprintf(line, sizeof(line), "MEGADUCK_BINS" PROFILE_TAG " id=%lu kind=%s part=%u", id, kind, chunk);
        unsigned width = part049 < 10 ? 4 : 8;
        for (unsigned i = chunk * width; i < (chunk + 1u) * width; ++i) {
            if (part049 < 10) {
                bucket049_t b = part049 < 6 ? capture049.top[i] : capture049.slow_top[i];
                if (!b.all) continue;
                used += snprintf(line + used, sizeof(line) - (unsigned)used,
                    " p%u=%08lx/%u/%u", i, (unsigned long)b.pc, (unsigned)b.all, (unsigned)b.slow);
            } else if (i < c->worst_n) {
                used += snprintf(line + used, sizeof(line) - (unsigned)used,
                    " p%u=%08lx", i, (unsigned long)capture049.worst[i]);
            }
        }
        megaduck_log047_printf("%s\n", line);
    }
    ++part049;
    emit_due049 = now + 80000u;
    if (enabled049) account049(start);
}
