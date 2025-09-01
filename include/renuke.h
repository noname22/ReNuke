#ifndef RENUKE_H
#define RENUKE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RNCM_YM2612 = 0x01,      /* Enables YM2612 emulation (MD1, MD2 VA2) */
    RNCM_READ_MODE = 0x02    /* Enables status read on any port (TeraDrive, MD1 VA7, MD2, etc) */
} RN_ChipType;

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RN_SAMPLE_RATE_NTSC 53267 // 53,267.03869047619Hz
#define RN_SAMPLE_RATE_PAL  52781 // 52,781.17460317460Hz

#define RN_LFO           0x22
#define RN_TIMERS_CH36   0x27
#define RN_DAC           0x2A
#define RN_DAC_EN        0x2B
#define RN_DT_MUL        0x30
#define RN_TOT_LEVEL     0x40
#define RN_RS_AR         0x50
#define RN_AM_D1R        0x60
#define RN_D2R           0x70
#define RN_D1L_RR        0x80
#define RN_PROP          0x90
#define RN_FEED_ALG      0xB0
#define RN_ST_LFOSEN     0xB4
#define RN_KEYONOFF      0x28
#define RN_FREQ_LSB      0xA0
#define RN_FREQ_BLOCK_MSB 0xA4

typedef struct RN_Chip RN_Chip;

RN_Chip* RN_Create(RN_ChipType chip_type);
void RN_Destroy(RN_Chip *chip);

void RN_Reset(RN_Chip *chip);
void RN_Write(RN_Chip *chip, uint32_t port, uint8_t data);
void RN_Clock(RN_Chip *chip, int clock_count);
void RN_SetTestPin(RN_Chip *chip, uint32_t value);
uint32_t RN_ReadTestPin(RN_Chip *chip);
uint32_t RN_ReadIRQPin(RN_Chip *chip);
uint8_t RN_Read(RN_Chip *chip, uint32_t port);

uint32_t RN_GetQueuedSamplesCount(RN_Chip* chip);
bool RN_DequeueSample(RN_Chip* chip, int16_t* buffer);

#ifdef __cplusplus
}
#endif

#endif
