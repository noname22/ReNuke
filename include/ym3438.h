#ifndef YM3438_H
#define YM3438_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ym3438_mode_ym2612 = 0x01,      /* Enables YM2612 emulation (MD1, MD2 VA2) */
    ym3438_mode_readmode = 0x02     /* Enables status read on any port (TeraDrive, MD1 VA7, MD2, etc) */
};

#include <stdint.h>
#include <stddef.h>

typedef struct ym3438_t ym3438_t;

ym3438_t* OPN2_Create(uint32_t chip_type);
void OPN2_Destroy(ym3438_t *chip);
size_t OPN2_GetSize(void);

void OPN2_Reset(ym3438_t *chip);
void OPN2_Clock(ym3438_t *chip, int16_t *buffer);
void OPN2_Write(ym3438_t *chip, uint32_t port, uint8_t data);
void OPN2_SetTestPin(ym3438_t *chip, uint32_t value);
uint32_t OPN2_ReadTestPin(ym3438_t *chip);
uint32_t OPN2_ReadIRQPin(ym3438_t *chip);
uint8_t OPN2_Read(ym3438_t *chip, uint32_t port);

#ifdef __cplusplus
}
#endif

#endif
