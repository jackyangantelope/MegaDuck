
#include <stdbool.h>
#include <stdint.h>

#include <Core/gb.h>
#include <Core/megaduck_laptop.h>

bool GB_megaduck_laptop_is_enabled(GB_gameboy_t *gb)
{
    (void)gb;
    return false;
}

void GB_megaduck_laptop_peripheral_update(GB_gameboy_t *gb, uint8_t cycles)
{
    (void)gb;
    (void)cycles;
}