#ifndef RENUKE_H
#define RENUKE_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ym3438_mode_ym2612 = 0x01,      /* Enables YM2612 emulation (MD1, MD2 VA2) */
    ym3438_mode_readmode = 0x02     /* Enables status read on any port (TeraDrive, MD1 VA7, MD2, etc) */
};

#include <stdint.h>
#include <stddef.h>

typedef struct RN_Chip RN_Chip;

RN_Chip* RN_Create(uint32_t chip_type);
void RN_Destroy(RN_Chip *chip);
size_t RN_GetSize(void);

void RN_Reset(RN_Chip *chip);
void RN_Clock(RN_Chip *chip, int16_t *buffer);
void RN_Write(RN_Chip *chip, uint32_t port, uint8_t data);
void RN_SetTestPin(RN_Chip *chip, uint32_t value);
uint32_t RN_ReadTestPin(RN_Chip *chip);
uint32_t RN_ReadIRQPin(RN_Chip *chip);
uint8_t RN_Read(RN_Chip *chip, uint32_t port);

#ifdef __cplusplus
}
#endif

#endif
