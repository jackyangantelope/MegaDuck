#pragma once
/* Opt-in placement only. No timing, memory-map or instruction semantics change. */
#ifdef JACK_MEGADUCK_HOT_CODE_SRAM
#define JACK_MEGADUCK_HOT_FUNC __attribute__((section(".time_critical.megaduck_hot")))
#define JACK_MEGADUCK_HOT_TABLE(name) __attribute__((section(".data.megaduck_hot_" #name)))
#else
#define JACK_MEGADUCK_HOT_FUNC
#define JACK_MEGADUCK_HOT_TABLE(name)
#endif
