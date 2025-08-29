# ReNuke
High accuracy Yamaha YM3438(OPN2) emulator.

This is a fork of the original [Nuked-OPN2](https://github.com/nukeykt/Nuked-OPN2) project with modernized API and build system.

The YM3438 is a CMOS variant of the YM2612 used in Sega MegaDrive(Genesis) and FM Towns.

# Features:
- Based on YM3438 die shot reverse engineering and thus provides very high emulation accuracy.

- Cycle-accurate.

- Undocumented registers/features emulation.
- SSG-EG, CSM mode emulation.
- Compatible with the YM2612.

# API Documentation
```
/* Memory management */
RN_Chip* RN_Create(uint32_t chip_type) - Allocate and initialize chip instance with type
void RN_Destroy(RN_Chip *chip) - Free chip instance
size_t RN_GetSize(void) - Get structure size in bytes

/* Core emulation */
void RN_Reset(RN_Chip *chip) - Reset emulated chip
void RN_Clock(RN_Chip *chip, int16_t *buffer) - Advance chip by 1 internal clock (6 master clocks)
void RN_Write(RN_Chip *chip, uint32_t port, uint8_t data) - Write data to chip port
uint8_t RN_Read(RN_Chip *chip, uint32_t port) - Read chip status
void RN_SetTestPin(RN_Chip *chip, uint32_t value) - Set TEST pin
uint32_t RN_ReadTestPin(RN_Chip *chip) - Read TEST pin
uint32_t RN_ReadIRQPin(RN_Chip *chip) - Read IRQ pin
```

# Samples
Sonic the Hedgehog:
https://youtu.be/ImmKy_-pJ8g

Sega CD BIOS v1.10:
https://youtu.be/s-8ASMbtojQ

