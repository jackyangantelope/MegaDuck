
#pragma once
#include <stdint.h>
#include "hardware/regs/addressmap.h"

enum { MEGADUCK_FRAME050_BAD_EXC = 1, MEGADUCK_FRAME050_BAD_SP = 2,
       MEGADUCK_FRAME050_BAD_PSR = 3, MEGADUCK_FRAME050_BAD_PC = 4 };

static inline __attribute__((always_inline)) unsigned
megaduck_exception050_read(const uint32_t *raw, uint32_t exc,
                          uint32_t *pc, uint32_t *psr)
{
    if ((exc & 0xffffffc3u) != 0xffffffc1u || (!(exc & 8u) && (exc & 4u)))
        return MEGADUCK_FRAME050_BAD_EXC;
    uintptr_t sp = (uintptr_t)raw;
    unsigned offset = (exc & 32u) ? 0u : 40u;
    if ((sp & 3u) || sp < SRAM_BASE || sp > SRAM_END - 32u - offset)
        return MEGADUCK_FRAME050_BAD_SP;
    const uint32_t *core = (const uint32_t *)(sp + offset);
    *pc = core[6];
    *psr = core[7];
    if (!(*psr & (1u << 24)) || (((exc & 8u) != 0) != ((*psr & 0x1ffu) == 0)))
        return MEGADUCK_FRAME050_BAD_PSR;
    uint32_t address = *pc & ~1u;
    if (!((address >= 0x100u && address < 0x10000u) ||
          (address >= 0x10080000u && address < 0x11000000u) ||
          (address >= SRAM_BASE && address < SRAM_END)))
        return MEGADUCK_FRAME050_BAD_PC;
    return 0;
}
