# Firmware

STM32 + Qorvo/Decawave DW3000 (DWS3000 Arduino shield) firmware for UWB ranging,
built on the vendored `DW3_QM33_SDK_1.1.1/Drivers/API` (bare-metal driver) layer.

Each board is its own STM32CubeMX-generated CMake project under `firmware/<board_name>/`,
with a small `platform/` layer adapting the vendor SDK to that board's pins, and a `config/`
override so each board can run a different example at the same time (e.g. one board as an
SS-TWR ranging initiator, another as the responder).

Currently set up:

| Folder | Board | MCU | Role |
|---|---|---|---|
| `Nucleo_F446RE_DWS3000` | Nucleo-F446RE | STM32F446RE | SS-TWR initiator |
| `Nucleo_H753ZI_DWS3000` | Nucleo-H753ZI | STM32H753ZI | SS-TWR responder |

## Prerequisites

- `arm-none-eabi-gcc`, `cmake`, `ninja`
- STM32CubeMX (only needed when generating a *new* board project)
- STM32CubeProgrammer (`STM32_Programmer_CLI`) for flashing, or `openocd`

## Quick start (existing boards)

```bash
cd firmware/Nucleo_F446RE_DWS3000   # or Nucleo_H753ZI_DWS3000
cmake --preset Debug
cmake --build build/Debug
STM32_Programmer_CLI -c port=SWD -w build/Debug/<ProjectName>.elf -v -rst
```

Open a serial terminal on the board's ST-Link virtual COM port at 115200 8N1 to see output.
`STM32_Programmer_CLI --list` shows connected boards, their ST-Link serial numbers, and which
`/dev/ttyACMx` each one's VCP landed on (use `sn=<serial>` on multi-board setups so you flash
the board you mean to).

## How example selection works

Each board has its own `config/example_selection.h`, which shadows the vendor's example
selector via include-path ordering in that board's `CMakeLists.txt` (`config` is listed before
the vendored `API/Src` directory). Uncomment exactly one `TEST_*` macro in that file to choose
what the board runs (`TEST_READING_DEV_ID` for the SPI/pin smoke test, `TEST_SS_TWR_INITIATOR`
/ `TEST_SS_TWR_RESPONDER` for ranging, etc. - see the vendor SDK's
`Drivers/API/Src/examples/` for the full list). Rebuild and reflash after changing it.

**Important:** there is no vendor-default `example_selection.h` anymore - it was deleted
(see "Known gotchas" below) specifically so each board's local copy is unambiguous. Don't
recreate one at `DW3_QM33_SDK_1.1.1/Drivers/API/Src/example_selection.h`, or you'll reintroduce
the bug it was removed to fix.

Whichever examples a board actually runs also drives what belongs in that board's
`target_sources` - e.g. `TEST_SS_TWR_INITIATOR` needs `config_options.c`, `shared_functions.c`,
and `ex_06a_ss_twr_initiator/ss_twr_initiator.c` added, on top of the driver/platform files
every board needs. Copy the pattern from an existing board's `CMakeLists.txt`.

---

## Adding another board of the same MCU (e.g. a second Nucleo-F446RE)

If you're adding a second board that's electrically identical to one you already have
(same MCU, same DWS3000 wiring), you don't need a new CubeMX project:

1. Copy the existing project folder, e.g. `Nucleo_F446RE_DWS3000` → `Nucleo_F446RE_DWS3000_B`.
2. In the copy's `.ioc` and `CMakeLists.txt`, rename `ProjectManager.ProjectName` /
   `CMAKE_PROJECT_NAME` to the new folder name (CubeMX project name and CMake project name
   should match the folder for sanity, though nothing strictly requires it).
3. Edit `config/example_selection.h` to whatever role this board should play.
4. Update `target_sources`/`target_include_directories` in `CMakeLists.txt` to match whatever
   that role needs (see "How example selection works" above).
5. Build and flash exactly as in Quick start, using `sn=<its ST-Link serial>` to target it
   specifically once more than one board is connected.

You do **not** need to touch `platform/*.c` - it's already correct for this MCU/wiring.

## Adding a board of a different MCU type

This is the real work, and it's exactly what was done to bring up the H753ZI board from the
already-working F446RE one. Budget time for verifying pin roles from real documentation, not
by eyeballing - a wrong RSTn/IRQ/WAKEUP/CS pin produces a board that silently never talks to
the DW3000, and the debugging loop for that is slow.

### 1. Confirm the DWS3000 shield's Arduino signal roles

These are fixed by the shield itself and don't change per host board (confirmed against an
independent open-source port of this exact shield, see git history for the source link):

| Shield signal | Arduino pin |
|---|---|
| RSTn | D7 |
| IRQ | D8 |
| WAKEUP | D9 |
| SPI CS | D10 |
| SPI MOSI | D11 |
| SPI MISO | D12 |
| SPI SCK | D13 |

### 2. Find *this MCU's* physical pin for each Arduino signal

**This is the step that's easy to get subtly wrong.** Different Nucleo board form factors
(Nucleo-64 vs Nucleo-144) route the "same" Arduino D-number to *different* underlying MCU
pins - e.g. on the F446RE (Nucleo-64) D7 is `PA8`, but on the H753ZI (Nucleo-144) D7 is `PG12`.
Don't assume; verify per-board against an authoritative source:

- ST's Nucleo board User Manual (UM1724 for Nucleo-64, UM1974 for Nucleo-144) Arduino
  connector pinout table, or
- A maintained community source with an explicit pin table, e.g. the STM32duino
  `Arduino_Core_STM32` variant header for that exact board (`variant_NUCLEO_<board>.h`), which
  lists `#define PXn N` for each Arduino digital pin - reliable and easy to grep.

SPI pins (D11/D12/D13, i.e. MOSI/MISO/SCK) are commonly `PB5`/`PA6`/`PA5` across many Nucleo
boards, but confirm rather than assume - especially D11, which has a solder-bridge-selectable
alternate (`PA7`) on some boards.

### 3. Generate the CubeMX project

New STM32CubeMX project targeting your Nucleo board, CMake toolchain, with:
- One SPI instance: Master, Full-Duplex, **8-bit data size**, **CPOL=Low / CPHA=1Edge** (SPI
  Mode 0 - the DW3000 requires this), NSS = Software.
- One GPIO output for RSTn (see step 4 for its exact required mode), initial state doesn't
  matter much since `platform/port.c` reconfigures it at runtime - but CS should be initialized
  **HIGH** (deselected) in `MX_GPIO_Init()`, since some CubeMX defaults leave it LOW.
- One GPIO output for WAKEUP (push-pull is fine).
- One GPIO input with an EXTI rising-edge interrupt for IRQ.
- A console UART (raw HAL UART, or whatever your board's BSP normally uses for its ST-Link VCP
  - the H753ZI project uses `BSP_COM_Init()`/`hcom_uart[COM1]` instead of a plain HAL UART
    because that's how ST's Nucleo-144 BSP does it; match your board's own convention rather
    than copying either existing project verbatim).

**Check the generated `MX_SPI1_Init()` carefully before moving on** - CubeMX defaults have
been wrong twice in this repo's history (once `SPI_DATASIZE_4BIT` instead of `8BIT`, once a
prescaler giving 45 MHz on a chip that needs well under 20 MHz). See "Known gotchas" below.

### 4. Create `platform/*.c`

Copy an existing board's `platform/` folder as your starting point (`deca_mutex.c`,
`deca_sleep.c`, `deca_spi.h`, `deca_probe_interface.c/.h` are 100% generic across boards and
copy verbatim - they only reference `hspi1` and `SPI1_CSn_*`, names shared across every board's
`main.h`). You'll need to adapt:

- **`port.h`**: `#include <stm32<family>xx_hal.h>` for your chip family; `DECAIRQ_EXTI_IRQn`
  should reference your `main.h`'s `DW_IRQn_EXTI_IRQn`.
- **`port.c`**: RSTn is configured `GPIO_MODE_OUTPUT_OD` (open-drain) at runtime regardless of
  what CubeMX set it to statically - the DW IC's RSTn line must never be driven high
  externally, only pulled low or released. Recompute the slow/fast SPI prescalers for *this
  chip's* actual SPI peripheral clock (not APB2 blindly - on STM32H7 in particular, SPI1's
  clock comes from a separately-configurable kernel clock mux, not directly off APB2). Target
  roughly ≤3 MHz for the slow rate (used before the DW IC's own crystal is confirmed running)
  and comfortably under 20 MHz for the fast rate.
- **`deca_spi.c`**: check whether your chip family's SPI peripheral has a legacy `DR` data
  register (SPIv1/v2, e.g. F4) or a FIFO-based `TXDR`/`RXDR` (SPIv3, e.g. H7). If it's SPIv3,
  don't port the F4 platform's manual register-polling read loop - just call
  `HAL_SPI_Receive()` directly; H7's HAL doesn't have the single-byte-read quirk that loop
  existed to work around.

### 5. Wire `CMakeLists.txt`

Copy the pattern from an existing board's `CMakeLists.txt`: the `DW3_SDK_DRIVERS_PATH` /
`UWB_DRIVER_PATH` / `EXAMPLES_PATH` variables, the `add_subdirectory` for `uwb_driver` with
`DWT_DW3000=1`, the `target_sources`/`target_include_directories` (remember `config` before
`EXAMPLES_PATH`), `USE_DRV_DW3000` in `target_compile_definitions`, and
`-u _printf_float` in `target_link_options` if this board will ever format a float with
`printf`/`snprintf` (nano.specs strips float formatting by default - see "Known gotchas").

### 6. Give it a local `config/example_selection.h`

Copy from an existing board and enable whichever `TEST_*` macro this board should run.

### 7. Build, flash, verify

Start with `TEST_READING_DEV_ID` regardless of this board's eventual role - it's the fastest
way to confirm SPI/pins/reset sequencing are all correct before debugging anything more complex
on top of them. Expect `DEV ID OK` on the serial console. Only move on to the board's real role
once that passes.

---

## Known gotchas (all hit for real getting this repo working)

- **CubeMX SPI defaults aren't automatically DW3000-safe.** Check `DataSize` (must be 8-bit -
  one board's CubeMX default was silently `4BIT`, which would have corrupted every transfer)
  and `BaudRatePrescaler` (must land well under ~20 MHz - one board's default was 45 MHz) by
  hand every time.
- **CS must idle HIGH.** Some CubeMX GPIO defaults leave the CS pin's initial `HAL_GPIO_WritePin`
  call at `GPIO_PIN_RESET` (LOW = selected), which is backwards for an active-low chip select.
- **RSTn must be open-drain**, reconfigured at runtime in `port.c` regardless of what CubeMX's
  static `MX_GPIO_Init()` set it to.
- **`nano.specs` silently drops float support from `printf`/`snprintf`.** Symptom: a `%f`
  format specifier prints nothing (e.g. `"DIST:  m"` instead of `"DIST: 1.23 m"`) with no
  compiler warning or crash. Fix: `-u _printf_float` in the linker flags for any board that
  formats floats.
- **Don't trust cross-board pin analogies.** Nucleo-64 and Nucleo-144 (and different Nucleo-144
  chips) route the same Arduino D-number to different physical MCU pins. Verify per-board (see
  step 2 above) rather than copying another board's pin choice by name-similarity.
- **Watch include-guard collisions between quote- and angle-bracket includes.** The vendor SDK
  mixes `#include "example_selection.h"` (in `config_options.h`) with `#include
  <example_selection.h>` (in most examples). A quote-include always checks the *including
  file's own directory* first, before any `-I` search path - so if a vendor file sitting next
  to `config_options.h` exists, it wins regardless of your include-path ordering, silently
  poisoning the shared header guard for the rest of that translation unit. This is why the
  vendor's own `Drivers/API/Src/example_selection.h` was deleted rather than just shadowed:
  shadowing only reliably works for angle-bracket includes.
