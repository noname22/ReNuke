# ReNuke
High accuracy Yamaha YM3438(OPN2) emulator.

This is a fork of the original [Nuked-OPN2](https://github.com/nukeykt/Nuked-OPN2) project with modernized API and build system.

The YM3438 is a CMOS variant of the YM2612 used in Sega MegaDrive(Genesis) and FM Towns.

## Features
* Based on YM3438 die shot reverse engineering and thus provides very high emulation accuracy.
* Cycle-accurate.
* Undocumented registers/features emulation.
* SSG-EG, CSM mode emulation.
* Compatible with the YM2612.

## Examples

The repository includes example programs demonstrating ReNuke usage:

- **tone-generation**: Simple YM2612 tone generator that outputs a 440Hz A4 note to WAV file
- **vgm-player**: VGM 1.50 file player with SDL3 for playback of YM2612+PSG chiptune music

Build and run examples:
```bash
meson setup build
ninja -C build
./build/examples/tone-generation/tone-generation  # Creates output.wav
./build/examples/vgm-player/vgm-player song.vgm   # Play VGM file
```

## API
```c
/* Memory management */
RN_Chip* RN_Create(RN_ChipType chip_type) // Allocate and initialize chip instance with type
void RN_Destroy(RN_Chip *chip) // Free chip instance

/* Core emulation */
void RN_Reset(RN_Chip *chip) // Reset emulated chip
void RN_Clock(RN_Chip *chip, int clock_count) // Advance chip by specified internal clock cycles
void RN_Write(RN_Chip *chip, uint32_t port, uint8_t data) // Write data to chip port
uint8_t RN_Read(RN_Chip *chip, uint32_t port) // Read chip status
void RN_SetTestPin(RN_Chip *chip, uint32_t value) // Set TEST pin
uint32_t RN_ReadTestPin(RN_Chip *chip) // Read TEST pin
uint32_t RN_ReadIRQPin(RN_Chip *chip) // Read IRQ pin

/* Sample output */
uint32_t RN_GetQueuedSamplesCount(RN_Chip* chip) // Get number of samples in queue
uint32_t RN_DequeueSamples(RN_Chip* chip, int16_t* buffer, uint32_t sample_count) // Dequeue samples

/* Write scheduling */
void RN_ScheduleWrite(RN_Chip* chip, uint32_t port, uint8_t data) // Schedule write with proper timing
```
