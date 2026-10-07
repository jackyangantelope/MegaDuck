
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "jack_psram_heap.h"
#include "jack_wave1_perf.h"
#include "jack_wave1_runtime.h"
#include "libretro.h"
#include "pico/stdlib.h"
#ifdef JACK_POKEMINI_FAST_VIDEO
#include "pokemini_video.h"
#endif
#ifdef JACK_POKEMINI_DEFERRED_LOG
#include "pokemini_log.h"
#endif
#ifdef JACK_MEGADUCK_STACK_FIX032
#include "megaduck_stack032.h"
#endif
#ifdef JACK_MEGADUCK_PACE036
#include "megaduck_pace036.h"
#endif
#ifdef JACK_MEGADUCK_DIAG040
#include "megaduck_diag040.h"
#endif
#ifdef JACK_WAVE1_HDMI_AUDIO
#include "jack_wave1_audio.h"
#endif
#ifdef JACK_MEGADUCK_PROFILE014
#include "megaduck_probe014.h"
#endif
#ifdef JACK_MEGADUCK_HALT015
#include "megaduck_halt015.h"
#endif
#ifdef JACK_MEGADUCK_HALT016
#include "megaduck_halt016.h"
#endif
#ifdef JACK_MEGADUCK_HALT017
#include "megaduck_halt017.h"
#endif
#ifdef JACK_MEGADUCK_PC_SAMPLER
#include "hardware/clocks.h"
#include "megaduck_pc_sampler.h"
#endif
#ifdef JACK_MEGADUCK_PROFILE038
#include "hardware/clocks.h"
#include "megaduck_profile038.h"
#endif
#ifdef JACK_MEGADUCK_PROFILE049
#include "hardware/clocks.h"
#include "megaduck_profile049.h"
#endif

#ifndef JACK_LR_APP_NAME
#error JACK_LR_APP_NAME is required
#endif
#ifndef JACK_LR_PICKER_TITLE
#error JACK_LR_PICKER_TITLE is required
#endif
#ifndef JACK_LR_ROM_DIRECTORY
#error JACK_LR_ROM_DIRECTORY is required
#endif
#ifndef JACK_LR_EXTENSIONS
#error JACK_LR_EXTENSIONS is required
#endif
#ifndef JACK_LR_LOG_PREFIX
#define JACK_LR_LOG_PREFIX "LIBRETRO"
#endif
#ifndef JACK_LR_VERSION
#define JACK_LR_VERSION "0.1.0-playable"
#endif

namespace {
enum retro_pixel_format g_pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
uint16_t g_player_buttons[2]{};
JackWave1Perf g_perf;
char g_rom_path[JACK_WAVE1_PATH_MAX]{};
uint32_t g_last_draw_us = 0;
unsigned g_last_video_width = 0;
unsigned g_last_video_height = 0;
constexpr char kSystemDirectory[] = "/SYSTEM";
constexpr char kSaveDirectory[] = "/SAVES";

void core_log(enum retro_log_level level, const char *format, ...)
{
    printf("%s_CORE level=%d ", JACK_LR_LOG_PREFIX, static_cast<int>(level));
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

bool environment(unsigned command, void *data)
{
    switch (command) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        g_pixel_format = *static_cast<enum retro_pixel_format *>(data);
        return g_pixel_format == RETRO_PIXEL_FORMAT_RGB565 ||
               g_pixel_format == RETRO_PIXEL_FORMAT_XRGB8888 ||
               g_pixel_format == RETRO_PIXEL_FORMAT_0RGB1555;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback *>(data)->log = core_log;
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<const char **>(data) = kSystemDirectory;
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char **>(data) = kSaveDirectory;
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool *>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        retro_variable *var = static_cast<retro_variable *>(data);
        if (!var || !var->key) return false;
        if (strcmp(var->key, "pokemini_video_scale") == 0) var->value = "1x";
        else if (strcmp(var->key, "pokemini_60hz_mode") == 0) var->value = "enabled";
        else return false;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
        return true;
    default:
        return false;
    }
}

void input_poll()
{
    jack_wave1_poll_player_buttons(g_player_buttons);
#ifdef JACK_WAVE1_HDMI_AUDIO
    static bool mute_held = false, muted = false;
    const bool held = (g_player_buttons[0] & JACK_KEYBOARD_MUTE) != 0;
    if (held && !mute_held) {
        muted = !muted;
        jack_wave1_audio_mute(muted);
        printf("MEGADUCK_AUDIO mute=%u\n", muted ? 1u : 0u);
    }
    mute_held = held;
#endif
}

int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    (void)index;
    if (port >= 2 || device != RETRO_DEVICE_JOYPAD) return 0;
    const uint16_t buttons = g_player_buttons[port];
    uint16_t bits = 0;
    if (buttons & JACK_BUTTON_B) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
    if (buttons & JACK_BUTTON_Y) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
    if (buttons & JACK_BUTTON_SELECT) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
    if (buttons & JACK_BUTTON_START) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
    if (buttons & JACK_BUTTON_UP) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
    if (buttons & JACK_BUTTON_DOWN) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
    if (buttons & JACK_BUTTON_LEFT) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
    if (buttons & JACK_BUTTON_RIGHT) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
    if (buttons & JACK_BUTTON_A) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
    if (buttons & JACK_BUTTON_X) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
    if (buttons & JACK_BUTTON_L) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
    if (buttons & JACK_BUTTON_R) bits |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return static_cast<int16_t>(bits);
    return id < 16 && (bits & (1u << id)) ? 1 : 0;
}
#ifdef JACK_WAVE1_HDMI_AUDIO
void audio_sample(int16_t left, int16_t right) {
    const int16_t samples[2] = {left, right};
    jack_wave1_audio_submit(samples, 1);
}
size_t audio_batch(const int16_t *samples, size_t frames) {
    return jack_wave1_audio_submit(samples, frames);
}
#else
void audio_sample(int16_t, int16_t) {}
size_t audio_batch(const int16_t *, size_t frames) { return frames; }
#endif

uint16_t convert_pixel(const void *data, size_t index)
{
    if (g_pixel_format == RETRO_PIXEL_FORMAT_RGB565) {
        const uint16_t p = static_cast<const uint16_t *>(data)[index];
        return static_cast<uint16_t>(((p >> 1u) & 0x7c00u) |
                                     ((p >> 1u) & 0x03e0u) | (p & 0x001fu));
    }
    if (g_pixel_format == RETRO_PIXEL_FORMAT_XRGB8888) {
        const uint32_t p = static_cast<const uint32_t *>(data)[index];
        return jack_wave1_rgb555((p >> 16u) & 0xffu, (p >> 8u) & 0xffu, p & 0xffu);
    }
    return static_cast<const uint16_t *>(data)[index] & 0x7fffu;
}

void video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (data == nullptr || width == 0 || height == 0) return;
    const uint32_t draw_start = time_us_32();
#ifdef JACK_POKEMINI_FAST_VIDEO
    if (width == 96 && height == 64 && g_pixel_format == RETRO_PIXEL_FORMAT_RGB565 &&
        pitch >= 96 * sizeof(uint16_t) && pitch % sizeof(uint16_t) == 0) {
        uint16_t *frame = jack_wave1_framebuffer();
        if (width != g_last_video_width || height != g_last_video_height) {
            memset(frame, 0, JACK_WAVE1_SCREEN_WIDTH * JACK_WAVE1_SCREEN_HEIGHT * sizeof(uint16_t));
            g_last_video_width = width;
            g_last_video_height = height;
            printf("%s_VIDEO path=rgb565-3x source=96x64 output=288x192 offset=16,24\n",
                   JACK_LR_LOG_PREFIX);
        }
        jack_pokemini_blit_rgb565_3x(static_cast<const uint16_t *>(data), pitch, frame);
        g_last_draw_us = time_us_32() - draw_start;
        return;
    }
#endif
#ifdef JACK_LR_NATIVE_RGB555
    if (g_pixel_format == RETRO_PIXEL_FORMAT_0RGB1555 &&
        pitch == static_cast<size_t>(width) * sizeof(uint16_t) &&
        width <= JACK_WAVE1_SCREEN_WIDTH &&
        height <= JACK_WAVE1_SCREEN_HEIGHT) {
        uint16_t *frame = jack_wave1_framebuffer();
        if (width != g_last_video_width || height != g_last_video_height) {
            memset(frame, 0, JACK_WAVE1_SCREEN_WIDTH * JACK_WAVE1_SCREEN_HEIGHT * sizeof(uint16_t));
            g_last_video_width = width;
            g_last_video_height = height;
        }
        const unsigned left = (JACK_WAVE1_SCREEN_WIDTH - width) / 2u;
        const unsigned top = (JACK_WAVE1_SCREEN_HEIGHT - height) / 2u;
        const uint16_t *source = static_cast<const uint16_t *>(data);
        for (unsigned y = 0; y < height; ++y) {
            memcpy(frame + (top + y) * JACK_WAVE1_SCREEN_WIDTH + left,
                   source + y * width, width * sizeof(uint16_t));
        }
        g_last_draw_us = time_us_32() - draw_start;
        return;
    }
#endif
    unsigned out_w;
    unsigned out_h;
    if (width <= JACK_WAVE1_SCREEN_WIDTH && height <= JACK_WAVE1_SCREEN_HEIGHT) {
        unsigned scale_x = JACK_WAVE1_SCREEN_WIDTH / width;
        unsigned scale_y = JACK_WAVE1_SCREEN_HEIGHT / height;
        unsigned scale = scale_x < scale_y ? scale_x : scale_y;
        if (scale == 0) scale = 1;
        out_w = width * scale;
        out_h = height * scale;
    } else {
        out_w = JACK_WAVE1_SCREEN_WIDTH;
        out_h = static_cast<unsigned>((static_cast<uint64_t>(height) * out_w) / width);
        if (out_h > JACK_WAVE1_SCREEN_HEIGHT) {
            out_h = JACK_WAVE1_SCREEN_HEIGHT;
            out_w = static_cast<unsigned>((static_cast<uint64_t>(width) * out_h) / height);
        }
    }
    const unsigned left = (JACK_WAVE1_SCREEN_WIDTH - out_w) / 2u;
    const unsigned top = (JACK_WAVE1_SCREEN_HEIGHT - out_h) / 2u;
    uint16_t *frame = jack_wave1_framebuffer();
    if (width != g_last_video_width || height != g_last_video_height) {
        memset(frame, 0,
               JACK_WAVE1_SCREEN_WIDTH * JACK_WAVE1_SCREEN_HEIGHT * sizeof(uint16_t));
        g_last_video_width = width;
        g_last_video_height = height;
    }
    const size_t bytes_per_pixel = g_pixel_format == RETRO_PIXEL_FORMAT_XRGB8888 ? 4u : 2u;
    const size_t source_stride = pitch / bytes_per_pixel;
    for (unsigned y = 0; y < out_h; ++y) {
        const unsigned source_y = static_cast<unsigned>(
            (static_cast<uint64_t>(y) * height) / out_h);
        uint16_t *dest = frame + (top + y) * JACK_WAVE1_SCREEN_WIDTH + left;
        for (unsigned x = 0; x < out_w; ++x) {
            const unsigned source_x = static_cast<unsigned>(
                (static_cast<uint64_t>(x) * width) / out_w);
            dest[x] = convert_pixel(data, source_y * source_stride + source_x);
        }
    }
    const uint32_t draw_end = time_us_32();
    g_last_draw_us = draw_end - draw_start;
}
bool load_content(const char *path, void **data, size_t *size)
{
    FIL file;
    if (f_open(&file, path, FA_READ) != FR_OK) return false;
    const FSIZE_t length = f_size(&file);
    void *buffer = jack_psram_malloc(static_cast<size_t>(length));
    UINT read = 0;
    const bool ok = buffer != nullptr &&
        f_read(&file, buffer, static_cast<UINT>(length), &read) == FR_OK && read == length;
    f_close(&file);
    if (!ok) return false;
    *data = buffer;
    *size = static_cast<size_t>(length);
    return true;
}
}

int main()
{
#ifdef JACK_MEGADUCK_STACK_FIX032
    jack_megaduck_stack032_init();
#endif
    if (!jack_wave1_init(JACK_LR_APP_NAME)) for (;;) tight_loop_contents();
    printf("%s_BOOT version=%s uart=0 tx=2 rx=3 baud=115200\n",
           JACK_LR_LOG_PREFIX, JACK_LR_VERSION);
#ifdef JACK_MEGADUCK_STACK_FIX032
    jack_megaduck_stack032_report("runtime");
#endif
    if (!jack_wave1_memory_timing_ready()) {
        jack_wave1_show_error(JACK_LR_APP_NAME, "PSRAM timing unavailable");
        for (;;) tight_loop_contents();
    }
    jack_psram_heap_reset();
    if (!jack_wave1_mount_sd()) {
        jack_wave1_show_error(JACK_LR_APP_NAME, "SD mount failed");
        for (;;) tight_loop_contents();
    }

    jack_wave1_set_picker_auto_select_single(false);
    if (!jack_wave1_select_file(JACK_LR_PICKER_TITLE, JACK_LR_ROM_DIRECTORY,
                                JACK_LR_EXTENSIONS, g_rom_path, sizeof(g_rom_path))) {
        jack_wave1_show_error(JACK_LR_APP_NAME, "No compatible ROM found");
        for (;;) tight_loop_contents();
    }
    void *rom_data = nullptr;
    size_t rom_size = 0;
    if (!load_content(g_rom_path, &rom_data, &rom_size)) {
        jack_wave1_show_error(JACK_LR_APP_NAME, "ROM load/PSRAM failed");
        for (;;) tight_loop_contents();
    }
    printf("%s_ROM path=%s bytes=%lu psram_used=%lu\n", JACK_LR_LOG_PREFIX,
           g_rom_path, static_cast<unsigned long>(rom_size),
           static_cast<unsigned long>(jack_psram_heap_used()));

    retro_set_environment(environment);
    retro_set_video_refresh(video_refresh);
    retro_set_audio_sample(audio_sample);
    retro_set_audio_sample_batch(audio_batch);
    retro_set_input_poll(input_poll);
    retro_set_input_state(input_state);
    retro_init();
    retro_game_info game{};
    game.path = g_rom_path;
    game.data = rom_data;
    game.size = rom_size;
    if (!retro_load_game(&game)) {
        jack_wave1_show_error(JACK_LR_APP_NAME, "Core rejected ROM");
        for (;;) tight_loop_contents();
    }
    retro_system_av_info av{};
#ifdef JACK_MEGADUCK_STACK_FIX032
    jack_megaduck_stack032_report("loaded");
#endif
    retro_get_system_av_info(&av);
#ifdef JACK_WAVE1_HDMI_AUDIO
    if (!jack_wave1_audio_begin((unsigned)av.timing.sample_rate)) {
        jack_wave1_show_error(JACK_LR_APP_NAME, "Unsupported audio rate");
        for (;;) tight_loop_contents();
    }
#endif
#ifdef JACK_POKEMINI_DEFERRED_LOG
    jack_pokemini_log_start();
#endif
    printf("%s_START width=%u height=%u core_fps=%.3f sample_rate=%.1f psram_used=%lu\n",
           JACK_LR_LOG_PREFIX, av.geometry.base_width, av.geometry.base_height,
           av.timing.fps, av.timing.sample_rate,
           static_cast<unsigned long>(jack_psram_heap_used()));
#ifdef JACK_MEGADUCK_PC_SAMPLER
    megaduck_pc_profile_t profile{};
    const bool pc_sampler_ready =
        megaduck_pc_sampler_init(clock_get_hz(clk_sys));
    printf("MEGADUCK_PC_COUNTER ready=%u sample_hz=%u report_ms=5000 align=32 slots=1024 probes=16 top=%u frame=v8m-dcrs\n",
           pc_sampler_ready ? 1u : 0u, MEGADUCK_PC_SAMPLE_HZ,
           MEGADUCK_PC_TOP_COUNT);
    uint32_t profile_window_start = time_us_32();
#endif
#ifdef JACK_MEGADUCK_HALT015
    printf("MEGADUCK_HALT015_INIT enabled=%u mode=ppu-boundary max_batch=126 wake=vblank-only\n",
           jack_megaduck_halt015_enabled);
    uint32_t halt015_window_start = time_us_32();
#endif
#ifdef JACK_MEGADUCK_HALT016
    printf("MEGADUCK_HALT016_INIT enabled=%u mode=ppu-boundary-wide max_batch=510 wake=vblank-only\n",
           jack_megaduck_halt016_enabled);
    uint32_t halt016_window_start = time_us_32();
#endif
#ifdef JACK_MEGADUCK_HALT017
    printf("MEGADUCK_HALT017_INIT enabled=%u mode=idle-timer-apu-fast max_batch=510 wake=vblank-only\n",
           jack_megaduck_halt017_enabled);
    uint32_t halt017_window_start = time_us_32();
#ifdef JACK_MEGADUCK_PPU_DEADLINE022
    printf("MEGADUCK_PPU022_INIT states=3,5 max_batch=510 active_timer021=0\n");
#endif
#endif
#ifdef JACK_MEGADUCK_PROFILE038
    megaduck_profile038_init(clock_get_hz(clk_sys));
#endif
#ifdef JACK_MEGADUCK_PACE036
    printf("MEGADUCK_PACE036 init fps=4194304/70224 period_us=16742+46288/65536 quiet_keys=%u audio_flush=after_wait\n",
           JACK_MEGADUCK_QUIET_KEYS036);
    megaduck_pace036_begin(time_us_32());
#endif
#ifdef JACK_MEGADUCK_DIAG040
    megaduck_diag040_init();
#endif
#ifdef JACK_MEGADUCK_PROFILE049
    megaduck_profile049_init(clock_get_hz(clk_sys));
#endif
    g_perf.begin();
#ifdef JACK_MEGADUCK_TIMA037
    printf("MEGADUCK_TIMA037 init ie=05 tac=04 policy=before-edge fallback=generic\n");
#endif
    for (;;) {
#ifdef JACK_MEGADUCK_OPT047
        megaduck_log047_pump();
#endif
#ifdef JACK_MEGADUCK_DIAG040
        megaduck_diag040_poll();
#endif
        const uint32_t frame = jack_wave1_frame_counter();
#ifdef JACK_MEGADUCK_PROFILE049
        megaduck_profile049_frame_begin();
#endif
        const uint32_t run_start = time_us_32();
        g_last_draw_us = 0;
        retro_run();
#ifdef JACK_MEGADUCK_TIMA037
        const uint32_t retro_end = time_us_32();
#endif
#ifdef JACK_MEGADUCK_PROFILE049
        megaduck_profile049_frame_stop();
#endif
#ifdef JACK_MEGADUCK_PACE036

        const uint32_t pace_wait_us = megaduck_pace036_wait();
#endif
#ifdef JACK_MEGADUCK_AUDIO034
#ifdef JACK_MEGADUCK_TIMA037
        const uint32_t audio_start = time_us_32();
#endif
        jack_wave1_audio_end_frame();
#endif
        const uint32_t run_end = time_us_32();
        const uint32_t wait_start = run_end;
#ifdef JACK_POKEMINI_DEFERRED_LOG
        jack_pokemini_log_pump();
#endif
#ifdef JACK_WAVE1_HDMI_AUDIO
        const bool waited = jack_wave1_audio_wait();
#else
        const bool waited = jack_wave1_wait_frame_boundary(frame);
#endif
        const uint32_t wait_end = time_us_32();
        const uint32_t run_us = run_end - run_start
#ifdef JACK_MEGADUCK_PACE036
            - pace_wait_us
#endif
            ;
        const uint32_t core_us = run_us >= g_last_draw_us
            ? run_us - g_last_draw_us : 0;
        jack_wave1_usb_service();
#ifdef JACK_MEGADUCK_DIAG040
        megaduck_diag040_frame_done();
#endif
        g_perf.sample(JACK_LR_LOG_PREFIX, core_us, g_last_draw_us,
                      wait_end - wait_start
#ifdef JACK_MEGADUCK_PACE036
                          + pace_wait_us
#endif
                          ,
                      static_cast<uint16_t>(g_player_buttons[0] | g_player_buttons[1]),
#ifdef JACK_MEGADUCK_PACE036
                      pace_wait_us ? "paced" : (waited ? "running" : "late"),
#else
                      waited ? "running" : "late",
#endif
                      jack_wave1_perf_stdout
#ifdef JACK_MEGADUCK_TIMA037
                      , retro_end - run_start >= g_last_draw_us
                          ? retro_end - run_start - g_last_draw_us : 0u,
                      run_end - audio_start
#endif
                      );
#ifdef JACK_WAVE1_HDMI_AUDIO
        jack_wave1_audio_report();
#endif
#ifdef JACK_MEGADUCK_OPT047
        megaduck_log047_pump();
#endif
#ifdef JACK_POKEMINI_DEFERRED_LOG
        jack_pokemini_log_report();
        jack_pokemini_log_pump();
#endif
#ifndef JACK_MEGADUCK_AUDIO034
#ifdef JACK_MEGADUCK_HALT015
        if (static_cast<uint32_t>(wait_end - halt015_window_start) >=
            JACK_WAVE1_PERF_INTERVAL_US) {
            jack_megaduck_halt015_stats_t stats{};
            jack_megaduck_halt015_take(&stats);
            printf("MEGADUCK_HALT015 enabled=%u elapsed_us=%lu batches=%lu guest_cycles=%lu max_batch=%lu\n",
                   jack_megaduck_halt015_enabled,
                   static_cast<unsigned long>(wait_end - halt015_window_start),
                   static_cast<unsigned long>(stats.batches),
                   static_cast<unsigned long>(stats.guest_cycles),
                   static_cast<unsigned long>(stats.max_batch));
            halt015_window_start = wait_end;
        }
#endif
#ifdef JACK_MEGADUCK_HALT016
        if (static_cast<uint32_t>(wait_end - halt016_window_start) >=
            JACK_WAVE1_PERF_INTERVAL_US) {
            jack_megaduck_halt016_stats_t stats{};
            jack_megaduck_halt016_take(&stats);
            printf("MEGADUCK_HALT016 enabled=%u elapsed_us=%lu batches=%lu guest_cycles=%lu wide_batches=%lu max_batch=%lu\n",
                   jack_megaduck_halt016_enabled,
                   static_cast<unsigned long>(wait_end - halt016_window_start),
                   static_cast<unsigned long>(stats.batches),
                   static_cast<unsigned long>(stats.guest_cycles),
                   static_cast<unsigned long>(stats.wide_batches),
                   static_cast<unsigned long>(stats.max_batch));
            halt016_window_start = wait_end;
        }
#endif
#ifdef JACK_MEGADUCK_HALT017
        if (static_cast<uint32_t>(wait_end - halt017_window_start) >=
            JACK_WAVE1_PERF_INTERVAL_US) {
            jack_megaduck_halt017_stats_t stats{};
            jack_megaduck_halt017_take(&stats);
            printf("MEGADUCK_HALT017 enabled=%u elapsed_us=%lu batches=%lu guest_cycles=%lu wide_batches=%lu max_batch=%lu timer_fast=%lu timer_ticks=%lu apu_fast=%lu lfsr_steps=%lu\n",
                   jack_megaduck_halt017_enabled,
                   static_cast<unsigned long>(wait_end - halt017_window_start),
                   static_cast<unsigned long>(stats.batches),
                   static_cast<unsigned long>(stats.guest_cycles),
                   static_cast<unsigned long>(stats.wide_batches),
                   static_cast<unsigned long>(stats.max_batch),
                   static_cast<unsigned long>(stats.timer_fast_calls),
                   static_cast<unsigned long>(stats.timer_ticks),
                   static_cast<unsigned long>(stats.apu_fast_calls),
                   static_cast<unsigned long>(stats.lfsr_steps));
#ifdef JACK_MEGADUCK_PPU_DEADLINE022
            printf("MEGADUCK_PPU022 batches=%lu guest_cycles=%lu\n",
                   static_cast<unsigned long>(stats.ppu_batches),
                   static_cast<unsigned long>(stats.ppu_cycles));
#endif
            halt017_window_start = wait_end;
#ifdef JACK_MEGADUCK_STACK_FIX032
            jack_megaduck_stack032_report("running");
#endif
        }
#endif
#endif
#ifdef JACK_MEGADUCK_PROFILE014
        megaduck_probe014_report();
#endif
#ifdef JACK_MEGADUCK_PROFILE038
        megaduck_profile038_poll(time_us_32());
#endif
#ifdef JACK_MEGADUCK_PROFILE049
        megaduck_profile049_frame_commit(retro_end - run_start >= g_last_draw_us
            ? retro_end - run_start - g_last_draw_us : 0u);
        megaduck_profile049_poll(time_us_32());
#endif
#ifdef JACK_MEGADUCK_PC_SAMPLER
        if (static_cast<uint32_t>(wait_end - profile_window_start) >= JACK_WAVE1_PERF_INTERVAL_US) {
            megaduck_pc_sampler_take(&profile);
            printf("MEGADUCK_PC total=%lu overflow=%lu",
                   static_cast<unsigned long>(profile.total),
                   static_cast<unsigned long>(profile.overflow));
            for (unsigned i = 0; i < MEGADUCK_PC_TOP_COUNT; i++) {
                if (!profile.hits[i]) break;
                printf(" p%u=%08lx/%lu", i,
                       static_cast<unsigned long>(profile.pc[i]),
                       static_cast<unsigned long>(profile.hits[i]));
            }
            printf("\n");
            profile_window_start = wait_end;
        }
#endif
    }
}
