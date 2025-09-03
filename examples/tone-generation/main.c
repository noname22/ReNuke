#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "renuke.h"

void write_le(FILE *f, size_t size, uint64_t value)
{
    for (size_t i = 0; i < size; i++)
    {
        fputc((value >> (i * 8)) & 0xFF, f);
    }
}

void write_wav_header(FILE *f, uint32_t sample_count)
{
    fwrite("RIFF", 1, 4, f);               // RIFF chunk ID
    write_le(f, 4, 36 + sample_count * 4); // File size - 8 bytes
    fwrite("WAVE", 1, 4, f);               // WAVE format
    fwrite("fmt ", 1, 4, f);               // Format chunk ID
    write_le(f, 4, 16);                    // Format chunk size
    write_le(f, 2, 1);                     // Audio format (PCM)
    write_le(f, 2, 2);                     // Number of channels
    write_le(f, 4, RN_SAMPLE_RATE_NTSC);          // Sample rate
    write_le(f, 4, RN_SAMPLE_RATE_NTSC * 4);      // Byte rate
    write_le(f, 2, 4);                     // Block align
    write_le(f, 2, 16);                    // Bits per sample
    fwrite("data", 1, 4, f);               // Data chunk ID
    write_le(f, 4, sample_count * 4);      // Data chunk size
}

void write_register(RN_Chip *chip, uint32_t part, uint32_t reg, uint8_t data)
{
    RN_Write(chip, part, (uint8_t)reg);
    RN_Clock(chip, 32);
    RN_Write(chip, part + 1, data);
    RN_Clock(chip, 32);
}

int main()
{
    uint32_t sample_count = RN_SAMPLE_RATE_NTSC * 10;
    RN_Chip *chip = RN_Create(RNCM_YM2612);

    if (!chip)
    {
        fprintf(stderr, "Failed to create chip\n");
        return 1;
    }

    RN_Reset(chip);

    // Basic global setup
    write_register(chip, 0, RN_LFO,         0x00); // LFO off
    write_register(chip, 0, RN_TIMERS_CH36, 0x00); // CH3 normal
    write_register(chip, 0, RN_DAC_EN,      0x00); // DAC off
    write_register(chip, 0, RN_KEYONOFF,    0x00); // key off ch1

    // Algorithm: 7 (all carriers), no modulation chain
    write_register(chip, 0, RN_FEED_ALG,    0x07); // FB=0, ALG=7
    write_register(chip, 0, RN_ST_LFOSEN,   0xC0); // both speakers

    // --- Make OP1 the only audible carrier; mute the others ---
    // OP1: near-sine, loud
    write_register(chip, 0, RN_DT_MUL   + 0x0, 0x01); // DT=0, MUL=1
    write_register(chip, 0, RN_TOT_LEVEL+ 0x0, 0x00); // TL=0 (max volume)
    write_register(chip, 0, RN_RS_AR    + 0x0, 0x1F); // AR=31, RS=0 (fast attack)
    write_register(chip, 0, RN_AM_D1R   + 0x0, 0x00); // D1R=0, AM=0
    write_register(chip, 0, RN_D2R      + 0x0, 0x00); // D2R=0
    write_register(chip, 0, RN_D1L_RR   + 0x0, 0x0F); // SL=0, RR=15 (holds)

    // OP2/OP3/OP4: silence them (high TL). (Still set sane rates)
    write_register(chip, 0, RN_DT_MUL   + 0x4, 0x01); // MUL=1
    write_register(chip, 0, RN_TOT_LEVEL+ 0x4, 0x7F); // TL=max (mute)
    write_register(chip, 0, RN_RS_AR    + 0x4, 0x00);
    write_register(chip, 0, RN_AM_D1R   + 0x4, 0x00);
    write_register(chip, 0, RN_D2R      + 0x4, 0x00);
    write_register(chip, 0, RN_D1L_RR   + 0x4, 0x0F);

    write_register(chip, 0, RN_DT_MUL   + 0x8, 0x01);
    write_register(chip, 0, RN_TOT_LEVEL+ 0x8, 0x7F);
    write_register(chip, 0, RN_RS_AR    + 0x8, 0x00);
    write_register(chip, 0, RN_AM_D1R   + 0x8, 0x00);
    write_register(chip, 0, RN_D2R      + 0x8, 0x00);
    write_register(chip, 0, RN_D1L_RR   + 0x8, 0x0F);

    write_register(chip, 0, RN_DT_MUL   + 0xC, 0x01);
    write_register(chip, 0, RN_TOT_LEVEL+ 0xC, 0x7F);
    write_register(chip, 0, RN_RS_AR    + 0xC, 0x00);
    write_register(chip, 0, RN_AM_D1R   + 0xC, 0x00);
    write_register(chip, 0, RN_D2R      + 0xC, 0x00);
    write_register(chip, 0, RN_D1L_RR   + 0xC, 0x0F);

    // Pitch A4 (440 Hz)
    int block = 4;
    int freq_number = 541;  // A4 from YM2612 frequency table
    write_register(chip, 0, RN_FREQ_BLOCK_MSB, (block << 3) | (freq_number >> 8));
    write_register(chip, 0, RN_FREQ_LSB, freq_number & 0xFF);

    // Key on channel 1 (all 4 ops bits set, though only OP1 is audible)
    write_register(chip, 0, RN_KEYONOFF, 0xF0);

    FILE *f = fopen("output.wav", "wb");
    if (!f)
    {
        fprintf(stderr, "Failed to open output.wav\n");
        RN_Destroy(chip);
        return 1;
    }

    write_wav_header(f, sample_count);
    int samples_written = 0;

    while(samples_written < sample_count)
    {
        int16_t b[2];
        while(RN_DequeueSamples(chip, b, 1) > 0 && samples_written < sample_count)
        {
            write_le(f, 2, b[0]);
            write_le(f, 2, b[1]);
            samples_written++;
        }

        RN_Clock(chip, 24);
    }

    fclose(f);
    RN_Destroy(chip);
    printf("Generated output.wav with YM2612 test tone\n");
    return 0;
}
