#ifndef MEGADUCK_PPU040_H
#define MEGADUCK_PPU040_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Diagnostic-only runtime gate. Both renderers remain in the same ELF. */
extern volatile uint32_t jack_megaduck_ppu040_enabled;
#ifdef __cplusplus
}
#endif
#endif
