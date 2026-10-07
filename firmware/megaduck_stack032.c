

#include "megaduck_stack032.h"
#include "pico/stdlib.h"
#include "hardware/exception.h"
#include "hardware/structs/scb.h"
#include "hardware/uart.h"
#include <stdio.h>
#ifdef JACK_MEGADUCK_BOOT046
#include "megaduck_boot046.h"
#endif

#define STACK_PATTERN 0x5a32c0deu
#define GUARD_BYTES 128u
extern uint32_t __StackBottom[], __StackTop[];
extern uint32_t megaduck_core1_stack[2048];
extern void jack_core1_fault_capture(uint32_t *stack, uint32_t exc_return)
    __attribute__((noreturn));
uint32_t megaduck_fault_stack032[2][256] __attribute__((aligned(32), used));
static uint32_t stack_limit;
static bool stack_ready;

static void fault_char(char ch)
{
    for (unsigned n = 0; n < 65536u; ++n) {
        if (uart_is_writable(uart0)) {
            uart_get_hw(uart0)->dr = (uint32_t)(unsigned char)ch;
            return;
        }
    }
}
static void fault_text(const char *s) { while (*s) fault_char(*s++); }
static void fault_hex(uint32_t value)
{
    for (int shift = 28; shift >= 0; shift -= 4)
        fault_char("0123456789abcdef"[(value >> shift) & 15u]);
}

void __attribute__((noreturn, used)) jack_megaduck_fault032_capture(
    uint32_t *original_sp, uint32_t exc_return
#ifdef JACK_MEGADUCK_BOOT046
    , uint32_t original_r11
#endif
    )
{
    const unsigned core = get_core_num();
    const uint32_t cfsr = scb_hw->cfsr;
    const uint32_t hfsr = scb_hw->hfsr;
    const uintptr_t bottom = core ? (uintptr_t)megaduck_core1_stack : (uintptr_t)__StackBottom;
    const uintptr_t top = core ? (uintptr_t)(megaduck_core1_stack + 2048) : (uintptr_t)__StackTop;
    const uintptr_t sp = (uintptr_t)original_sp;

    const bool frame_valid = (exc_return == 0xfffffff9u || exc_return == 0xfffffffdu) &&
        !(cfsr & 0x00103838u) && !(sp & 3u) && sp >= bottom && sp <= top - 32u;
    uint32_t pc = UINT32_MAX, lr = UINT32_MAX, xpsr = UINT32_MAX;
    if (frame_valid) {
        lr = original_sp[5]; pc = original_sp[6]; xpsr = original_sp[7];
    }
    fault_text("\r\nMEGADUCK_FAULT032 core="); fault_hex(core);
    fault_text(" sp="); fault_hex((uint32_t)sp);
    fault_text(" exc_return="); fault_hex(exc_return);
    fault_text(" cfsr="); fault_hex(cfsr);
    fault_text(" hfsr="); fault_hex(hfsr);
    fault_text(" frame_valid="); fault_char(frame_valid ? '1' : '0');
    fault_text(" pc="); fault_hex(pc);
    fault_text(" lr="); fault_hex(lr);
    fault_text(" xpsr="); fault_hex(xpsr);
#ifdef JACK_MEGADUCK_BOOT046
    fault_text(" boot046="); fault_hex(jack_megaduck_boot046_stage);
    fault_text(" r11="); fault_hex(original_r11);
    if (frame_valid) {
        fault_text(" r0="); fault_hex(original_sp[0]);
        fault_text(" r1="); fault_hex(original_sp[1]);
        fault_text(" r2="); fault_hex(original_sp[2]);
        fault_text(" r3="); fault_hex(original_sp[3]);
        fault_text(" r12="); fault_hex(original_sp[4]);
    }
#endif
    fault_text("\r\n");

    if (core == 1u) jack_core1_fault_capture(original_sp, exc_return);
    for (;;) __asm volatile("wfe");
}

void __attribute__((naked, used)) jack_megaduck_hardfault032(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
#ifdef JACK_MEGADUCK_BOOT046
        "mov r12, r11\n"
#endif
        "cpsid i\n"
        "movs r2, #0\n"
        "msr msplim, r2\n"
        "ldr r2, =0xd0000000\n"
        "ldr r2, [r2]\n"
        "lsls r2, r2, #10\n"
        "ldr r3, =megaduck_fault_stack032\n"
        "add.w r3, r3, #1024\n"
        "add r3, r2\n"
        "msr msp, r3\n"
        "isb\n"
#ifdef JACK_MEGADUCK_BOOT046
        "mov r2, r12\n"
#endif
        "b jack_megaduck_fault032_capture\n");
}

void __attribute__((noinline)) jack_megaduck_stack032_init(void)
{
    uintptr_t sp;
    __asm volatile("mrs %0, msp" : "=r"(sp));

    for (volatile uint32_t *p = __StackBottom; (uintptr_t)p < sp - 64u; ++p)
        *p = STACK_PATTERN;
    stack_limit = (uint32_t)(uintptr_t)__StackBottom + GUARD_BYTES;
    __asm volatile("msr msplim, %0\nisb\n" : : "r"(stack_limit) : "memory");
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, jack_megaduck_hardfault032);
    stack_ready = true;
}

void jack_megaduck_stack032_report(const char *phase)
{
    if (!stack_ready) return;
    unsigned free_words = 0;
    const unsigned words = (uintptr_t)(__StackTop) / 4u - (uintptr_t)(__StackBottom) / 4u;
    while (free_words < words && ((volatile uint32_t *)__StackBottom)[free_words] == STACK_PATTERN)
        ++free_words;
    bool guard = true;
    for (unsigned i = 0; i < GUARD_BYTES / 4u; ++i)
        if (((volatile uint32_t *)__StackBottom)[i] != STACK_PATTERN) guard = false;
    uint32_t live_limit;
    __asm volatile("mrs %0, msplim" : "=r"(live_limit));
    printf("MEGADUCK_STACK032 phase=%s core=0 bottom=%08lx top=%08lx bytes=%u limit=%08lx guard=%u low_free=%u used_max=%u\n",
        phase, (unsigned long)(uintptr_t)__StackBottom, (unsigned long)(uintptr_t)__StackTop,
        words * 4u, (unsigned long)live_limit, guard ? 1u : 0u, free_words * 4u,
        (words - free_words) * 4u);
}
