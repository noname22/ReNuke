#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <assert.h>

#include "renuke.h"
#include "emu76489.h"

#define ASSERT_MSG(_v, ...) if(!(_v)) { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); exit(1); }
#define CLAMP(x, low, high) (((x) < (low)) ? (low) : (((x) > (high)) ? (high) : (x)))
#define SAMPLE_RATE RN_SAMPLE_RATE_NTSC
#define MAX_DATA_BLOCKS 256
#define MAX_PCM_BANKS 40

static int8_t command_lengths[0x100] = {
    // 0x00-0x0F
    [0x00] = -2,  // Undefined
    [0x01] = -2,  // Undefined
    [0x02] = -2,  // Undefined
    [0x03] = -2,  // Undefined
    [0x04] = -2,  // Undefined
    [0x05] = -2,  // Undefined
    [0x06] = -2,  // Undefined
    [0x07] = -2,  // Undefined
    [0x08] = -2,  // Undefined
    [0x09] = -2,  // Undefined
    [0x0A] = -2,  // Undefined
    [0x0B] = -2,  // Undefined
    [0x0C] = -2,  // Undefined
    [0x0D] = -2,  // Undefined
    [0x0E] = -2,  // Undefined
    [0x0F] = -2,  // Undefined
    
    // 0x10-0x1F
    [0x10] = -2,  // Undefined
    [0x11] = -2,  // Undefined
    [0x12] = -2,  // Undefined
    [0x13] = -2,  // Undefined
    [0x14] = -2,  // Undefined
    [0x15] = -2,  // Undefined
    [0x16] = -2,  // Undefined
    [0x17] = -2,  // Undefined
    [0x18] = -2,  // Undefined
    [0x19] = -2,  // Undefined
    [0x1A] = -2,  // Undefined
    [0x1B] = -2,  // Undefined
    [0x1C] = -2,  // Undefined
    [0x1D] = -2,  // Undefined
    [0x1E] = -2,  // Undefined
    [0x1F] = -2,  // Undefined
    
    // 0x20-0x2F
    [0x20] = -2,  // Undefined
    [0x21] = -2,  // Undefined
    [0x22] = -2,  // Undefined
    [0x23] = -2,  // Undefined
    [0x24] = -2,  // Undefined
    [0x25] = -2,  // Undefined
    [0x26] = -2,  // Undefined
    [0x27] = -2,  // Undefined
    [0x28] = -2,  // Undefined
    [0x29] = -2,  // Undefined
    [0x2A] = -2,  // Undefined
    [0x2B] = -2,  // Undefined
    [0x2C] = -2,  // Undefined
    [0x2D] = -2,  // Undefined
    [0x2E] = -2,  // Undefined
    [0x2F] = -2,  // Undefined
    
    // 0x30-0x3F: one operand, reserved for future use
    [0x30] = 1,   // Reserved (1 byte operand)
    [0x31] = 1,   // Reserved (1 byte operand)
    [0x32] = 1,   // Reserved (1 byte operand)
    [0x33] = 1,   // Reserved (1 byte operand)
    [0x34] = 1,   // Reserved (1 byte operand)
    [0x35] = 1,   // Reserved (1 byte operand)
    [0x36] = 1,   // Reserved (1 byte operand)
    [0x37] = 1,   // Reserved (1 byte operand)
    [0x38] = 1,   // Reserved (1 byte operand)
    [0x39] = 1,   // Reserved (1 byte operand)
    [0x3A] = 1,   // Reserved (1 byte operand)
    [0x3B] = 1,   // Reserved (1 byte operand)
    [0x3C] = 1,   // Reserved (1 byte operand)
    [0x3D] = 1,   // Reserved (1 byte operand)
    [0x3E] = 1,   // Reserved (1 byte operand)
    [0x3F] = 1,   // Reserved (1 byte operand)
    
    // 0x40-0x4F: two operands, reserved for future use  
    [0x40] = 2,   // Reserved (2 byte operands)
    [0x41] = 2,   // Reserved (2 byte operands)
    [0x42] = 2,   // Reserved (2 byte operands)
    [0x43] = 2,   // Reserved (2 byte operands)
    [0x44] = 2,   // Reserved (2 byte operands)
    [0x45] = 2,   // Reserved (2 byte operands)
    [0x46] = 2,   // Reserved (2 byte operands)
    [0x47] = 2,   // Reserved (2 byte operands)
    [0x48] = 2,   // Reserved (2 byte operands)
    [0x49] = 2,   // Reserved (2 byte operands)
    [0x4A] = 2,   // Reserved (2 byte operands)
    [0x4B] = 2,   // Reserved (2 byte operands)
    [0x4C] = 2,   // Reserved (2 byte operands)
    [0x4D] = 2,   // Reserved (2 byte operands)
    [0x4E] = 2,   // Reserved (2 byte operands)
    [0x4F] = 1,   // Game Gear PSG stereo write
    
    // 0x50: PSG (SN76489/SN76496) write
    [0x50] = 1,   // PSG write
    
    // 0x51-0x5F: Chip writes
    [0x51] = 2,   // YM2413 write
    [0x52] = 2,   // YM2612 write port 0
    [0x53] = 2,   // YM2612 write port 1
    [0x54] = 2,   // YM2151 write
    [0x55] = 2,   // YM2203 write
    [0x56] = 2,   // YM2608 write port 0
    [0x57] = 2,   // YM2608 write port 1
    [0x58] = 2,   // YM2610 write port 0
    [0x59] = 2,   // YM2610 write port 1
    [0x5A] = 2,   // YM3812 write
    [0x5B] = 2,   // YM3526 write
    [0x5C] = 2,   // Y8950 write
    [0x5D] = 2,   // YMZ280B write
    [0x5E] = 2,   // YMF262 write port 0
    [0x5F] = 2,   // YMF262 write port 1
    
    // 0x60: Reserved
    [0x60] = -2,  // Undefined
    
    // 0x61: Wait n samples
    [0x61] = 2,   // Wait n samples
    
    // 0x62-0x63: Wait fixed samples
    [0x62] = 0,   // Wait 735 samples (1/60 second)
    [0x63] = 0,   // Wait 882 samples (1/50 second)
    
    // 0x64-0x65: Reserved
    [0x64] = 3,   // Reserved (3 byte operands)
    [0x65] = 3,   // Reserved (3 byte operands)
    
    // 0x66: End of sound data
    [0x66] = 0,   // End of sound data
    
    // 0x67: Data block
    [0x67] = -1,  // Data block (variable length)
    
    // 0x68: PCM RAM write
    [0x68] = -1,  // PCM RAM write (variable length)
    
    // 0x69: Reserved
    [0x69] = -2,  // Undefined
    
    // 0x6A-0x6F: Reserved
    [0x6A] = -2,  // Undefined
    [0x6B] = -2,  // Undefined
    [0x6C] = -2,  // Undefined
    [0x6D] = -2,  // Undefined
    [0x6E] = -2,  // Undefined
    [0x6F] = -2,  // Undefined
    
    // 0x70-0x7F: Wait n+1 samples
    [0x70] = 0,   // Wait 1 sample
    [0x71] = 0,   // Wait 2 samples
    [0x72] = 0,   // Wait 3 samples
    [0x73] = 0,   // Wait 4 samples
    [0x74] = 0,   // Wait 5 samples
    [0x75] = 0,   // Wait 6 samples
    [0x76] = 0,   // Wait 7 samples
    [0x77] = 0,   // Wait 8 samples
    [0x78] = 0,   // Wait 9 samples
    [0x79] = 0,   // Wait 10 samples
    [0x7A] = 0,   // Wait 11 samples
    [0x7B] = 0,   // Wait 12 samples
    [0x7C] = 0,   // Wait 13 samples
    [0x7D] = 0,   // Wait 14 samples
    [0x7E] = 0,   // Wait 15 samples
    [0x7F] = 0,   // Wait 16 samples
    
    // 0x80-0x8F: YM2612 DAC write + wait
    [0x80] = 0,   // YM2612 DAC write + wait 0 samples
    [0x81] = 0,   // YM2612 DAC write + wait 1 sample
    [0x82] = 0,   // YM2612 DAC write + wait 2 samples
    [0x83] = 0,   // YM2612 DAC write + wait 3 samples
    [0x84] = 0,   // YM2612 DAC write + wait 4 samples
    [0x85] = 0,   // YM2612 DAC write + wait 5 samples
    [0x86] = 0,   // YM2612 DAC write + wait 6 samples
    [0x87] = 0,   // YM2612 DAC write + wait 7 samples
    [0x88] = 0,   // YM2612 DAC write + wait 8 samples
    [0x89] = 0,   // YM2612 DAC write + wait 9 samples
    [0x8A] = 0,   // YM2612 DAC write + wait 10 samples
    [0x8B] = 0,   // YM2612 DAC write + wait 11 samples
    [0x8C] = 0,   // YM2612 DAC write + wait 12 samples
    [0x8D] = 0,   // YM2612 DAC write + wait 13 samples
    [0x8E] = 0,   // YM2612 DAC write + wait 14 samples
    [0x8F] = 0,   // YM2612 DAC write + wait 15 samples
    
    // 0x90-0x95: DAC Stream Control
    [0x90] = 4,   // DAC Stream Control - Setup Stream Control
    [0x91] = 4,   // DAC Stream Control - Set Stream Data
    [0x92] = 5,   // DAC Stream Control - Set Stream Frequency
    [0x93] = 10,  // DAC Stream Control - Start Stream
    [0x94] = 1,   // DAC Stream Control - Stop Stream
    [0x95] = 4,   // DAC Stream Control - Start Stream (fast call)
    
    // 0x96-0x9F: Reserved
    [0x96] = -2,  // Undefined
    [0x97] = -2,  // Undefined
    [0x98] = -2,  // Undefined
    [0x99] = -2,  // Undefined
    [0x9A] = -2,  // Undefined
    [0x9B] = -2,  // Undefined
    [0x9C] = -2,  // Undefined
    [0x9D] = -2,  // Undefined
    [0x9E] = -2,  // Undefined
    [0x9F] = -2,  // Undefined
    
    // 0xA0-0xAF: AY8910 chip writes
    [0xA0] = 2,   // AY8910 write
    [0xA1] = 2,   // Reserved (2 byte operands)
    [0xA2] = 2,   // Reserved (2 byte operands)
    [0xA3] = 2,   // Reserved (2 byte operands)
    [0xA4] = 2,   // Reserved (2 byte operands)
    [0xA5] = 2,   // Reserved (2 byte operands)
    [0xA6] = 2,   // Reserved (2 byte operands)
    [0xA7] = 2,   // Reserved (2 byte operands)
    [0xA8] = 2,   // Reserved (2 byte operands)
    [0xA9] = 2,   // Reserved (2 byte operands)
    [0xAA] = 2,   // Reserved (2 byte operands)
    [0xAB] = 2,   // Reserved (2 byte operands)
    [0xAC] = 2,   // Reserved (2 byte operands)
    [0xAD] = 2,   // Reserved (2 byte operands)
    [0xAE] = 2,   // Reserved (2 byte operands)
    [0xAF] = 2,   // Reserved (2 byte operands)
    
    // 0xB0-0xBF: Chip writes
    [0xB0] = 2,   // RF5C68 write
    [0xB1] = 2,   // RF5C164 write
    [0xB2] = 2,   // PWM write
    [0xB3] = 2,   // GameBoy DMG write
    [0xB4] = 2,   // NES APU write
    [0xB5] = 2,   // MultiPCM write
    [0xB6] = 2,   // uPD7759 write
    [0xB7] = 2,   // OKIM6258 write
    [0xB8] = 2,   // OKIM6295 write
    [0xB9] = 2,   // HuC6280 write
    [0xBA] = 2,   // K053260 write
    [0xBB] = 2,   // Pokey write
    [0xBC] = 2,   // WonderSwan write
    [0xBD] = 2,   // SAA1099 write
    [0xBE] = 2,   // ES5506 write
    [0xBF] = 2,   // GA20 write
    
    // 0xC0-0xCF: Sega PCM/memory writes
    [0xC0] = 3,   // Sega PCM write
    [0xC1] = 3,   // RF5C68 memory write
    [0xC2] = 3,   // RF5C164 memory write
    [0xC3] = 3,   // MultiPCM memory write
    [0xC4] = 3,   // QSound write
    [0xC5] = 3,   // SCSP write
    [0xC6] = 3,   // WonderSwan memory write
    [0xC7] = 3,   // VSU write
    [0xC8] = 3,   // X1-010 write
    [0xC9] = 3,   // Reserved (3 byte operands)
    [0xCA] = 3,   // Reserved (3 byte operands)
    [0xCB] = 3,   // Reserved (3 byte operands)
    [0xCC] = 3,   // Reserved (3 byte operands)
    [0xCD] = 3,   // Reserved (3 byte operands)
    [0xCE] = 3,   // Reserved (3 byte operands)
    [0xCF] = 3,   // Reserved (3 byte operands)
    
    // 0xD0-0xDF: Chip writes
    [0xD0] = 3,   // YMF278B write
    [0xD1] = 3,   // YMF271 write
    [0xD2] = 3,   // K051649 write
    [0xD3] = 3,   // K054539 write
    [0xD4] = 3,   // C140 write
    [0xD5] = 3,   // ES5503 write
    [0xD6] = 3,   // ES5506 write
    [0xD7] = 3,   // Reserved (3 byte operands)
    [0xD8] = 3,   // Reserved (3 byte operands)
    [0xD9] = 3,   // Reserved (3 byte operands)
    [0xDA] = 3,   // Reserved (3 byte operands)
    [0xDB] = 3,   // Reserved (3 byte operands)
    [0xDC] = 3,   // Reserved (3 byte operands)
    [0xDD] = 3,   // Reserved (3 byte operands)
    [0xDE] = 3,   // Reserved (3 byte operands)
    [0xDF] = 3,   // Reserved (3 byte operands)
    
    // 0xE0: Seek to offset in PCM data bank
    [0xE0] = 4,   // Seek to offset in PCM data bank
    
    // 0xE1-0xEF: C352/ES5503/ES5506 writes
    [0xE1] = 4,   // C352 write
    [0xE2] = 4,   // Reserved (4 byte operands)
    [0xE3] = 4,   // Reserved (4 byte operands)
    [0xE4] = 4,   // Reserved (4 byte operands)
    [0xE5] = 4,   // Reserved (4 byte operands)
    [0xE6] = 4,   // Reserved (4 byte operands)
    [0xE7] = 4,   // Reserved (4 byte operands)
    [0xE8] = 4,   // Reserved (4 byte operands)
    [0xE9] = 4,   // Reserved (4 byte operands)
    [0xEA] = 4,   // Reserved (4 byte operands)
    [0xEB] = 4,   // Reserved (4 byte operands)
    [0xEC] = 4,   // Reserved (4 byte operands)
    [0xED] = 4,   // Reserved (4 byte operands)
    [0xEE] = 4,   // Reserved (4 byte operands)
    [0xEF] = 4,   // Reserved (4 byte operands)
    
    // 0xF0-0xFF: Reserved
    [0xF0] = 4,   // Reserved (4 byte operands)
    [0xF1] = 4,   // Reserved (4 byte operands)
    [0xF2] = 4,   // Reserved (4 byte operands)
    [0xF3] = 4,   // Reserved (4 byte operands)
    [0xF4] = 4,   // Reserved (4 byte operands)
    [0xF5] = 4,   // Reserved (4 byte operands)
    [0xF6] = 4,   // Reserved (4 byte operands)
    [0xF7] = 4,   // Reserved (4 byte operands)
    [0xF8] = 4,   // Reserved (4 byte operands)
    [0xF9] = 4,   // Reserved (4 byte operands)
    [0xFA] = 4,   // Reserved (4 byte operands)
    [0xFB] = 4,   // Reserved (4 byte operands)
    [0xFC] = 4,   // Reserved (4 byte operands)
    [0xFD] = 4,   // Reserved (4 byte operands)
    [0xFE] = 4,   // Reserved (4 byte operands)
    [0xFF] = 4,   // Reserved (4 byte operands)
};

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint8_t type;
} DataBlock;

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t pos;
    
    // Header fields
    uint32_t version;
    uint32_t eof_offset;
    uint32_t gd3_offset;
    uint32_t sample_count;
    uint32_t loop_offset;
    uint32_t loop_samples;
    uint32_t rate;
    uint32_t data_offset;
    
    // Chip clocks
    uint32_t sn76489_clock;
    uint32_t ym2413_clock;
    uint32_t ym2612_clock;
    uint32_t ym2151_clock;
    uint32_t sega_pcm_clock;
    uint32_t rf5c68_clock;
    uint32_t ym2203_clock;
    uint32_t ym2608_clock;
    uint32_t ym2610_clock;
    uint32_t ym3812_clock;
    uint32_t ym3526_clock;
    uint32_t y8950_clock;
    uint32_t ymf262_clock;
    uint32_t ymf278b_clock;
    uint32_t ymf271_clock;
    uint32_t ymz280b_clock;
    uint32_t rf5c164_clock;
    uint32_t pwm_clock;
    uint32_t ay8910_clock;
    uint32_t gb_dmg_clock;
    uint32_t nes_apu_clock;
    uint32_t multipcm_clock;
    uint32_t upd7759_clock;
    uint32_t okim6258_clock;
    uint32_t okim6295_clock;
    uint32_t k051649_clock;
    uint32_t k054539_clock;
    uint32_t huc6280_clock;
    uint32_t c140_clock;
    uint32_t k053260_clock;
    uint32_t pokey_clock;
    uint32_t qsound_clock;
    
    // Version 1.51+ fields
    uint8_t sn76489_feedback;
    uint8_t sn76489_shift_width;
    uint8_t sn76489_flags;
    
    // Volume modifier
    uint8_t volume_modifier;
    
    // Loop modifier  
    uint8_t loop_modifier;
    
    // Extra header
    uint32_t extra_header_offset;
    
    // Data blocks
    DataBlock data_blocks[MAX_DATA_BLOCKS];
    uint32_t data_block_count;
    
    // PCM data banks
    uint8_t *pcm_banks[MAX_PCM_BANKS];
    uint32_t pcm_bank_sizes[MAX_PCM_BANKS];
    uint32_t pcm_bank_pos[MAX_PCM_BANKS];
} VGMFile;

typedef struct {
    uint32_t frequency;
    uint32_t position;
    uint32_t length;
    uint8_t bank_id;
    uint8_t step_size;
    uint8_t step_base;
    bool playing;
    bool reverse;
    bool loop;
    uint8_t chip_type;
    uint8_t chip_id;
    uint8_t port;
    uint8_t channel;
} DACStream;

typedef struct {
    // Chip instances (single chip only)
    RN_Chip *ym2612;
    SNG *psg;
    
    // VGM file
    VGMFile *vgm;
    
    // Playback state
    uint32_t samples_played;
    uint32_t wait_samples;
    bool paused;
    bool loop_enabled;
    
    // DAC streaming
    DACStream dac_streams[256];
    uint8_t dac_stream_count;
    
    // Sample rate conversion
    double resample_ratio;
    
    // Audio buffer
    float *audio_buffer;
    size_t audio_buffer_size;

    int last_ym_port;
    int last_ym_reg;
} PlayerState;

static uint32_t read_u32_le(const uint8_t *data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *data) {
    return data[0] | (data[1] << 8);
}

static uint8_t read_u8(const uint8_t *data) {
    return data[0];
}

static VGMFile* load_vgm(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    if (memcmp(data, "Vgm ", 4) != 0) {
        fprintf(stderr, "Not a VGM file\n");
        free(data);
        return NULL;
    }

    VGMFile *vgm = calloc(1, sizeof(VGMFile));
    if (!vgm) {
        free(data);
        return NULL;
    }
    
    vgm->data = data;
    vgm->size = size;
    
    // Parse header
    vgm->eof_offset = read_u32_le(data + 0x04);
    vgm->version = read_u32_le(data + 0x08);
    vgm->sn76489_clock = read_u32_le(data + 0x0C);
    vgm->ym2413_clock = read_u32_le(data + 0x10);
    vgm->gd3_offset = read_u32_le(data + 0x14);
    vgm->sample_count = read_u32_le(data + 0x18);
    vgm->loop_offset = read_u32_le(data + 0x1C);
    vgm->loop_samples = read_u32_le(data + 0x20);
    
    // Version 1.01+ fields
    if (vgm->version >= 0x101) {
        vgm->rate = read_u32_le(data + 0x24);
    }
    
    // Version 1.10+ fields  
    if (vgm->version >= 0x110) {
        vgm->sn76489_feedback = read_u16_le(data + 0x28);
        vgm->sn76489_shift_width = read_u8(data + 0x2A);
        
        if (vgm->version >= 0x151) {
            vgm->sn76489_flags = read_u8(data + 0x2B);
        }
        
        vgm->ym2612_clock = read_u32_le(data + 0x2C);
        vgm->ym2151_clock = read_u32_le(data + 0x30);
    }
    
    // Version 1.50+ fields
    if (vgm->version >= 0x150) {
        uint32_t vgm_data_offset = read_u32_le(data + 0x34);
        if (vgm_data_offset) {
            vgm->data_offset = 0x34 + vgm_data_offset;
        } else {
            vgm->data_offset = 0x40;
        }
    } else {
        vgm->data_offset = 0x40;
    }
    
    // Version 1.51+ fields
    if (vgm->version >= 0x151) {
        vgm->sega_pcm_clock = read_u32_le(data + 0x38);
        vgm->rf5c68_clock = read_u32_le(data + 0x40);
        vgm->ym2203_clock = read_u32_le(data + 0x44);
        vgm->ym2608_clock = read_u32_le(data + 0x48);
        vgm->ym2610_clock = read_u32_le(data + 0x4C);
        vgm->ym3812_clock = read_u32_le(data + 0x50);
        vgm->ym3526_clock = read_u32_le(data + 0x54);
        vgm->y8950_clock = read_u32_le(data + 0x58);
        vgm->ymf262_clock = read_u32_le(data + 0x5C);
        vgm->ymf278b_clock = read_u32_le(data + 0x60);
        vgm->ymf271_clock = read_u32_le(data + 0x64);
        vgm->ymz280b_clock = read_u32_le(data + 0x68);
        vgm->rf5c164_clock = read_u32_le(data + 0x6C);
        vgm->pwm_clock = read_u32_le(data + 0x70);
        vgm->ay8910_clock = read_u32_le(data + 0x74);
        
        if (vgm->version >= 0x160) {
            vgm->volume_modifier = read_u8(data + 0x7C);
            vgm->loop_modifier = read_u8(data + 0x7F);
        }
        
        if (vgm->version >= 0x161) {
            vgm->gb_dmg_clock = read_u32_le(data + 0x80);
            vgm->nes_apu_clock = read_u32_le(data + 0x84);
            vgm->multipcm_clock = read_u32_le(data + 0x88);
            vgm->upd7759_clock = read_u32_le(data + 0x8C);
            vgm->okim6258_clock = read_u32_le(data + 0x90);
            vgm->okim6295_clock = read_u32_le(data + 0x98);
            vgm->k051649_clock = read_u32_le(data + 0x9C);
            vgm->k054539_clock = read_u32_le(data + 0xA0);
            vgm->huc6280_clock = read_u32_le(data + 0xA4);
            vgm->c140_clock = read_u32_le(data + 0xA8);
            vgm->k053260_clock = read_u32_le(data + 0xAC);
            vgm->pokey_clock = read_u32_le(data + 0xB0);
            vgm->qsound_clock = read_u32_le(data + 0xB4);
        }
        
        if (vgm->version >= 0x170) {
            vgm->extra_header_offset = read_u32_le(data + 0xBC);
        }
    }
    
    vgm->pos = vgm->data_offset;
    
    printf("VGM Version: %x.%02x\n", (vgm->version >> 8) & 0xFF, vgm->version & 0xFF);
    if (vgm->ym2612_clock) printf("YM2612 Clock: %u Hz\n", vgm->ym2612_clock & 0x3FFFFFFF);
    if (vgm->sn76489_clock) printf("SN76489 Clock: %u Hz\n", vgm->sn76489_clock & 0x3FFFFFFF);
    printf("Sample Count: %u\n", vgm->sample_count);
    if (vgm->loop_offset) printf("Loop Point: Sample %u\n", vgm->loop_samples);
    
    return vgm;
}

static void process_data_block(VGMFile *vgm, PlayerState *state) {
    if (vgm->pos + 6 >= vgm->size) return;
    uint8_t b = vgm->data[vgm->pos++];
    assert(b == 0x66);

    uint8_t block_type = vgm->data[vgm->pos++];
    uint32_t block_size = read_u32_le(&vgm->data[vgm->pos]);
    vgm->pos += 4;
    
    if (vgm->pos + block_size > vgm->size) {
        vgm->pos = vgm->size;
        return;
    }
    
    // Store data block
    if (vgm->data_block_count < MAX_DATA_BLOCKS) {
        vgm->data_blocks[vgm->data_block_count].type = block_type;
        vgm->data_blocks[vgm->data_block_count].data = &vgm->data[vgm->pos];
        vgm->data_blocks[vgm->data_block_count].size = block_size;
        vgm->data_block_count++;
    }
    
    ASSERT_MSG(block_type <= 0x3F, "Only uncompressed data streams supported");

    // Handle specific block types
    if (block_type <= 0x3F) {
        // Uncompressed YM2612 PCM data
        
        ASSERT_MSG(block_type < MAX_PCM_BANKS, "Too many data banks")

        vgm->pcm_banks[block_type] = vgm->data + vgm->pos;
        vgm->pcm_bank_sizes[block_type] = block_size;
        vgm->pcm_bank_pos[block_type] = 0;
    }
    
    vgm->pos += block_size;
}

static void process_dac_stream_control(VGMFile *vgm, PlayerState *state) {
    if (vgm->pos + 4 >= vgm->size) return;
    
    uint8_t stream_id = vgm->data[vgm->pos++];
    uint8_t chip_type = vgm->data[vgm->pos++];
    uint8_t port_channel = vgm->data[vgm->pos++];
    vgm->pos++; // command byte (unused)
    
    DACStream *stream = &state->dac_streams[stream_id];
    stream->chip_type = chip_type;
    stream->port = (port_channel >> 4) & 0x0F;
    stream->channel = port_channel & 0x0F;
}

static void process_dac_stream_data(VGMFile *vgm, PlayerState *state) {
    if (vgm->pos + 4 >= vgm->size) return;
    
    uint8_t stream_id = vgm->data[vgm->pos++];
    uint8_t bank_id = vgm->data[vgm->pos++];
    uint8_t step_size = vgm->data[vgm->pos++];
    uint8_t step_base = vgm->data[vgm->pos++];
    
    DACStream *stream = &state->dac_streams[stream_id];
    stream->bank_id = bank_id;
    stream->step_size = step_size;
    stream->step_base = step_base;
}

static void process_dac_stream_frequency(VGMFile *vgm, PlayerState *state) {
    if (vgm->pos + 5 >= vgm->size) return;
    
    uint8_t stream_id = vgm->data[vgm->pos++];
    uint32_t frequency = read_u32_le(&vgm->data[vgm->pos]);
    vgm->pos += 4;
    
    state->dac_streams[stream_id].frequency = frequency;
}

static void process_dac_stream_start(VGMFile *vgm, PlayerState *state, bool fast) {
    uint32_t bytes_needed = fast ? 11 : 11;
    if (vgm->pos + bytes_needed >= vgm->size) return;
    
    uint8_t stream_id = vgm->data[vgm->pos++];
    uint32_t data_start = read_u32_le(&vgm->data[vgm->pos]);
    vgm->pos += 4;
    uint8_t length_mode = vgm->data[vgm->pos++];
    uint32_t data_length = read_u32_le(&vgm->data[vgm->pos]);
    vgm->pos += 4;
    
    if (!fast) {
        // Additional non-fast parameters would be read here
    }
    
    DACStream *stream = &state->dac_streams[stream_id];
    stream->position = data_start;
    stream->length = data_length;
    stream->playing = true;
    stream->loop = (length_mode & 0x01) != 0;
    stream->reverse = (length_mode & 0x10) != 0;
}

static void process_dac_stream_stop(VGMFile *vgm, PlayerState *state) {
    if (vgm->pos >= vgm->size) return;
    
    uint8_t stream_id = vgm->data[vgm->pos++];
    state->dac_streams[stream_id].playing = false;
}

void ym_write(PlayerState *state, uint32_t port, uint8_t reg, uint8_t val)
{
    if (state->ym2612)
    {

        if (state->last_ym_port != port || state->last_ym_reg != reg)
        {
            RN_Write(state->ym2612, port, reg);
            RN_Clock(state->ym2612, 12); // Timing delay

            state->last_ym_reg = reg;
            state->last_ym_port = port;
        }

        RN_Write(state->ym2612, port + 1, val);

        if(port != 0 || reg != 0x2A)
        {
            RN_Clock(state->ym2612, 32); // Timing delay
        }
    }
}

static void process_vgm_command(VGMFile *vgm, PlayerState *state)
{
    if (vgm->pos >= vgm->size) return;
    
    uint32_t start_pos = vgm->pos;
    uint8_t cmd = vgm->data[vgm->pos++];
    
    //printf("cmd %02x %d/%d\n", cmd, vgm->pos, vgm->size);
    
    // Get expected command length
    int8_t expected_length = command_lengths[cmd];
    if (expected_length == -2) {
        fprintf(stderr, "Warning: Undefined command 0x%02X at offset 0x%X\n", cmd, start_pos);
    }

    switch (cmd) {
        // Dual chip commands (second chip)
        case 0x30 ... 0x3F:
            // Second chip commands - skip data
            if (vgm->pos < vgm->size) {
                vgm->pos++; // Skip data byte
            }
            break;
            
        // Reserved
        case 0x40 ... 0x4E:
            vgm->pos += 2; // Skip 2 operands
            break;
            
        case 0x4F: // Game Gear PSG stereo
            if (vgm->pos < vgm->size) {
                uint8_t val = vgm->data[vgm->pos++];
                if (state->psg) {
                    SNG_writeGGIO(state->psg, val);
                }
            }
            break;
            
        case 0x50: // PSG write
            if (vgm->pos < vgm->size) {
                uint8_t val = vgm->data[vgm->pos++];
                if (state->psg) {
                    SNG_writeIO(state->psg, val);
                }
            }
            break;
            
        case 0x51: // YM2413 write
            if (vgm->pos + 1 < vgm->size) {
                vgm->pos += 2; // Skip reg and val (YM2413 not implemented)
            }
            break;
            
        case 0x52: // YM2612 port 0 write
        case 0x53: // YM2612 port 1 write
            if (vgm->pos + 1 < vgm->size) {
                uint8_t reg = vgm->data[vgm->pos++];
                uint8_t val = vgm->data[vgm->pos++];
                uint32_t port = (cmd == 0x52) ? 0 : 2;
                ym_write(state, port, reg, val);
            }
            break;
            
        case 0x54: // YM2151 write
            if (vgm->pos + 1 < vgm->size) {
                vgm->pos += 2; // Skip reg and val (YM2151 not implemented)
            }
            break;
            
        case 0x55: // YM2203 write
        case 0x56: // YM2608 port 0 write
        case 0x57: // YM2608 port 1 write
        case 0x58: // YM2610 port 0 write
        case 0x59: // YM2610 port 1 write
        case 0x5A: // YM3812 write
        case 0x5B: // YM3526 write
        case 0x5C: // Y8950 write
        case 0x5D: // YMZ280B write
        case 0x5E: // YMF262 port 0 write
        case 0x5F: // YMF262 port 1 write
            if (vgm->pos + 1 < vgm->size) {
                vgm->pos += 2; // Skip register and value
            }
            break;
            
        case 0x61: // Wait n samples
            if (vgm->pos + 1 < vgm->size) {
                uint16_t vgm_samples = read_u16_le(&vgm->data[vgm->pos]);
                state->wait_samples = (uint32_t)(vgm_samples * state->resample_ratio);
                vgm->pos += 2;
            }
            break;
            
        case 0x62: // Wait 735 samples (1/60 second)
            state->wait_samples = (uint32_t)(735 * state->resample_ratio);
            break;
            
        case 0x63: // Wait 882 samples (1/50 second)
            state->wait_samples = (uint32_t)(882 * state->resample_ratio);
            break;
            
        case 0x66: // End of sound data
            if (state->loop_enabled && vgm->loop_offset) {
                vgm->pos = 0x1C + vgm->loop_offset;
            } else {
                vgm->pos = vgm->size;
            }
            break;
            
        case 0x67: // Data block
            process_data_block(vgm, state);
            break;
            
        case 0x68: // PCM RAM write
            if (vgm->pos + 10 < vgm->size) {
                assert(vgm->data[vgm->pos] == 0x66);
                vgm->pos++; // Skip 0x66
                vgm->pos++; // Skip chip_type
                vgm->pos += 3; // Skip offset
                uint32_t size = read_u32_le(&vgm->data[vgm->pos]) & 0xFFFFFF;
                vgm->pos += 3;
                
                if (vgm->pos + size <= vgm->size) {
                    // Handle PCM RAM write based on chip type
                    vgm->pos += size;
                }
            }
            break;
            
        case 0x70 ... 0x7F: // Wait 1-16 samples
            state->wait_samples = (uint32_t)(((cmd & 0x0F) + 1) * state->resample_ratio);
            break;
            
        case 0x80 ... 0x8F: // YM2612 DAC write + wait
            state->wait_samples = (uint32_t)((cmd & 0x0F) * state->resample_ratio);
            // Read from data bank 0 at current position
            if (vgm->pcm_banks[0] && vgm->pcm_bank_pos[0] < vgm->pcm_bank_sizes[0]) {
                uint8_t data = vgm->pcm_banks[0][vgm->pcm_bank_pos[0]++];
                ym_write(state, 0, 0x2A, data);
            }
            break;
            
        case 0x90: // DAC Stream Control - Setup
            printf("dac %02x", cmd);
            process_dac_stream_control(vgm, state);
            break;
            
        case 0x91: // DAC Stream Control - Set Data
            printf("dac %02x", cmd);
            process_dac_stream_data(vgm, state);
            break;
            
        case 0x92: // DAC Stream Control - Set Frequency
            printf("dac %02x", cmd);
            process_dac_stream_frequency(vgm, state);
            break;
            
        case 0x93: // DAC Stream Control - Start Stream
            printf("dac %02x", cmd);
            process_dac_stream_start(vgm, state, false);
            break;
            
        case 0x94: // DAC Stream Control - Stop Stream
            printf("dac %02x", cmd);
            process_dac_stream_stop(vgm, state);
            break;
            
        case 0x95: // DAC Stream Control - Start Stream (fast)
            printf("dac %02x", cmd);
            process_dac_stream_start(vgm, state, true);
            break;
            
        case 0xA0: // AY8910 write
            if (vgm->pos + 1 < vgm->size) {
                vgm->pos += 2; // Skip reg and val (AY8910 not implemented)
            }
            break;
            
        case 0xB0 ... 0xBF: // Various chip writes
            if (vgm->pos + 1 < vgm->size) {
                vgm->pos += 2; // Skip register and value
            }
            break;
            
        case 0xC0 ... 0xDF: // Various memory writes
            if (vgm->pos + 2 < vgm->size) {
                vgm->pos += 3; // Skip 3 bytes (these commands have 3 byte operands)
            }
            break;
            
        case 0xE0: // Seek to offset in PCM data bank
            if (vgm->pos + 3 < vgm->size) {
                uint32_t offset = read_u32_le(&vgm->data[vgm->pos]);
                vgm->pos += 4;
                uint8_t bank_id = (offset >> 24) & 0x7F;
                uint32_t bank_offset = offset & 0xFFFFFF;
                
                if (bank_id < MAX_PCM_BANKS && vgm->pcm_banks[bank_id]) {
                    vgm->pcm_bank_pos[bank_id] = bank_offset;
                }
            }
            break;
            
        case 0xE1 ... 0xFF: // Reserved/chip specific
            // Skip various amounts based on command  
            if (vgm->pos + 3 < vgm->size) {
                vgm->pos += 4; // Most E1-FF commands have 4 byte operands
            }
            break;
            
        default:
            fprintf(stderr, "Unknown VGM command: 0x%02X at offset 0x%X\n", cmd, (unsigned)start_pos);
            // Try to skip based on command_lengths if defined
            if (expected_length > 0 && vgm->pos + expected_length <= vgm->size) {
                vgm->pos += expected_length;
            }
            break;
    }
    
    // Verify position was advanced correctly (except for variable length and undefined commands)
    if (expected_length >= 0) {
        uint32_t actual_advance = vgm->pos - start_pos - 1; // -1 for the command byte itself
        uint32_t expected_advance = expected_length;
        
        ASSERT_MSG(actual_advance == expected_advance, "Command 0x%02X at offset 0x%X advanced %u bytes but expected %u bytes", cmd, start_pos, actual_advance, expected_advance);
    }
}

static void audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    PlayerState *state = (PlayerState*)userdata;
    
    if (state->paused || !state->vgm) {
        return;
    }
    
    int samples_needed = additional_amount / (sizeof(float) * 2);
    
    // Reallocate buffer if needed
    if (state->audio_buffer_size < additional_amount) {
        state->audio_buffer = realloc(state->audio_buffer, additional_amount);
        ASSERT_MSG(state->audio_buffer, "Failed to allocate audio buffer");
        state->audio_buffer_size = additional_amount;
    }
    
    int samples_written = 0;
    int total_samples_waited = 0;
    float* cur_buffer = state->audio_buffer;
    
    while (samples_written < samples_needed) {
        // Process VGM commands if no wait pending
        int command_count = 0;
        while (state->wait_samples == 0 && state->vgm->pos < state->vgm->size) {
            process_vgm_command(state->vgm, state);
            command_count++;
            total_samples_waited += state->wait_samples;
        }
        
        // Generate samples while we have queued samples and need more output
        while (RN_GetQueuedSamplesCount(state->ym2612) > 0 && samples_written < samples_needed)
        {
            int32_t psg_out[2] = {0, 0};
            int16_t ym_out[2] = {0, 0};
            
            // Get YM2612 sample
            if (state->ym2612) {
                RN_DequeueSample(state->ym2612, ym_out);
            }
            
            // Generate PSG sample
            if (state->psg) {
                SNG_calc_stereo(state->psg, psg_out);
            }
            
            float ym_gain = 32.0f;
            float psg_gain = 4.0f;

            // Mix outputs
            *(cur_buffer++) = (ym_out[0] / 65535.0f) * ym_gain + (psg_out[0] / 65535.0f) * psg_gain;
            *(cur_buffer++) = (ym_out[1] / 65535.0f) * ym_gain + (psg_out[1] / 65535.0f) * psg_gain;
            
            // Apply volume modifier if present
            // if (state->vgm->volume_modifier) {
            //     l = (left * state->vgm->volume_modifier) / 256;
            //     right = (right * state->vgm->volume_modifier) / 256;
            // }

            samples_written++;
            
            if (state->wait_samples > 0) {
                state->wait_samples--;
            }

            state->samples_played++;
        }
        
        // Clock the chip to generate more samples if needed
        if (samples_written < samples_needed) {
            RN_Clock(state->ym2612, 1);
        }
    }
    
    SDL_PutAudioStreamData(stream, state->audio_buffer, additional_amount);
}

static void print_gd3_info(VGMFile *vgm) {
    if (!vgm->gd3_offset || vgm->gd3_offset + 0x14 >= vgm->size) {
        return;
    }
    
    uint8_t *gd3 = vgm->data + 0x14 + vgm->gd3_offset;
    if (memcmp(gd3, "Gd3 ", 4) != 0) {
        return;
    }
    
    uint32_t gd3_version = read_u32_le(gd3 + 4);
    // uint32_t gd3_length = read_u32_le(gd3 + 8);
    
    // GD3 contains null-terminated UTF-16 strings
    // Would need proper UTF-16 parsing for full implementation
    printf("GD3 Tag Version: %x.%02x\n", (gd3_version >> 8) & 0xFF, gd3_version & 0xFF);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <vgm_file>\n", argv[0]);
        return 1;
    }
    
    VGMFile *vgm = load_vgm(argv[1]);
    if (!vgm) {
        return 1;
    }
    
    print_gd3_info(vgm);
    
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        free(vgm->data);
        free(vgm);
        return 1;
    }
    
    SDL_Window *window = SDL_CreateWindow("VGM Player", 400, 300, 0);
    if (!window) {
        fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
        free(vgm->data);
        free(vgm);
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        free(vgm->data);
        free(vgm);
        SDL_Quit();
        return 1;
    }
    
    PlayerState state = {0};
    state.last_ym_reg = -1;
    state.last_ym_port = -1;
    state.vgm = vgm;
    state.loop_enabled = true;
    
    // Use YM2612 native sample rate - no resampling needed
    state.resample_ratio = SAMPLE_RATE / (double)44100;
    
    // Create chip emulators (single chip only)
    if (vgm->ym2612_clock & 0x3FFFFFFF) {
        state.ym2612 = RN_Create(RNCM_YM2612);
        if (state.ym2612) {
            RN_Reset(state.ym2612);
        }
        
        // Warn if dual chip detected but ignore it
        if (vgm->ym2612_clock & 0x40000000) {
            printf("Warning: Dual YM2612 detected but not supported - using first chip only\n");
        }
    }
    
    if (vgm->sn76489_clock & 0x3FFFFFFF) {
        state.psg = SNG_new(vgm->sn76489_clock & 0x3FFFFFFF, SAMPLE_RATE);
        if (state.psg) {
            SNG_reset(state.psg);
            
            // Apply version-specific PSG settings
            if (vgm->version >= 0x110) {
                // Configure PSG noise feedback pattern and shift register width
                // The feedback is typically written to register 0xE0-0xE3
                if (vgm->sn76489_feedback) {
                    // Write feedback pattern (16-bit value)
                    SNG_writeIO(state.psg, 0xE0 | (vgm->sn76489_feedback & 0x0F));
                    SNG_writeIO(state.psg, 0xE4 | ((vgm->sn76489_feedback >> 4) & 0x0F));
                    SNG_writeIO(state.psg, 0xE8 | ((vgm->sn76489_feedback >> 8) & 0x0F));
                    SNG_writeIO(state.psg, 0xEC | ((vgm->sn76489_feedback >> 12) & 0x0F));
                }
                
                // Configure shift register width
                if (vgm->sn76489_shift_width) {
                    // Shift width is typically 15 or 16 bits
                    SNG_writeIO(state.psg, 0xF0 | (vgm->sn76489_shift_width & 0x0F));
                }
            }
        }
        
        // Warn if dual chip detected but ignore it
        if (vgm->sn76489_clock & 0x40000000) {
            printf("Warning: Dual SN76489 detected but not supported - using first chip only\n");
        }
    }
    
    // Setup audio
    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_F32LE,
        .channels = 2,
        .freq = SAMPLE_RATE,
    };
    
    SDL_AudioStream *audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, &state);
    if (!audio_stream) {
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        if (state.ym2612) RN_Destroy(state.ym2612);
        if (state.psg) SNG_delete(state.psg);
        free(vgm->data);
        free(vgm);
        SDL_Quit();
        return 1;
    }
    
    SDL_ResumeAudioStreamDevice(audio_stream);
    
    SDL_Event event;
    bool running = true;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_SPACE:
                        state.paused = !state.paused;
                        break;
                        
                    case SDLK_L:
                        state.loop_enabled = !state.loop_enabled;
                        break;
                        
                    case SDLK_R:
                        vgm->pos = vgm->data_offset;
                        state.samples_played = 0;
                        state.wait_samples = 0;
                        break;
                        
                    case SDLK_Q:
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                }
            }
        }
        
        // Render UI
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // Set text color to white
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        
        // Show VGM info
        SDL_RenderDebugTextFormat(renderer, 10, 10, "VGM Player - Version %x.%02x", 
                                 (vgm->version >> 8) & 0xFF, vgm->version & 0xFF);
        
        if (vgm->ym2612_clock) {
            SDL_RenderDebugTextFormat(renderer, 10, 30, "YM2612: %u Hz", 
                                     vgm->ym2612_clock & 0x3FFFFFFF);
        }
        
        if (vgm->sn76489_clock) {
            SDL_RenderDebugTextFormat(renderer, 10, 50, "SN76489: %u Hz", 
                                     vgm->sn76489_clock & 0x3FFFFFFF);
        }
        
        // Show playback status
        uint32_t total_time = vgm->sample_count / (vgm->rate ? vgm->rate : 44100);
        uint32_t current_time = state.samples_played / SAMPLE_RATE;
        SDL_RenderDebugTextFormat(renderer, 10, 80, "Time: %02u:%02u / %02u:%02u", 
                                 current_time / 60, current_time % 60,
                                 total_time / 60, total_time % 60);
        
        SDL_RenderDebugTextFormat(renderer, 10, 100, "Status: %s", 
                                 state.paused ? "PAUSED" : "PLAYING");
        
        SDL_RenderDebugTextFormat(renderer, 10, 120, "Looping: %s", 
                                 state.loop_enabled ? "ON" : "OFF");
        
        // Show controls
        SDL_RenderDebugText(renderer, 10, 160, "Controls:");
        SDL_RenderDebugText(renderer, 10, 180, "  Space: Pause/Resume");
        SDL_RenderDebugText(renderer, 10, 200, "  L: Toggle looping");
        SDL_RenderDebugText(renderer, 10, 220, "  R: Restart");
        SDL_RenderDebugText(renderer, 10, 240, "  Q/Escape: Quit");
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
        
        // Check if playback finished
        if (!state.loop_enabled && vgm->pos >= vgm->size) {
            running = false;
        }
    }
    
    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    
    // Cleanup
    if (state.ym2612) RN_Destroy(state.ym2612);
    if (state.psg) SNG_delete(state.psg);
    free(state.audio_buffer);
    free(vgm->data);
    free(vgm);
    SDL_Quit();
    
    return 0;
}