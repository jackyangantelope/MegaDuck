# Source changes from upstream

Base: [bbbbbr/SuperJuniorSameDuck](https://github.com/bbbbbr/SuperJuniorSameDuck) `89b5e923311f4685134cce71f2eca03bbc08a099` (Expat/MIT, with file-specific third-party notices). The `deps/SuperJuniorSameDuck` directory includes both upstream files and local modifications; do not assume it is pristine upstream.

| Files | Embedded-port changes |
| --- | --- |
| `Core/gb.c`, `Core/gb.h`, `Core/memory.c`, `Core/timing.c`, `Core/timing.h` | Mega Duck cartridge/register handling, SRAM placement, timing and guarded HALT batching. |
| `Core/display.c`, `Core/graphics/*_border.inc` | Direct RGB555 scanline output and reduced framebuffer work. |
| `Core/apu.c` | 44.1 kHz output and the accepted 0.1.51 equivalent APU fast paths. |
| `Core/sm83_cpu.c`, `Core/sgb.c`, `Core/joypad.c` | Embedded input and execution-path adjustments. |
| `Core/megaduck_*.h` | Port-specific timing, hot-code placement, and diagnostic helpers. |
| `firmware/libretro_embedded.c`, `dummy_boot.c`, `megaduck_laptop_stubs.c`, `newlib_compat.c` | Embedded libretro bridge and portable stubs; no console boot ROM data is included. |
| `firmware/megaduck_*`, `firmware/video_output041.c` | SD/ROM flow support, HDMI and PCM transport, pacing and diagnostic implementation. |
| `platform/wave1/*`, `cmake/LoaderPartition.cmake` | Fruit Jam runtime, USB host, HDMI adapter, and resident-loader application partition. |

The Fruit Jam build disables the higher-clock test configuration and omits the optional 0.1.49/0.1.50 sampled-PC profiler. It retains the accepted 0.1.51 APU fast path. The Fruit Jam adaptation has been compiled but not hardware-tested.
