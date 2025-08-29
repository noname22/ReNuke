# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Nuked-OPN2 is a high-accuracy Yamaha YM3438 (OPN2) emulator based on die shot reverse engineering. The YM3438 is a CMOS variant of the YM2612 used in Sega MegaDrive/Genesis and FM Towns. This is a header-only C library licensed under LGPL v2.1.

## Architecture

The emulator consists of two main files:
- `include/ym3438.h` - Public API header with chip state structure and function declarations
- `src/ym3438.c` - Implementation containing cycle-accurate emulation logic

The emulator operates at the chip's internal clock level (6 master clocks per internal clock cycle) and provides cycle-accurate emulation including undocumented features, SSG-EG, and CSM mode.

## API Reference

Memory management:
- `OPN2_Create()` - Allocate and initialize a new chip instance
- `OPN2_Destroy(ym3438_t *chip)` - Free a chip instance
- `OPN2_GetSize()` - Get the size of the chip structure in bytes

Core functions for chip emulation:
- `OPN2_Reset(ym3438_t *chip)` - Reset emulated chip
- `OPN2_Clock(ym3438_t *chip, Bit32s *buffer)` - Advance chip by 1 internal clock, outputs 9-bit signed MOL/MOR values
- `OPN2_Write(ym3438_t *chip, Bit32u port, Bit8u data)` - Write to chip port
- `OPN2_Read(ym3438_t *chip, Bit32u port)` - Read chip status
- `OPN2_SetTestPin(ym3438_t *chip, Bit32u value)` - Set TEST pin
- `OPN2_ReadTestPin(ym3438_t *chip)` - Read TEST pin
- `OPN2_ReadIRQPin(ym3438_t *chip)` - Read IRQ pin

## Key Implementation Details

The emulation uses custom integer types defined in the header:
- `Bit8u/Bit8s` - 8-bit unsigned/signed
- `Bit16u/Bit16s` - 16-bit unsigned/signed  
- `Bit32u/Bit32s` - 32-bit unsigned/signed
- `Bit64u/Bit64s` - 64-bit unsigned/signed

The `ym3438_t` structure is now opaque (implementation details hidden). The structure contains the complete chip state including:
- I/O registers and pins
- LFO (Low Frequency Oscillator) state
- Phase generator state for all 24 operators
- Envelope generator state
- Channel output accumulators

Users must use `OPN2_Create()` to allocate chip instances and `OPN2_Destroy()` to free them.

## Emulation Modes

Two optional modes can be enabled via flags:
- `ym3438_mode_ym2612` - YM2612 compatibility mode
- `ym3438_mode_readmode` - Enable status read on any port

## Integration

This is a self-contained emulator that can be integrated into other projects by including the header and source files. The Genesis Plus GX fork with this core integrated is available at https://github.com/nukeykt/Genesis-Plus-GX