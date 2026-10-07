#include "gb.h"
#include <string.h>
#ifdef JACK_MEGADUCK_TIMING_PROFILE
#include "megaduck_profile.h"
#endif
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <windows.h>
#else
#include <sys/time.h>
#endif

static const unsigned TAC_TRIGGER_BITS[] JACK_MEGADUCK_HOT_TABLE(TAC_TRIGGER_BITS) = {512, 8, 32, 128};

#ifdef JACK_MEGADUCK_TIMING_PROFILE
typedef struct {
    GB_megaduck_timing_profile_t counters;
    uint32_t random;
    uint32_t countdown;
    uint32_t marks[8];
} megaduck_timing_profile_state_t;

static megaduck_timing_profile_state_t megaduck_timing_profile;

static inline uint32_t megaduck_cycle_counter(void)
{
    return *(volatile uint32_t *)0xe0001004u;
}

static inline uint32_t megaduck_profile_delta(uint32_t begin, uint32_t end)
{
    const uint32_t elapsed = end - begin;
    return elapsed > megaduck_timing_profile.counters.read_cycles
        ? elapsed - megaduck_timing_profile.counters.read_cycles : 0;
}

static noinline void megaduck_profile_commit(void)
{
    GB_megaduck_timing_profile_t *const counters =
        &megaduck_timing_profile.counters;
    const uint32_t *const marks = megaduck_timing_profile.marks;
    counters->head_cycles += megaduck_profile_delta(marks[0], marks[1]);
    counters->timer_cycles += megaduck_profile_delta(marks[1], marks[2]);
    counters->bookkeeping_cycles += megaduck_profile_delta(marks[2], marks[3]);
    counters->joypad_cycles += megaduck_profile_delta(marks[3], marks[4]);
    counters->apu_cycles += megaduck_profile_delta(marks[4], marks[5]);
    counters->display_cycles += megaduck_profile_delta(marks[5], marks[6]);
    counters->tail_cycles += megaduck_profile_delta(marks[6], marks[7]);
    counters->samples++;
}

static noinline void megaduck_profile_commit_early(void)
{
    GB_megaduck_timing_profile_t *const counters =
        &megaduck_timing_profile.counters;
    const uint32_t *const marks = megaduck_timing_profile.marks;
    counters->head_cycles += megaduck_profile_delta(marks[0], marks[1]);
    counters->timer_cycles += megaduck_profile_delta(marks[1], marks[2]);
    counters->bookkeeping_cycles += megaduck_profile_delta(marks[2], marks[3]);
    counters->samples++;
    counters->early_returns++;
}

void GB_megaduck_timing_profile_init(void)
{
    volatile uint32_t *const dwt_ctrl = (volatile uint32_t *)0xe0001000u;
    volatile uint32_t *const dwt_cyccnt = (volatile uint32_t *)0xe0001004u;
    volatile uint32_t *const demcr = (volatile uint32_t *)0xe000edfcu;
    *demcr |= 1u << 24;
    *dwt_ctrl |= 1u;
    *dwt_cyccnt = 0u;
    __asm volatile("dsb\n\tisb" ::: "memory");

    uint32_t best_read_cycles = UINT32_MAX;
    const uint32_t first = *dwt_cyccnt;
    for (unsigned i = 0; i < 32; i++) {
        const uint32_t begin = *dwt_cyccnt;
        const uint32_t end = *dwt_cyccnt;
        const uint32_t elapsed = end - begin;
        if (elapsed < best_read_cycles) best_read_cycles = elapsed;
    }
    const uint32_t last = *dwt_cyccnt;

    memset(&megaduck_timing_profile, 0, sizeof(megaduck_timing_profile));
    megaduck_timing_profile.counters.ready =
        ((*dwt_ctrl & 1u) != 0u && last != first) ? 1u : 0u;
    megaduck_timing_profile.counters.read_cycles = best_read_cycles;
    megaduck_timing_profile.random = 0x6d2b79f5u;
    megaduck_timing_profile.countdown = 4096u;
}

void GB_megaduck_timing_profile_take(GB_megaduck_timing_profile_t *result)
{
    if (!result) return;
    *result = megaduck_timing_profile.counters;
    const uint32_t ready = megaduck_timing_profile.counters.ready;
    const uint32_t read_cycles = megaduck_timing_profile.counters.read_cycles;
    memset(&megaduck_timing_profile.counters, 0,
           sizeof(megaduck_timing_profile.counters));
    megaduck_timing_profile.counters.ready = ready;
    megaduck_timing_profile.counters.read_cycles = read_cycles;
}
#endif

#ifndef GB_DISABLE_TIMEKEEPING
static int64_t get_nanoseconds(void)
{
#ifndef _WIN32
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_usec) * 1000 + now.tv_sec * 1000000000L;
#else
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    return (((int64_t)time.dwHighDateTime << 32) | time.dwLowDateTime) * 100L;
#endif
}

static void nsleep(uint64_t nanoseconds)
{
#ifndef _WIN32
    struct timespec sleep = {0, nanoseconds};
    nanosleep(&sleep, NULL);
#else
    HANDLE timer;
    LARGE_INTEGER time;
    timer = CreateWaitableTimer(NULL, true, NULL);
    time.QuadPart = -(nanoseconds / 100L);
    SetWaitableTimer(timer, &time, 0, NULL, NULL, false);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
#endif
}

bool GB_timing_sync_turbo(GB_gameboy_t *gb)
{
#ifndef GB_DISABLE_DEBUGGER
    if (unlikely(gb->backstep_instructions)) return false;
#endif
    if (!gb->turbo_dont_skip) {
        int64_t nanoseconds = get_nanoseconds();
        if (nanoseconds <= gb->last_sync + (1000000000LL * LCDC_PERIOD / GB_get_clock_rate(gb))) {
            return true;
        }
        gb->last_sync = nanoseconds;
    }
    return false;
}

void GB_timing_sync(GB_gameboy_t *gb)
{
#ifndef GB_DISABLE_DEBUGGER
    if (unlikely(gb->backstep_instructions)) return;
#endif
    /* Prevent syncing if not enough time has passed.*/
    if (gb->cycles_since_last_sync < LCDC_PERIOD / 3) return;

    if (gb->turbo) {
        gb->cycles_since_last_sync = 0;
        if (gb->update_input_hint_callback) {
            gb->update_input_hint_callback(gb);
        }
        return;
    }
    
    uint64_t target_nanoseconds = gb->cycles_since_last_sync * 1000000000LL / 2 / GB_get_clock_rate(gb); /* / 2 because we use 8MHz units */
    int64_t nanoseconds = get_nanoseconds();
    int64_t time_to_sleep = target_nanoseconds + gb->last_sync - nanoseconds;
    if (time_to_sleep > 0 && time_to_sleep < LCDC_PERIOD * 1200000000LL / GB_get_clock_rate(gb)) { // +20% to be more forgiving
        nsleep(time_to_sleep);
        gb->last_sync += target_nanoseconds;
    }
    else {
        if (time_to_sleep < 0 && -time_to_sleep < LCDC_PERIOD * 1200000000LL / GB_get_clock_rate(gb)) {
            // We're running a bit too slow, but the difference is small enough,
            // just skip this sync and let it even out
            return;
        }
        gb->last_sync = nanoseconds;
    }

    gb->cycles_since_last_sync = 0;
    if (gb->update_input_hint_callback) {
        gb->update_input_hint_callback(gb);
    }
}
#else

bool GB_timing_sync_turbo(GB_gameboy_t *gb)
{
    return false;
}

void GB_timing_sync(GB_gameboy_t *gb)
{
#ifndef GB_DISABLE_DEBUGGER
    if (unlikely(gb->backstep_instructions)) return;
#endif
    if (gb->cycles_since_last_sync < LCDC_PERIOD / 3) return;
    gb->cycles_since_last_sync = 0;

    gb->cycles_since_last_sync = 0;
    if (gb->update_input_hint_callback) {
        gb->update_input_hint_callback(gb);
    }
    return;
}

#endif

#define IR_DECAY 31500
#define IR_WARMUP 19900
#define IR_THRESHOLD 240
#define IR_MAX IR_THRESHOLD * 2 + IR_DECAY + 268

static JACK_MEGADUCK_HOT_FUNC void ir_run(GB_gameboy_t *gb, uint32_t cycles)
{
    /* TODO: the way this thing works makes the CGB IR port behave inaccurately when used together with HUC1/3 IR ports*/
    if ((gb->model > GB_MODEL_CGB_E || !gb->cgb_mode) && gb->cartridge_type->mbc_type != GB_HUC1 && gb->cartridge_type->mbc_type != GB_HUC3) return;
    bool is_sensing = (gb->io_registers[GB_IO_RP] & 0xC0) == 0xC0 ||
                       (gb->cartridge_type->mbc_type == GB_HUC1 && gb->huc1.ir_mode) ||
                       (gb->cartridge_type->mbc_type == GB_HUC3 && gb->huc3.mode == 0xE);
    if (is_sensing && (gb->infrared_input || gb->cart_ir || (gb->io_registers[GB_IO_RP] & 1))) {
        gb->ir_sensor += cycles;
        if (gb->ir_sensor > IR_MAX) {
            gb->ir_sensor = IR_MAX;
        }
        
        gb->effective_ir_input = gb->ir_sensor >=  IR_WARMUP + IR_THRESHOLD && gb->ir_sensor <= IR_WARMUP + IR_THRESHOLD + IR_DECAY;
    }
    else {
        unsigned target = is_sensing? IR_WARMUP : 0;
        if (gb->ir_sensor < target) {
            gb->ir_sensor += cycles;
        }
        else if (gb->ir_sensor <= target + cycles) {
            gb->ir_sensor = target;
        }
        else {
            gb->ir_sensor -= cycles;
        }
        gb->effective_ir_input = false;
    }
    
}

static JACK_MEGADUCK_HOT_FUNC void advance_tima_state_machine(GB_gameboy_t *gb)
{
    if (gb->tima_reload_state == GB_TIMA_RELOADED) {
        gb->tima_reload_state = GB_TIMA_RUNNING;
    }
    else if (gb->tima_reload_state == GB_TIMA_RELOADING) {
        gb->io_registers[GB_IO_IF] |= 4;
        gb->tima_reload_state = GB_TIMA_RELOADED;
    }
}

static void increase_tima(GB_gameboy_t *gb)
{
    gb->io_registers[GB_IO_TIMA]++;
    if (gb->io_registers[GB_IO_TIMA] == 0) {
        gb->io_registers[GB_IO_TIMA] = gb->io_registers[GB_IO_TMA];
        gb->tima_reload_state = GB_TIMA_RELOADING;
    }
}

void GB_serial_master_edge(GB_gameboy_t *gb)
{
    if (gb->printer_callback) {
        unsigned ticks = 1 << gb->serial_mask;
        if (unlikely((gb->printer.command_state || gb->printer.bits_received))) {
            gb->printer.idle_time +=ticks;
        }
        if (unlikely(gb->printer.time_remaining)) {
            if (gb->printer.time_remaining <= ticks) {
                gb->printer.time_remaining = 0;
                if (gb->printer_done_callback) {
                    gb->printer_done_callback(gb);
                }
            }
            else {
                gb->printer.time_remaining -= ticks;
            }
        }
    }
    
    gb->serial_master_clock ^= true;
    
    if (!gb->serial_master_clock && (gb->io_registers[GB_IO_SC] & 0x81) == 0x81) {

        #ifdef DUCK_IMPROVED_SIO_TIMING
        // Changed vs SameBoy: SIO code now does *NOT* expect first bit to have
        // been clocked as soon as SC was written to (if 0x81).
        //
        // So the order of operations here has to change:
        // - Clock the bit in first (instead of last)
        // - Then do remainder of the SIO handling that was previously done
        // - And do *NOT* clock a bit at the end of the handling here
        if (gb->serial_count < 8) {
            /* Still more bits to send */
            if (gb->serial_transfer_bit_start_callback) {
                gb->serial_transfer_bit_start_callback(gb, gb->io_registers[GB_IO_SB] & 0x80);
            }
        }
        #endif

        gb->serial_count++;
        if (gb->serial_count == 8) {
            gb->serial_count = 0;
            gb->io_registers[GB_IO_SC] &= ~0x80;
            gb->io_registers[GB_IO_IF] |= 8;
        }
        
        gb->io_registers[GB_IO_SB] <<= 1;
        
        if (gb->serial_transfer_bit_end_callback) {
            gb->io_registers[GB_IO_SB] |= gb->serial_transfer_bit_end_callback(gb);
        }
        else {
            gb->io_registers[GB_IO_SB] |= 1;
        }

        #ifndef DUCK_IMPROVED_SIO_TIMING
        if (gb->serial_count) {
            /* Still more bits to send */
            if (gb->serial_transfer_bit_start_callback) {
                gb->serial_transfer_bit_start_callback(gb, gb->io_registers[GB_IO_SB] & 0x80);
            }
        }
        #endif

    }
}


JACK_MEGADUCK_HOT_FUNC void GB_set_internal_div_counter(GB_gameboy_t *gb, uint16_t value)
{
    /* TIMA increases when a specific high-bit becomes a low-bit. */
    uint16_t triggers = gb->div_counter & ~value;
    if ((gb->io_registers[GB_IO_TAC] & 4) && (triggers & TAC_TRIGGER_BITS[gb->io_registers[GB_IO_TAC] & 3])) {
        increase_tima(gb);
    }
    
    if (triggers & gb->serial_mask) {
        GB_serial_master_edge(gb);
    }
    
    /* TODO: Can switching to double speed mode trigger an event? */
    uint16_t apu_bit = gb->cgb_double_speed? 0x2000 : 0x1000;
    if (triggers & apu_bit) {
        GB_apu_div_event(gb);
    }
    else {
        uint16_t secondary_triggers = ~gb->div_counter & value;
        if (secondary_triggers & apu_bit) {
            GB_apu_div_secondary_event(gb);
        }
    }
    gb->div_counter = value;
}

#ifdef JACK_MEGADUCK_HALT017
uint32_t jack_megaduck_halt017_timer_fast_calls;
uint32_t jack_megaduck_halt017_timer_ticks;
static bool megaduck_halt017_timers_run(GB_gameboy_t *gb, GB_cycle_count_t cycles)
{
#ifdef JACK_MEGADUCK_AUDIO024
    if (!jack_megaduck_halt017_enabled || !gb->halted || cycles < 6 || cycles > 510 ||
        gb->stopped || gb->cgb_double_speed ||
        gb->tima_reload_state != GB_TIMA_RUNNING ||
#ifdef JACK_MEGADUCK_TIMA037
        ((gb->io_registers[GB_IO_TAC] & 4u) &&
         ((gb->io_registers[GB_IO_TAC] & 7u) != 4u ||
          (gb->interrupt_enable & 0x1fu) != 5u || (gb->div_counter & 3u))) ||
#else
        (gb->io_registers[GB_IO_TAC] & 4u) ||
#endif
        (gb->io_registers[GB_IO_SC] & 0x81u) == 0x81u ||
        gb->serial_mask != 0x80 ||
        gb->serial_transfer_bit_start_callback ||
        gb->serial_transfer_bit_end_callback || gb->printer_callback ||
        !gb->apu.apu_cycles_in_2mhz ||
        gb->div_state != 2 || gb->div_cycles > 0 || gb->div_cycles < -4) {
        return false;
    }
#else
    if (!jack_megaduck_halt017_enabled || !gb->halted || cycles < 6 || cycles > 510 ||
        gb->stopped || gb->cgb_double_speed ||
        gb->tima_reload_state != GB_TIMA_RUNNING ||
        (gb->io_registers[GB_IO_TAC] & 4u) ||
        (gb->io_registers[GB_IO_SC] & 0x81u) == 0x81u ||
        gb->serial_mask != 0x80 ||
        gb->serial_transfer_bit_start_callback ||
        gb->serial_transfer_bit_end_callback || gb->printer_callback ||
        gb->apu.global_enable || gb->apu_output.sample_rate ||
        !gb->apu.apu_cycles_in_2mhz ||
        gb->div_state != 2 || gb->div_cycles > 0 || gb->div_cycles < -4) {
        return false;
    }
#endif

    int32_t accumulated = gb->div_cycles + cycles;
    unsigned ticks = accumulated > 0 ? (unsigned)(accumulated + 3) / 4u : 0;
    uint32_t old_div = gb->div_counter;
    uint32_t end_div = old_div + ticks * 4u;
    if ((end_div >> 12) - (old_div >> 12) > 1u) return false;
#ifdef JACK_MEGADUCK_TIMA037
    /* No TIMA state can change inside this batch. Reject BEFORE modifying
     * state if a caller would cross a 1024-cycle falling edge, including
     * DIV wrap. The generic path owns every TIMA edge and delayed IRQ. */
    if ((gb->io_registers[GB_IO_TAC] & 4u) &&
        (end_div >> 10) != (old_div >> 10)) return false;
#endif

    gb->div_cycles = accumulated - (int32_t)(ticks * 4u);
    if (ticks) {
#ifdef JACK_MEGADUCK_AUDIO024
        /* AUDIO023 stops every HALT batch before the next PCM/APU/DIV event.
         * Advance only the sample phase here; the original two-cycle step
         * services the boundary through the normal state machine. */
        gb->apu_output.sample_cycles +=
            ticks * (gb->apu_output.sample_rate << 2);
#endif
        unsigned apu_boundaries = (end_div >> 12) - (old_div >> 12);
        if (apu_boundaries) {
            uint32_t boundary = ((old_div >> 12) + 1u) << 12;
            unsigned event_tick = (unsigned)(boundary - old_div) / 4u;
            gb->apu.apu_cycles += (event_tick - 1u) * 2u;
            gb->div_counter = (uint16_t)(boundary - 4u);
            if (boundary & 0x1000u) {
                GB_apu_div_secondary_event(gb);
            }
            else {
                GB_apu_div_event(gb);
            }
            gb->apu.apu_cycles += (ticks - event_tick + 1u) * 2u;
        }
        else {
            gb->apu.apu_cycles += ticks * 2u;
        }

        unsigned serial_edges = (end_div >> 8) - (old_div >> 8);
        if (serial_edges & 1u) gb->serial_master_clock ^= true;
        gb->div_counter = (uint16_t)end_div;
    }

    jack_megaduck_halt017_timer_fast_calls++;
    jack_megaduck_halt017_timer_ticks += ticks;
#ifdef JACK_MEGADUCK_NATIVE_REPLAY
    jack_megaduck_halt017_native_timer_fast_calls++;
    jack_megaduck_halt017_native_timer_ticks += ticks;
#endif
    return true;
}
#endif

#ifdef JACK_MEGADUCK_ACTIVE_TIMER021
#ifdef JACK_MEGADUCK_NATIVE_REPLAY
uint64_t jack_megaduck_active021_native_calls;
uint64_t jack_megaduck_active021_native_ticks;
#endif

/*
 * Catch keeps TIMA, serial transfers and audio output disabled.  The generic
 * divider state machine still wakes for almost every active CPU bus cycle,
 * even though the only observable work is advancing DIV, the idle serial
 * clock and the powered-off APU divider.  Preserve the exact event ordering
 * used by the HALT017 path, but admit small non-HALT batches as well.
 * Unsupported states always retain the original state machine.
 */
static JACK_MEGADUCK_HOT_FUNC bool megaduck_active021_timers_run(
    GB_gameboy_t *gb, GB_cycle_count_t cycles)
{
    if (gb->halted || gb->stopped || gb->cgb_double_speed ||
        gb->tima_reload_state != GB_TIMA_RUNNING ||
        (gb->io_registers[GB_IO_TAC] & 4u) ||
        (gb->io_registers[GB_IO_SC] & 0x81u) == 0x81u ||
        gb->serial_mask != 0x80 ||
        gb->serial_transfer_bit_start_callback ||
        gb->serial_transfer_bit_end_callback || gb->printer_callback ||
        gb->apu.global_enable || gb->apu_output.sample_rate ||
        !gb->apu.apu_cycles_in_2mhz ||
        gb->div_state != 2 || gb->div_cycles > 0 || gb->div_cycles < -4) {
        return false;
    }

    int32_t accumulated = gb->div_cycles + cycles;
    unsigned ticks = accumulated > 0 ? (unsigned)(accumulated + 3) / 4u : 0;
    uint32_t old_div = gb->div_counter;
    uint32_t end_div = old_div + ticks * 4u;
    if ((end_div >> 12) - (old_div >> 12) > 1u) return false;

    gb->div_cycles = accumulated - (int32_t)(ticks * 4u);
    if (ticks) {
        unsigned apu_boundaries = (end_div >> 12) - (old_div >> 12);
        if (apu_boundaries) {
            uint32_t boundary = ((old_div >> 12) + 1u) << 12;
            unsigned event_tick = (unsigned)(boundary - old_div) / 4u;
            gb->apu.apu_cycles += (event_tick - 1u) * 2u;
            gb->div_counter = (uint16_t)(boundary - 4u);
            if (boundary & 0x1000u) {
                GB_apu_div_secondary_event(gb);
            }
            else {
                GB_apu_div_event(gb);
            }
            gb->apu.apu_cycles += (ticks - event_tick + 1u) * 2u;
        }
        else {
            gb->apu.apu_cycles += ticks * 2u;
        }

        unsigned serial_edges = (end_div >> 8) - (old_div >> 8);
        if (serial_edges & 1u) gb->serial_master_clock ^= true;
        gb->div_counter = (uint16_t)end_div;
    }

#ifdef JACK_MEGADUCK_NATIVE_REPLAY
    jack_megaduck_active021_native_calls++;
    jack_megaduck_active021_native_ticks += ticks;
#endif
    return true;
}
#endif
static JACK_MEGADUCK_HOT_FUNC void timers_run(GB_gameboy_t *gb, GB_cycle_count_t cycles)
{
    if (gb->stopped) {
        if (GB_is_cgb(gb)) {
            gb->apu.apu_cycles += 1 << !gb->cgb_double_speed;
        }
        gb->apu_output.sample_cycles += (gb->apu_output.sample_rate << !gb->cgb_double_speed) << 1;
        return;
    }
    
    GB_STATE_MACHINE(gb, div, cycles, 1) {
        GB_STATE(gb, div, 1);
        GB_STATE(gb, div, 2);
    }
    
    GB_SLEEP(gb, div, 1, 3);
    while (true) {
        advance_tima_state_machine(gb);
        GB_set_internal_div_counter(gb, gb->div_counter + 4);
        gb->apu.apu_cycles += 1 << !gb->cgb_double_speed;
        gb->apu_output.sample_cycles += (gb->apu_output.sample_rate << !gb->cgb_double_speed) << 1;
        GB_SLEEP(gb, div, 2, 4);
    }
}

void GB_set_rtc_mode(GB_gameboy_t *gb, GB_rtc_mode_t mode)
{
    if (gb->rtc_mode != mode) {
        gb->rtc_mode = mode;
        gb->rtc_cycles = 0;
        gb->last_rtc_second = time(NULL);
    }
}


void GB_set_rtc_multiplier(GB_gameboy_t *gb, double multiplier)
{
    if (multiplier == 1) {
        gb->rtc_second_length = 0;
        return;
    }
    
    gb->rtc_second_length = GB_get_unmultiplied_clock_rate(gb) * 2 * multiplier;
}

void GB_rtc_set_time(GB_gameboy_t *gb, uint64_t current_time)
{
    if (gb->cartridge_type->mbc_type == GB_HUC3) {
        while (gb->last_rtc_second / 60 < current_time / 60) {
            gb->last_rtc_second += 60;
            gb->huc3.minutes++;
            if (gb->huc3.minutes == 60 * 24) {
                gb->huc3.days++;
                gb->huc3.minutes = 0;
            }
        }
        return;
    }
    
    bool running = false;
    if (gb->cartridge_type->mbc_type == GB_TPP1) {
        running = gb->tpp1_mr4 & 0x4;
    }
    else {
        running = (gb->rtc_real.high & 0x40) == 0;
    }
    
    if (!running) return;
    
    while (gb->last_rtc_second + 60 * 60 * 24 < current_time) {
        gb->last_rtc_second += 60 * 60 * 24;
        if (gb->cartridge_type->mbc_type == GB_TPP1) {
            if (++gb->rtc_real.tpp1.weekday == 7) {
                gb->rtc_real.tpp1.weekday = 0;
                if (++gb->rtc_real.tpp1.weeks == 0) {
                    gb->tpp1_mr4 |= 8; /* Overflow bit */
                }
            }
        }
        else if (++gb->rtc_real.days == 0) {
            if (gb->rtc_real.high & 1) { /* Bit 8 of days*/
                gb->rtc_real.high |= 0x80; /* Overflow bit */
            }
            
            gb->rtc_real.high ^= 1;
        }
    }
    
    while (gb->last_rtc_second < current_time) {
        gb->last_rtc_second++;
        if (++gb->rtc_real.seconds != 60) continue;
        gb->rtc_real.seconds = 0;
        
        if (++gb->rtc_real.minutes != 60) continue;
        gb->rtc_real.minutes = 0;
        
        if (gb->cartridge_type->mbc_type == GB_TPP1) {
            if (++gb->rtc_real.tpp1.hours != 24) continue;
            gb->rtc_real.tpp1.hours = 0;
            if (++gb->rtc_real.tpp1.weekday != 7) continue;
            gb->rtc_real.tpp1.weekday = 0;
            if (++gb->rtc_real.tpp1.weeks == 0) {
                gb->tpp1_mr4 |= 8; /* Overflow bit */
            }
        }
        else {
            if (++gb->rtc_real.hours != 24) continue;
            gb->rtc_real.hours = 0;
            
            if (++gb->rtc_real.days != 0) continue;
            
            if (gb->rtc_real.high & 1) { /* Bit 8 of days*/
                gb->rtc_real.high |= 0x80; /* Overflow bit */
            }
            
            gb->rtc_real.high ^= 1;
        }
    }
}

static JACK_MEGADUCK_HOT_FUNC void rtc_run(GB_gameboy_t *gb, GB_cycle_count_t cycles)
{
    if (likely(gb->cartridge_type->mbc_type != GB_HUC3 && !gb->cartridge_type->has_rtc)) return;
    gb->rtc_cycles += cycles;
    time_t current_time = 0;
    uint32_t rtc_second_length = unlikely(gb->rtc_second_length)? gb->rtc_second_length : GB_get_unmultiplied_clock_rate(gb) * 2;
    
    switch (gb->rtc_mode) {
        case GB_RTC_MODE_SYNC_TO_HOST:
            // Sync in a 1/32s resolution
            if (gb->rtc_cycles < GB_get_unmultiplied_clock_rate(gb) / 16) return;
            gb->rtc_cycles -= GB_get_unmultiplied_clock_rate(gb) / 16;
            current_time = time(NULL);
            break;
        case GB_RTC_MODE_ACCURATE:
            if (gb->cartridge_type->mbc_type != GB_HUC3 && (gb->rtc_real.high & 0x40)) {
                gb->rtc_cycles -= cycles;
                return;
            }
            if (gb->rtc_cycles < rtc_second_length) return;
            gb->rtc_cycles -= rtc_second_length;
            current_time = gb->last_rtc_second + 1;
            break;
    }

    GB_rtc_set_time(gb, current_time);
}

static void camera_run(GB_gameboy_t *gb, GB_cycle_count_t cycles)
{
    /* Do we have a camera? */
    if (likely(gb->cartridge_type->mbc_type != GB_CAMERA)) return;

    /* The camera mapper uses the PHI pin to clock itself */

    /* PHI does not run in halt nor stop mode */
    if (unlikely(gb->halted || gb->stopped)) return;

    /* Only every other PHI is used (as the camera wants a 512KiHz clock) */
    gb->camera_alignment += cycles;

    /* Is the camera processing an image? */
    if (likely(gb->camera_countdown == 0)) return;

    gb->camera_countdown -= cycles;
    if (gb->camera_countdown <= 0) {
        gb->camera_countdown = 0;
        GB_camera_updated(gb);
    }
}


// "cycles" param here in SameBoy looks like it means 4.n MHz T-States
static void megaduck_laptop_peripheral_run(GB_gameboy_t *gb, GB_cycle_count_t cycles)
{
    if (unlikely( !GB_megaduck_laptop_is_enabled(gb) )) return;

    gb->megaduck_laptop.t_states_elapsed     += cycles;
    gb->megaduck_laptop.t_states_till_update -= cycles;
    if (gb->megaduck_laptop.t_states_till_update <= 0) {
        // Called after 512 T-States have elapsed
        // Roughly matches serial clock timing in GB "normal" speed
        GB_megaduck_laptop_peripheral_update(gb, cycles);
    }
}


JACK_MEGADUCK_HOT_FUNC void GB_advance_cycles(GB_gameboy_t *gb, GB_cycle_count_t cycles)
{
#ifdef JACK_MEGADUCK_TIMING_PROFILE
    bool megaduck_profile_sample = false;
    if (--megaduck_timing_profile.countdown == 0) {
        megaduck_timing_profile.random =
            megaduck_timing_profile.random * 1664525u + 1013904223u;
        megaduck_timing_profile.countdown =
            4096u + (megaduck_timing_profile.random & 4095u);
        megaduck_profile_sample =
            megaduck_timing_profile.counters.ready != 0;
        if (megaduck_profile_sample) {
            megaduck_timing_profile.marks[0] = megaduck_cycle_counter();
        }
    }
#endif
    if (unlikely(gb->speed_switch_countdown)) {
        if (gb->speed_switch_countdown == cycles) {
            gb->cgb_double_speed ^= true;
            gb->speed_switch_countdown = 0;
        }
        else if (gb->speed_switch_countdown > cycles) {
            gb->speed_switch_countdown -= cycles;
        }
        else {
            GB_cycle_count_t old_cycles = gb->speed_switch_countdown;
            cycles -= old_cycles;
            gb->speed_switch_countdown = 0;
            GB_advance_cycles(gb, old_cycles);
            gb->cgb_double_speed ^= true;
        }
    }
    gb->apu.pcm_mask[0] = gb->apu.pcm_mask[1] = 0xFF; // Sort of hacky, but too many cross-component interactions to do it right
    // Affected by speed boost
    gb->dma_cycles = cycles;

#ifdef JACK_MEGADUCK_TIMING_PROFILE
    if (megaduck_profile_sample) {
        megaduck_timing_profile.marks[1] = megaduck_cycle_counter();
    }
#endif

#ifdef JACK_MEGADUCK_ACTIVE_TIMER021
    if (!megaduck_active021_timers_run(gb, cycles)) {
#ifdef JACK_MEGADUCK_HALT017
        if (!megaduck_halt017_timers_run(gb, cycles)) timers_run(gb, cycles);
#else
        timers_run(gb, cycles);
#endif
    }
#elif defined(JACK_MEGADUCK_HALT017)
    if (!megaduck_halt017_timers_run(gb, cycles)) timers_run(gb, cycles);
#else
    timers_run(gb, cycles);
#endif
    camera_run(gb, cycles);
    megaduck_laptop_peripheral_run(gb, cycles);

#ifdef JACK_MEGADUCK_TIMING_PROFILE
    if (megaduck_profile_sample) {
        megaduck_timing_profile.marks[2] = megaduck_cycle_counter();
    }
#endif

    if (unlikely(gb->speed_switch_halt_countdown)) {
        gb->speed_switch_halt_countdown -= cycles;
        if (gb->speed_switch_halt_countdown <= 0) {
            gb->speed_switch_halt_countdown = 0;
            gb->halted = false;
        }
    }
        
#ifndef GB_DISABLE_DEBUGGER
    gb->debugger_ticks += cycles;
#endif
    
    if (gb->speed_switch_freeze) {
        if (gb->speed_switch_freeze >= cycles) {
            gb->speed_switch_freeze -= cycles;
#ifdef JACK_MEGADUCK_TIMING_PROFILE
            if (megaduck_profile_sample) {
                megaduck_timing_profile.marks[3] = megaduck_cycle_counter();
                megaduck_profile_commit_early();
            }
#endif
            return;
        }
        cycles -= gb->speed_switch_freeze;
        gb->speed_switch_freeze = 0;
    }

    if (unlikely(!gb->cgb_double_speed)) {
        cycles <<= 1;
    }
    
#ifndef GB_DISABLE_DEBUGGER
    gb->absolute_debugger_ticks += cycles;
#endif
    
    // Not affected by speed boost
    if (likely(gb->io_registers[GB_IO_LCDC] & GB_LCDC_ENABLE)) {
        gb->double_speed_alignment += cycles;
    }
    gb->cycles_since_last_sync += cycles;
    gb->cycles_since_run += cycles;
    
    gb->rumble_on_cycles += gb->rumble_strength & 3;
    gb->rumble_off_cycles += (gb->rumble_strength & 3) ^ 3;
    
    if (unlikely(gb->data_bus_decay_countdown)) {
        if (gb->data_bus_decay_countdown <= cycles) {
            gb->data_bus_decay_countdown = 0;
            gb->data_bus = 0xFF;
        }
        else {
            gb->data_bus_decay_countdown -= cycles;
        }
    }
#ifdef JACK_MEGADUCK_TIMING_PROFILE
    if (megaduck_profile_sample) {
        megaduck_timing_profile.marks[3] = megaduck_cycle_counter();
    }
#endif
    
    GB_joypad_run(gb, cycles);
#ifdef JACK_MEGADUCK_TIMING_PROFILE
    if (megaduck_profile_sample) {
        megaduck_timing_profile.marks[4] = megaduck_cycle_counter();
    }
#endif
    GB_apu_run(gb, false);
#ifdef JACK_MEGADUCK_TIMING_PROFILE
    if (megaduck_profile_sample) {
        megaduck_timing_profile.marks[5] = megaduck_cycle_counter();
    }
#endif
    GB_display_run(gb, cycles, false);
#ifdef JACK_MEGADUCK_TIMING_PROFILE
    if (megaduck_profile_sample) {
        megaduck_timing_profile.marks[6] = megaduck_cycle_counter();
    }
#endif
    if (unlikely(!gb->stopped)) { // TODO: Verify what happens in STOP mode
        GB_dma_run(gb);
    }
    ir_run(gb, cycles);
    rtc_run(gb, cycles);
#ifdef JACK_MEGADUCK_TIMING_PROFILE
    if (megaduck_profile_sample) {
        megaduck_timing_profile.marks[7] = megaduck_cycle_counter();
        megaduck_profile_commit();
    }
#endif
}

/* 
   This glitch is based on the expected results of mooneye-gb rapid_toggle test.
   This glitch happens because how TIMA is increased, see GB_set_internal_div_counter.
   According to GiiBiiAdvance, GBC's behavior is different, but this was not tested or implemented.
*/
void GB_emulate_timer_glitch(GB_gameboy_t *gb, uint8_t old_tac, uint8_t new_tac)
{
    /* Glitch only happens when old_tac is enabled. */
    if (!(old_tac & 4)) return;

    unsigned old_clocks = TAC_TRIGGER_BITS[old_tac & 3];
    unsigned new_clocks = TAC_TRIGGER_BITS[new_tac & 3];

    /* The bit used for overflow testing must have been 1 */
    if (gb->div_counter & old_clocks) {
        /* And now either the timer must be disabled, or the new bit used for overflow testing be 0. */
        if (!(new_tac & 4) || !(gb->div_counter & new_clocks)) {
            increase_tima(gb);
        }
    }
}
