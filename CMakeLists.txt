cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD adafruit_fruit_jam CACHE STRING "Adafruit Fruit Jam" FORCE)
set(PICO_PSRAM_CS_PIN 47 CACHE STRING "Fruit Jam PSRAM CS" FORCE)
set(PICO_PSRAM_SIZE_BYTES 0x800000 CACHE STRING "Fruit Jam PSRAM size" FORCE)
set(PICO_DEFAULT_UART 0 CACHE STRING "Diagnostic UART" FORCE)
set(PICO_DEFAULT_UART_TX_PIN 44 CACHE STRING "Diagnostic UART TX" FORCE)
set(PICO_DEFAULT_UART_RX_PIN 45 CACHE STRING "Diagnostic UART RX" FORCE)

if(NOT DEFINED PICO_SDK_PATH OR NOT DEFINED PICO_PIO_USB_PATH)
    message(FATAL_ERROR "Set PICO_SDK_PATH and PICO_PIO_USB_PATH")
endif()
set(PICO_SHARED_PATH "${CMAKE_CURRENT_LIST_DIR}/deps/pico_shared" CACHE PATH "pico_shared checkout")
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(picoMegaDuck C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(MEGADUCK_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(CORE_ROOT "${MEGADUCK_ROOT}/deps/SuperJuniorSameDuck")
set(FW_ROOT "${MEGADUCK_ROOT}/firmware")
set(WAVE_ROOT "${MEGADUCK_ROOT}/platform/wave1")

add_library(sameduck_core STATIC
    "${CORE_ROOT}/Core/gb.c" "${CORE_ROOT}/Core/sgb.c"
    "${CORE_ROOT}/Core/apu.c" "${CORE_ROOT}/Core/memory.c"
    "${CORE_ROOT}/Core/mbc.c" "${CORE_ROOT}/Core/timing.c"
    "${CORE_ROOT}/Core/display.c" "${CORE_ROOT}/Core/camera.c"
    "${CORE_ROOT}/Core/sm83_cpu.c" "${CORE_ROOT}/Core/joypad.c"
    "${CORE_ROOT}/Core/save_state.c" "${CORE_ROOT}/Core/random.c"
    "${CORE_ROOT}/Core/rumble.c"
    "${FW_ROOT}/dummy_boot.c" "${FW_ROOT}/libretro_embedded.c"
    "${FW_ROOT}/megaduck_laptop_stubs.c" "${FW_ROOT}/newlib_compat.c")
target_include_directories(sameduck_core PUBLIC
    "${CORE_ROOT}" "${CORE_ROOT}/Core" "${CORE_ROOT}/libretro" "${WAVE_ROOT}")
target_include_directories(sameduck_core PRIVATE "${FW_ROOT}")
target_compile_definitions(sameduck_core PRIVATE
    GB_INTERNAL=1 GB_VERSION="0.15.8-duck" GB_DISABLE_TIMEKEEPING=1
    GB_DISABLE_REWIND=1 GB_DISABLE_DEBUGGER=1 GB_DISABLE_CHEATS=1
    AUDIO_FREQUENCY=44100 JACK_MEGADUCK_CORE_STATE_SRAM=1
    JACK_MEGADUCK_FRAMEBUFFER_SRAM=1
    JACK_MEGADUCK_HALT017=1 JACK_MEGADUCK_HALT017_DEFAULT_ON=1
    JACK_MEGADUCK_HOT_CODE_SRAM=1 JACK_MEGADUCK_PPU_DEADLINE022=1
    JACK_MEGADUCK_AUDIO023=1 JACK_MEGADUCK_AUDIO024=1
    JACK_MEGADUCK_TIMA037=1 JACK_MEGADUCK_PPU039=1
    JACK_MEGADUCK_DIAG040=1 JACK_MEGADUCK_HDMI041=1
    JACK_MEGADUCK_HDMI042=1 JACK_MEGADUCK_HDMI043=1
    malloc=jack_psram_malloc calloc=jack_psram_calloc
    realloc=jack_psram_realloc free=jack_psram_free)
target_compile_definitions(sameduck_core PUBLIC JACK_MEGADUCK_DIRECT_RGB555=1)
target_compile_options(sameduck_core PRIVATE
    -Wno-unused-parameter -Wno-sign-compare -Wno-implicit-function-declaration)
set_property(TARGET sameduck_core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set_property(SOURCE "${CORE_ROOT}/Core/apu.c" APPEND PROPERTY
    COMPILE_DEFINITIONS JACK_MEGADUCK_APU051=1)
set_property(SOURCE "${FW_ROOT}/libretro_embedded.c" APPEND PROPERTY
    COMPILE_DEFINITIONS JACK_MEGADUCK_OPT047=1)

set(JACK_WAVE1_HDMI_AUDIO ON)
set(JACK_WAVE1_HDMI_DRIVER_ROOT "${PICO_SHARED_PATH}/drivers/pico_hdmi")
set(JACK_WAVE1_HDMI_VIDEO_SOURCE "${FW_ROOT}/video_output041.c")
set(JACK_WAVE1_HDMI_ADAPTER "${WAVE_ROOT}/hdmi/jack_hdmi.c")
set_property(SOURCE "${JACK_WAVE1_HDMI_ADAPTER}" APPEND PROPERTY
    COMPILE_DEFINITIONS JACK_MEGADUCK_AUDIO044=1 JACK_MEGADUCK_AUDIO048=1 JACK_MEGADUCK_OPT047=1)
add_subdirectory("${WAVE_ROOT}" "${CMAKE_BINARY_DIR}/wave1")
target_compile_definitions(jack_wave1_runtime PRIVATE
    JACK_WAVE1_USB_NONBLOCKING=1 JACK_WAVE1_QUIET_INPUT_LOG=1)

option(BUILD_FOR_BOOTLOADER "Link to the resident loader app partition" ON)
add_executable(picoMegaDuck
    "${WAVE_ROOT}/jack_libretro_frontend.cpp"
    "${WAVE_ROOT}/jack_psram_heap.cpp"
    "${FW_ROOT}/megaduck_log047.c"
    "${FW_ROOT}/megaduck_audio047.c"
    "${FW_ROOT}/megaduck_vram047.c"
    "${FW_ROOT}/megaduck_audio048.c"
    "${FW_ROOT}/megaduck_diag040.cpp"
    "${FW_ROOT}/megaduck_pace036.c"
    "${FW_ROOT}/megaduck_stack032.c")
pico_set_program_name(picoMegaDuck "picoMegaDuck")
pico_set_program_version(picoMegaDuck "0.1.51-public-candidate")
pico_enable_stdio_uart(picoMegaDuck 1)
pico_enable_stdio_usb(picoMegaDuck 0)
target_compile_definitions(picoMegaDuck PRIVATE
    HW_CONFIG=8 PICO_STACK_SIZE=4096 PICO_USE_STACK_GUARDS=1
    JACK_LR_APP_NAME="Mega Duck"
    JACK_LR_PICKER_TITLE="SELECT MEGA DUCK ROM"
    JACK_LR_ROM_DIRECTORY="/roms/MEGADUCK"
    JACK_LR_EXTENSIONS=".md0 .md1 .md2 .md20s .md25s .duck .bin"
    JACK_LR_LOG_PREFIX="MEGADUCK"
    JACK_LR_VERSION="0.1.51-public-candidate"
    JACK_WAVE1_PERF_INTERVAL_US=5000000u
    JACK_LR_NATIVE_RGB555=1 JACK_MEGADUCK_CORE1_PROBE026=1
    JACK_MEGADUCK_STACK_ISOLATE030=1 JACK_MEGADUCK_CONTINUOUS031=1
    JACK_MEGADUCK_STACK_FIX032=1 JACK_MEGADUCK_AUDIO033=1
    JACK_MEGADUCK_AUDIO034=1 JACK_MEGADUCK_PACE036=1
    JACK_MEGADUCK_QUIET_KEYS036=1 JACK_MEGADUCK_TIMA037=1
    JACK_MEGADUCK_DIAG040=1 JACK_MEGADUCK_HDMI041=1
    JACK_MEGADUCK_HDMI042=1 JACK_MEGADUCK_HDMI043=1
    JACK_MEGADUCK_OPT047=1 JACK_MEGADUCK_AUDIO048=1
    JACK_MEGADUCK_HALT017=1 JACK_MEGADUCK_PPU_DEADLINE022=1)
target_include_directories(picoMegaDuck PRIVATE
    "${CORE_ROOT}/libretro" "${CORE_ROOT}/Core" "${FW_ROOT}")
target_link_libraries(picoMegaDuck PRIVATE
    pico_stdlib hardware_psram hardware_exception hardware_clocks
    sameduck_core jack_wave1_runtime)
target_link_options(picoMegaDuck PRIVATE -Wl,--gc-sections -Wl,--print-memory-usage)
if(BUILD_FOR_BOOTLOADER)
    include("${MEGADUCK_ROOT}/cmake/LoaderPartition.cmake")
    megaduck_link_for_loader(picoMegaDuck)
endif()
pico_add_extra_outputs(picoMegaDuck)
