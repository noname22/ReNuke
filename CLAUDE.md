# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ReNuke is a high-accuracy Yamaha YM3438 (OPN2) emulator based on die shot reverse engineering. This is a fork of the original Nuked-OPN2 project with modernized API and build system. The YM3438 is a CMOS variant of the YM2612 used in Sega MegaDrive/Genesis and FM Towns. This is a C library licensed under LGPL v2.1.

## Architecture

The emulator consists of two main files:
- `include/renuke.h` - Public API header with opaque chip type and function declarations
- `src/renuke.c` - Implementation containing cycle-accurate emulation logic

The emulator operates at the chip's internal clock level (6 master clocks per internal clock cycle) and provides cycle-accurate emulation including undocumented features, SSG-EG, and CSM mode.

## API Reference

Memory management:
- `RN_Create(uint32_t chip_type)` - Allocate and initialize a new chip instance with specified type
- `RN_Destroy(RN_Chip *chip)` - Free a chip instance
- `RN_GetSize()` - Get the size of the chip structure in bytes

Core functions for chip emulation:
- `RN_Reset(RN_Chip *chip)` - Reset emulated chip
- `RN_Clock(RN_Chip *chip, int16_t *buffer)` - Advance chip by 1 internal clock, outputs 9-bit signed MOL/MOR values
- `RN_Write(RN_Chip *chip, uint32_t port, uint8_t data)` - Write to chip port
- `RN_Read(RN_Chip *chip, uint32_t port)` - Read chip status
- `RN_SetTestPin(RN_Chip *chip, uint32_t value)` - Set TEST pin
- `RN_ReadTestPin(RN_Chip *chip)` - Read TEST pin
- `RN_ReadIRQPin(RN_Chip *chip)` - Read IRQ pin

## Key Implementation Details

The emulation uses standard C99 integer types:
- `uint8_t/int8_t` - 8-bit unsigned/signed
- `uint16_t/int16_t` - 16-bit unsigned/signed  
- `uint32_t/int32_t` - 32-bit unsigned/signed
- `uint64_t/int64_t` - 64-bit unsigned/signed

The `RN_Chip` structure is opaque (implementation details hidden). The structure contains the complete chip state including:
- I/O registers and pins
- LFO (Low Frequency Oscillator) state
- Phase generator state for all 24 operators
- Envelope generator state
- Channel output accumulators
- Chip type configuration

Users must use `RN_Create(chip_type)` to allocate chip instances with the desired chip type and `RN_Destroy()` to free them. The chip type is now stored per-instance instead of being global.

## Emulation Modes

Two optional modes can be enabled via flags:
- `ym3438_mode_ym2612` - YM2612 compatibility mode
- `ym3438_mode_readmode` - Enable status read on any port

## Integration

This is a self-contained emulator that can be integrated into other projects by including the header and source files. The Genesis Plus GX fork with this core integrated is available at https://github.com/nukeykt/Genesis-Plus-GX