# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This project uses **PlatformIO** (not CMake or Make).

```bash
# Build
pio build -e esp32dev

# Upload (requires manual bootloader entry — see below)
pio upload -e esp32dev

# Serial monitor
pio device monitor -b 115200
```

**Flashing requirement**: Auto-programming is not supported. Before uploading:
1. Disconnect the audio shield
2. Hold BOOT button, press RESET to enter bootloader
3. Run `pio upload` while board is in bootloader mode

## Architecture

The player has four layers:

1. **UI Layer** — OLED display (SSD1306 via I2C) and two GPIO buttons for navigation
2. **Player Orchestration** (`SidPlayer`) — Loads PSID files from SD card, manages subtune selection, drives the emulation loop
3. **Emulation Core** — MOS6502 CPU emulator executes C64 machine code; reSID emulates the MOS6581 SID chip
4. **Audio Output** — I2S DMA streams 44.1kHz 16-bit stereo audio to the UDA1334ATS DAC

### Playback loop (main.cpp)

Each iteration of the main loop:
1. `player.tick()` — runs the 6502 CPU for one PAL frame (20ms / 985,248 Hz clock), updating SID registers at `$D400–$D418` in the 64KB C64 memory space
2. `player.read(buffer)` — reSID generates 882 audio samples (44,100 ÷ 50 Hz) from the updated register state
3. Buffer is sent via I2S to the DAC

This is **sequential**, not real-time parallel — the 6502 and SID run one after the other per frame. This is a known limitation for some tunes (RSID format is unsupported for this reason).

## Key Files

- `src/main.cpp` — entry point, I2S/SD/display/button setup, main loop
- `include/main.h` — all hardware pin definitions and I2S buffer configuration
- `platformio.ini` — build config, library dependencies, build flags (`-O3`)
- `.pio/libdeps/esp32dev/arduino-sid-tools/src/SidPlayer/SidPlayer.h` — SID file loading and playback orchestration (inline implementation)
- `.pio/libdeps/esp32dev/arduino-sid-tools/src/Mos6502/mos6502.h` — 6502 CPU emulator
- `.pio/libdeps/esp32dev/arduino-sid-tools/src/reSID/sid.h` — SID chip emulator (reSID v0.16 by Dag Lem)

## Hardware Pin Assignments (Makerfab board)

| Function | Pins |
|----------|------|
| I2S (audio) | LRC=25, BCLK=26, DOUT=27 |
| SD card (SPI) | CLK=18, CS=22, MOSI=23, MISO=19 |
| Buttons | Next song=2, Next tune=15 |
| I2C (OLED) | SDA=4, SCL=5 |

## Oscilloscope Display

The display shows three voice lanes (each ~21 px tall) synthesized from the SID register state read every audio frame. Registers are accessed via the global `mem[]` array (declared `extern` in `mos6502.h`, included through `SidTools.h`) — no reSID library modifications are required.

The display runs in a dedicated FreeRTOS task pinned to **Core 0** (`displayTask` in `main.cpp`) so I2C transfers (~25 ms at 400 kHz) never block the audio loop on Core 1. A binary semaphore (`xDisplaySemaphore`) is given by `loop()` each audio frame; the display task renders whenever it can keep up.

Waveform shapes (sawtooth, triangle, pulse, noise) are drawn based on the control register; the number of cycles shown per lane is derived from the frequency register so higher-pitched voices show more cycles.

## SID File Format Notes

- Only **PSID** format is supported (register-based playback)
- **RSID** (real-time interrupt-driven) is not supported
- Place `.sid` files in the `/SID/` directory on the SD card
- Files may contain multiple subtunes; the next-tune button cycles through them
