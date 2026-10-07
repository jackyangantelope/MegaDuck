
#include "megaduck_opt047.h"
#include <string.h>

static uint8_t vram047[8192] __attribute__((aligned(4)));
static uint8_t **slot047, *original047;
static bool enabled047;

bool __attribute__((noinline, used)) megaduck_vram047_set(bool enabled)
{
    if (!slot047 || !original047) return false;

    if (*slot047 != (enabled047 ? vram047 : original047)) return false;
    if (enabled != enabled047) {
        uint8_t *destination = enabled ? vram047 : original047;
        memcpy(destination, *slot047, sizeof(vram047));
        *slot047 = destination;
        enabled047 = enabled;
    }
    return true;
}
bool megaduck_vram047_get(void)
{
    return slot047 && enabled047 && *slot047 == vram047;
}
void __attribute__((noinline, used)) megaduck_vram047_release(void)
{
    if (megaduck_vram047_get()) (void)megaduck_vram047_set(false);
    slot047 = NULL; original047 = NULL; enabled047 = false;
}
bool __attribute__((noinline, used)) megaduck_vram047_bind(uint8_t **slot, uint32_t size)
{
    megaduck_vram047_release();
    if (!slot || !*slot || size != sizeof(vram047)) return false;

    uintptr_t address = (uintptr_t)*slot;
    if (address < 0x11000000u || address > 0x11800000u - sizeof(vram047)) return false;
    slot047 = slot; original047 = *slot;
    return megaduck_vram047_set(true);
}
