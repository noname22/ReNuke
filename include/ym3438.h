/*
 * Copyright (C) 2017-2021 Alexey Khokholov (Nuke.YKT)
 *
 * This file is part of Nuked OPN2.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *  Nuked OPN2(Yamaha YM3438) emulator.
 *  Thanks:
 *      Silicon Pr0n:
 *          Yamaha YM3438 decap and die shot(digshadow).
 *      OPLx decapsulated(Matthew Gambrell, Olli Niemitalo):
 *          OPL2 ROMs.
 *
 * version: 1.0.9
 */

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


/* Opaque structure - implementation details are hidden */
typedef struct ym3438_t ym3438_t;

/* Allocation and deallocation */
ym3438_t* OPN2_Create(void);
void OPN2_Destroy(ym3438_t *chip);
size_t OPN2_GetSize(void);

void OPN2_Reset(ym3438_t *chip);
void OPN2_SetChipType(uint32_t type);
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
