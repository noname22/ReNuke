# Nuked-OPN2
High accuracy Yamaha YM3438(OPN2) emulator.

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
ym3438_t* OPN2_Create(uint32_t chip_type) - Allocate and initialize chip instance with type
void OPN2_Destroy(ym3438_t *chip) - Free chip instance
size_t OPN2_GetSize(void) - Get structure size in bytes

/* Core emulation */
void OPN2_Reset(ym3438_t *chip) - Reset emulated chip
void OPN2_Clock(ym3438_t *chip, int16_t *buffer) - Advance chip by 1 internal clock (6 master clocks)
void OPN2_Write(ym3438_t *chip, uint32_t port, uint8_t data) - Write data to chip port
uint8_t OPN2_Read(ym3438_t *chip, uint32_t port) - Read chip status
void OPN2_SetTestPin(ym3438_t *chip, uint32_t value) - Set TEST pin
uint32_t OPN2_ReadTestPin(ym3438_t *chip) - Read TEST pin
uint32_t OPN2_ReadIRQPin(ym3438_t *chip) - Read IRQ pin
```

# Samples
Sonic the Hedgehog:
https://youtu.be/ImmKy_-pJ8g

Sega CD BIOS v1.10:
https://youtu.be/s-8ASMbtojQ

