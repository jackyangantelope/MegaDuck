# MegaDuck for Adafruit Fruit Jam

This is a source and build candidate for the Adafruit Fruit Jam (RP2350B). It is based on the accepted Mega Duck 0.1.51 emulator source, which had limited RP2350B hardware testing at 378 MHz. This Fruit Jam adaptation has compiled, but has **not** been tested on Fruit Jam hardware. Do not treat its performance or audio quality as verified on Fruit Jam.

The firmware contains no game ROMs or console boot ROM. Put your own lawful Mega Duck cartridges in `/roms/MEGADUCK` on the SD card. Accepted file extensions are `.md0`, `.md1`, `.md2`, `.md20s`, `.md25s`, `.duck`, and `.bin`. The firmware starts with a ROM picker. HDMI output is 640×480, with 44.1 kHz stereo audio and USB host input.

The default build uses the public Fruit Jam board profile at 252 MHz; it does **not** enable the 378 MHz / 1.50 V configuration used for the original 0.1.51 test. Recheck boot, speed, HDMI video/audio, controls, SD loading, and saves on Fruit Jam before distributing a binary as hardware-validated.

## Build

On Windows, install CMake, Ninja, an ARM Cortex-M33 toolchain, Pico SDK 2.3.0 or newer, and Pico-PIO-USB. Run:

```powershell
./scripts/build-release.ps1 -PicoSdkPath C:\path\to\pico-sdk -PicoPioUsbPath C:\path\to\Pico-PIO-USB -ToolchainPath C:\path\to\toolchain -NinjaPath C:\path\to\ninja.exe
```

`-PioasmDir`, `-PicotoolDir`, and `-TinyUsbPath` are optional if those tools are not found by the SDK. The output is `build/fruit-jam-release/picoMegaDuck.uf2`.

The default output is linked for the resident loader's application partition starting at `0x10080000`. **Do not flash it as a standalone full-flash image.** For a standalone link, configure with `-DBUILD_FOR_BOOTLOADER=OFF`; that variant has only been compile-tested.

## Origin and license

The emulation core comes from [bbbbbr/SuperJuniorSameDuck](https://github.com/bbbbbr/SuperJuniorSameDuck), revision `89b5e923311f4685134cce71f2eca03bbc08a099`. It is SameBoy-derived and includes Mega Duck support. This is an adapted copy, **not** an unmodified mirror: `Core/gb.c`, `display.c`, `apu.c`, `memory.c`, `timing.c`, `sm83_cpu.c`, `sgb.c`, `joypad.c`, related headers and border assets were modified for this embedded port. The added `Core/megaduck_*.h` files provide port-specific fast paths, diagnostics, and memory placement. `firmware/libretro_embedded.c` adapts the upstream libretro frontend for this embedded target. See [SOURCE_CHANGES.md](SOURCE_CHANGES.md) for the file-level summary.

The upstream core's Expat/MIT notice and embedded third-party notices are retained in [deps/SuperJuniorSameDuck/LICENSE](deps/SuperJuniorSameDuck/LICENSE). The public board runtime uses [PicoPlus-devel/pico_shared](https://github.com/PicoPlus-devel/pico_shared) revision `df683bf00ff95e1ceda88f132bbf72f8cc7cadb0` under GPL-3.0, with its own license file. The combined firmware is distributed under [GPL-3.0](LICENSE); component-level credits and file-specific notices are in [THIRD_PARTY.md](THIRD_PARTY.md).
