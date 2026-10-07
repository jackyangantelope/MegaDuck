
#include "megaduck_diag040.h"
#ifdef JACK_MEGADUCK_HDMI041
#include "megaduck_hdmi041.h"
#endif
#include "megaduck_ppu040.h"
#include "jack_wave1_runtime.h"
#ifdef JACK_MEGADUCK_AUDIO044
#include "jack_wave1_audio.h"
#endif
#include "pico/stdlib.h"
#include <stdio.h>
#ifdef JACK_MEGADUCK_PROFILE049
#include "megaduck_profile049.h"
#endif
#ifdef JACK_MEGADUCK_OPT047
#include "megaduck_opt047.h"
#include "hardware/clocks.h"
#define printf megaduck_log047_printf
#endif

namespace {
megaduck_dma040_t previous{};
megaduck_queue040_t previous_queue{};
uint32_t epoch, report_id, window_start, due, frames, snapshot_failures;
bool baseline_valid;

void baseline(uint32_t now)
{
    baseline_valid = megaduck_dma040_snapshot(&previous);
    megaduck_queue040_snapshot(&previous_queue);
    window_start = now;
    frames = 0;
}

void report(uint32_t now)
{
    megaduck_dma040_t sample{};
    if (!megaduck_dma040_snapshot(&sample)) {
        ++snapshot_failures;
        return;
    }
    if (!baseline_valid) {
        baseline(now);
        return;
    }
    megaduck_queue040_t queue{};
    megaduck_queue040_snapshot(&queue);
    ++report_id;
#ifdef JACK_MEGADUCK_OPT047
    if (!megaduck_log047_detailed()) {
        megaduck_dma041_t handoff{};
        const bool valid = megaduck_dma041_snapshot(&handoff);
        printf("MEGADUCK_DMA047 id=%lu dt_us=%lu updates=%lu ppu=%lu vram=%u snap=%u faults=%lu kicks=%lu resync=%lu irq_max=%lu/%lu/%lu\n",
               (unsigned long)report_id, (unsigned long)(now - window_start), (unsigned long)frames,
               (unsigned long)jack_megaduck_ppu040_enabled, megaduck_vram047_get() ? 1u : 0u,
               valid ? 1u : 0u, (unsigned long)handoff.faults, (unsigned long)handoff.kicks,
               (unsigned long)queue.resync, (unsigned long)sample.phase[0].max_us,
               (unsigned long)sample.phase[1].max_us, (unsigned long)sample.phase[2].max_us);
        previous = sample; previous_queue = queue; window_start = now; frames = 0;
        return;
    }
#endif
    printf("MEGADUCK_DMA040 id=%lu epoch=%lu ppu=%lu dt_us=%lu updates=%lu snap_fail=%lu",
           (unsigned long)report_id, (unsigned long)epoch,
           (unsigned long)jack_megaduck_ppu040_enabled,
           (unsigned long)(now - window_start), (unsigned long)frames,
           (unsigned long)snapshot_failures);
    for (unsigned i = 0; i < MEGADUCK_DMA040_PHASES; ++i) {
        const auto &a = sample.phase[i];
        const auto &b = previous.phase[i];
        printf(" p%u=%lu/%lu/%lu/%lu/%lu/%lu/%lu", i,
               (unsigned long)(a.samples - b.samples), (unsigned long)a.max_us,
               (unsigned long)(a.ge24 - b.ge24), (unsigned long)(a.ge32 - b.ge32),
               (unsigned long)(a.entry_bad - b.entry_bad),
               (unsigned long)(a.entry_both - b.entry_both),
               (unsigned long)(a.exit_pending - b.exit_pending));
    }
    printf("\n");
    printf("MEGADUCK_Q040 id=%lu pcm=%lu di=%lu source=%lu output=%lu encoded=%lu padded=%lu under=%lu wait=%lu resync=%lu\n",
           (unsigned long)report_id, (unsigned long)queue.pcm, (unsigned long)queue.di,
           (unsigned long)(queue.source - previous_queue.source),
           (unsigned long)(queue.output - previous_queue.output),
           (unsigned long)(queue.encoded_frames - previous_queue.encoded_frames),
           (unsigned long)(queue.padded - previous_queue.padded),
           (unsigned long)(queue.under - previous_queue.under),
           (unsigned long)(queue.wait - previous_queue.wait),
           (unsigned long)(queue.resync - previous_queue.resync));
    if (sample.event.count != previous.event.count) {
        const auto &e = sample.event;
        printf("MEGADUCK_EVENT040 id=%lu count=%lu t_us=%lu line=%lu phase=%lu body_us=%lu expected=%lx in=%lx out=%lx ctrl=%08lx/%08lx remaining=%lu/%lu fifo=%08lx\n",
               (unsigned long)report_id, (unsigned long)e.count,
               (unsigned long)e.time_us, (unsigned long)e.line,
               (unsigned long)e.phase, (unsigned long)e.body_us,
               (unsigned long)e.expected, (unsigned long)e.pending_in,
               (unsigned long)e.pending_out, (unsigned long)e.ctrl0,
               (unsigned long)e.ctrl1, (unsigned long)e.remaining0,
               (unsigned long)e.remaining1, (unsigned long)e.fifo);
    }
    previous = sample;
#ifdef JACK_MEGADUCK_HDMI041
    megaduck_dma041_t handoff{};
    if (megaduck_dma041_snapshot(&handoff)) {
#if defined(JACK_MEGADUCK_HDMI043)
#ifdef JACK_MEGADUCK_AUDIO044
        printf("MEGADUCK_DMA043 id=%lu fault_total=%lu chain_kicks=%lu reason=%lu pending=%lx ctrl=%08lx/%08lx line=%lu phase=%lu pixel_dma_bits=16 command_dma_bits=32\n",
#else
        printf("MEGADUCK_DMA043 id=%lu ppu=0 fault_total=%lu chain_kicks=%lu reason=%lu pending=%lx ctrl=%08lx/%08lx line=%lu phase=%lu pixel_dma_bits=16 command_dma_bits=32\n",
#endif
#elif defined(JACK_MEGADUCK_HDMI042)
        printf("MEGADUCK_DMA042 id=%lu ppu=0 fault_total=%lu chain_kicks=%lu reason=%lu pending=%lx ctrl=%08lx/%08lx line=%lu phase=%lu pixel_dma_bits=16 command_dma_bits=32\n",
#else
        printf("MEGADUCK_DMA041 id=%lu ppu=0 fault_total=%lu chain_kicks=%lu reason=%lu pending=%lx ctrl=%08lx/%08lx line=%lu phase=%lu pixel_dma_bits=16 command_dma_bits=32\n",
#endif
               (unsigned long)report_id, (unsigned long)handoff.faults,
               (unsigned long)handoff.kicks,
               (unsigned long)handoff.reason, (unsigned long)handoff.pending,
               (unsigned long)handoff.ctrl0, (unsigned long)handoff.ctrl1,
               (unsigned long)handoff.line, (unsigned long)handoff.phase);
    }
#endif
    previous_queue = queue;
    window_start = now;
    frames = 0;
}
}

extern "C" void megaduck_diag040_init(void)
{
#ifdef JACK_MEGADUCK_OPT047
    megaduck_log047_begin();
#endif
    jack_megaduck_ppu040_enabled = 0;
#ifdef JACK_MEGADUCK_AUDIO044
    jack_megaduck_ppu040_enabled = 1;
    jack_wave1_audio044_set_enabled(true);
#endif
    jack_wave1_key_event_t key{};

    while (jack_wave1_usb_pop_key(&key)) {}
#ifdef JACK_MEGADUCK_HDMI041
#if defined(JACK_MEGADUCK_OPT047)
    printf("MEGADUCK_OPT047 init ppu=1 ramp=1 resample=%u vram=%u low_request=%u clock_hz=%lu F5=resample F6=vram F7=latency F8=detail\n",
           jack_wave1_audio047_resampling() ? 1u : 0u, megaduck_vram047_get() ? 1u : 0u,
           jack_wave1_audio047_low_requested() ? 1u : 0u, (unsigned long)clock_get_hz(clk_sys));
#elif defined(JACK_MEGADUCK_AUDIO044)
    printf("MEGADUCK_AV044 init ppu=1 F1=ppu-off F2=ppu-on F3=ramp-off F4=ramp-on handoff=043 audio=034+gap-ramp clock=126MHz\n");
#elif defined(JACK_MEGADUCK_HDMI043)
    printf("MEGADUCK_HDMI043 init ppu=0 ppu_locked=1 F1=ignored F2=ignored packed_pixels=320 pixel_dma_bits=16 command_dma_bits=32 handoff=preserve-peer-irq resync_log_hz=1 audio=034 clock=126MHz\n");
#elif defined(JACK_MEGADUCK_HDMI042)
    printf("MEGADUCK_HDMI042 init ppu=0 ppu_locked=1 F1=ignored F2=ignored packed_pixels=320 pixel_dma_bits=16 command_dma_bits=32 handoff=restart-configured-target audio=034 clock=126MHz\n");
#else
    printf("MEGADUCK_HDMI041 init ppu=0 ppu_locked=1 F1=ignored F2=ignored packed_pixels=320 pixel_dma_bits=16 command_dma_bits=32 handoff_guard=1 audio=034 clock=126MHz\n");
#endif
    printf("MEGADUCK_DIAG040 init ppu=%lu sample_every=127 report_ms=10000 phases=blank,fill,data fields=samples,max_boot_us,ge24,ge32,entry_bad,entry_both,exit_pending line_miss_valid=0\n",
           (unsigned long)jack_megaduck_ppu040_enabled);
#else
    printf("MEGADUCK_DIAG040 init ppu=0 F1=off F2=on boundary=guest-frame sample_every=127 report_ms=10000 phases=blank,fill,data fields=samples,max_boot_us,ge24,ge32,entry_bad,entry_both,exit_pending line_miss_valid=0\n");
#endif
    baseline(time_us_32());
    due = window_start + 1250000u;
}

extern "C" void megaduck_diag040_poll(void)
{
    uint32_t requested = jack_megaduck_ppu040_enabled;
#ifdef JACK_MEGADUCK_AUDIO044
    bool ramp_requested = jack_wave1_audio044_get_enabled();
#endif
    jack_wave1_key_event_t key{};
    while (jack_wave1_usb_pop_key(&key)) {
#ifdef JACK_MEGADUCK_PROFILE049
        if (key.keycode == 0x42)
            megaduck_profile049_enable(!megaduck_profile049_enabled());
#endif
#ifdef JACK_MEGADUCK_OPT047
        if (key.keycode == 0x3e) {
            jack_wave1_audio047_resample(!jack_wave1_audio047_resampling());
            printf("MEGADUCK_MODE047 resample=%u\n", jack_wave1_audio047_resampling() ? 1u : 0u);
        }
        if (key.keycode == 0x3f) {
            bool ok = megaduck_core047_vram(!megaduck_vram047_get());
            printf("MEGADUCK_MODE047 vram=%u ok=%u\n", megaduck_vram047_get() ? 1u : 0u, ok ? 1u : 0u);
        }
        if (key.keycode == 0x40) {
            jack_wave1_audio047_low_latency(!jack_wave1_audio047_low_requested());
            printf("MEGADUCK_MODE047 low_request=%u warmup_us=10000000\n", jack_wave1_audio047_low_requested() ? 1u : 0u);
        }
        if (key.keycode == 0x41) {
            megaduck_log047_detail(!megaduck_log047_detailed());
            printf("MEGADUCK_MODE047 detail=%u\n", megaduck_log047_detailed() ? 1u : 0u);
        }
#endif

#if !defined(JACK_MEGADUCK_HDMI041) || defined(JACK_MEGADUCK_AUDIO044)
        if (key.keycode == 0x3a) requested = 0;
        if (key.keycode == 0x3b) requested = 1;
#endif
#ifdef JACK_MEGADUCK_AUDIO044
        if (key.keycode == 0x3c) ramp_requested = false;
        if (key.keycode == 0x3d) ramp_requested = true;
#endif
    }
    const uint32_t now = time_us_32();
#ifdef JACK_MEGADUCK_AUDIO044
    if (ramp_requested != jack_wave1_audio044_get_enabled()) {
        jack_wave1_audio044_set_enabled(ramp_requested);
        printf("MEGADUCK_RAMP044 enabled=%u boundary=guest-frame\n", ramp_requested ? 1u : 0u);
    }
#endif
    if (requested != jack_megaduck_ppu040_enabled) {
        report(now);
        jack_megaduck_ppu040_enabled = requested;
        ++epoch;
        printf("MEGADUCK_MODE040 epoch=%lu ppu=%lu boundary=guest-frame\n",
               (unsigned long)epoch, (unsigned long)requested);
        baseline(time_us_32());
        due = window_start + 1250000u;
    } else if ((int32_t)(now - due) >= 0) {
        report(now);
#ifdef JACK_MEGADUCK_OPT047
        due = now + (megaduck_log047_detailed() ? 10000000u : 30000000u);
#else
        due = now + 10000000u;
#endif
    }
}

extern "C" void megaduck_diag040_frame_done(void) { ++frames; }
