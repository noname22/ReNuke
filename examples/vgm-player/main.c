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
    
    // Version 1.10+ fields
    uint8_t sn76489_feedback;
    uint8_t sn76489_shift_width;
    
    // Data blocks
    DataBlock data_blocks[MAX_DATA_BLOCKS];
    uint32_t data_block_count;
    
    // PCM data banks
    uint8_t *pcm_banks[MAX_PCM_BANKS];
    uint32_t pcm_bank_sizes[MAX_PCM_BANKS];
    uint32_t pcm_bank_pos[MAX_PCM_BANKS];
} VGMFile;


typedef struct {
    // Chip instances (single chip only)
    RN_Chip *ym2612;
    SNG *psg;
    
    // VGM file
    VGMFile *vgm;
    
    // Playback state
    uint32_t samples_played;
    uint32_t wait_clocks;  // Wait time in YM2612 clock cycles
    bool paused;
    bool loop_enabled;
    
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
    ASSERT_MSG(f, "Failed to open: %s", filename)

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

    ASSERT_MSG(memcmp(data, "Vgm ", 4) == 0, "Not a VGM file");

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
    
    vgm->pos = vgm->data_offset;

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

static inline uint32_t vgm_samples_to_clocks(VGMFile *vgm, uint32_t vgm_samples) {
    // VGM uses 44100 Hz sample rate
    // YM2612 runs at chip_clock Hz with 6 master clocks per internal clock
    // and generates one sample every 24 internal clocks (144 master clocks)
    return (uint32_t)((uint64_t)vgm_samples * vgm->ym2612_clock / (44100 * 6));
}

void ym_write(PlayerState *state, uint32_t port, uint8_t reg, uint8_t val)
{
    if (state->ym2612)
    {

        if (state->last_ym_port != port || state->last_ym_reg != reg)
        {
            RN_Write(state->ym2612, port, reg);
            RN_Clock(state->ym2612, 12);

            state->last_ym_reg = reg;
            state->last_ym_port = port;
        }

        RN_Write(state->ym2612, port + 1, val);

        if(port != 0 || reg != 0x2A)
        {
            RN_Clock(state->ym2612, 32);
        }
    }
}

static void process_vgm_command(VGMFile *vgm, PlayerState *state)
{
    if (vgm->pos >= vgm->size) return;
    
    uint32_t start_pos = vgm->pos;
    uint8_t cmd = vgm->data[vgm->pos++];
    
    switch (cmd) {
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
            
        case 0x52: // YM2612 port 0 write
        case 0x53: // YM2612 port 1 write
            if (vgm->pos + 1 < vgm->size) {
                uint8_t reg = vgm->data[vgm->pos++];
                uint8_t val = vgm->data[vgm->pos++];
                uint32_t port = (cmd == 0x52) ? 0 : 2;
                ym_write(state, port, reg, val);
            }
            break;
            
        case 0x61: // Wait n samples
            if (vgm->pos + 1 < vgm->size) {
                uint16_t vgm_samples = read_u16_le(&vgm->data[vgm->pos]);
                state->wait_clocks = vgm_samples_to_clocks(vgm, vgm_samples);
                vgm->pos += 2;
            }
            break;
            
        case 0x62: // Wait 735 samples (1/60 second)
            state->wait_clocks = vgm_samples_to_clocks(vgm, 735);
            break;
            
        case 0x63: // Wait 882 samples (1/50 second)
            state->wait_clocks = vgm_samples_to_clocks(vgm, 882);
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
            
        case 0x70 ... 0x7F: // Wait 1-16 samples
            state->wait_clocks = vgm_samples_to_clocks(vgm, (cmd & 0x0F) + 1);
            break;
            
        case 0x80 ... 0x8F: // YM2612 DAC write + wait
            state->wait_clocks = vgm_samples_to_clocks(vgm, cmd & 0x0F);
            // Read from data bank 0 at current position
            if (vgm->pcm_banks[0] && vgm->pcm_bank_pos[0] < vgm->pcm_bank_sizes[0]) {
                uint8_t data = vgm->pcm_banks[0][vgm->pcm_bank_pos[0]++];
                ym_write(state, 0, 0x2A, data);
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
            
        default:
            ASSERT_MSG(0, "Unknown VGM command: 0x%02X at offset 0x%X\n", cmd, (unsigned)start_pos);
            break;
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
    
    float* cur_buffer = state->audio_buffer;
    
    while (samples_needed > 0) {
        // Process VGM commands if no wait pending
        while (state->wait_clocks == 0 && state->vgm->pos < state->vgm->size) {
            process_vgm_command(state->vgm, state);
        }
        
        // Clock the chip if we have wait time
        if (state->wait_clocks > 0) {
            uint32_t clocks_to_run = state->wait_clocks > 512 ? 512 : state->wait_clocks;
            RN_Clock(state->ym2612, clocks_to_run);
            state->wait_clocks -= clocks_to_run;
        }
        
        // Generate samples while we have queued samples and need more output
        int16_t buffer[256 * 2];
        int samples_dequeued = RN_DequeueSamples(state->ym2612, buffer, samples_needed > 256 ? 256 : samples_needed);
        
        for(int i = 0; i < samples_dequeued; i++)
        {
            int32_t psg_out[2] = {0, 0};
            int16_t* ym_out = buffer + i * 2;
            
            SNG_calc_stereo(state->psg, psg_out);
            
            float ym_gain = 32.0f;
            float psg_gain = 4.0f;

            // Mix outputs
            *(cur_buffer++) = (ym_out[0] / 65535.0f) * ym_gain + (psg_out[0] / 65535.0f) * psg_gain;
            *(cur_buffer++) = (ym_out[1] / 65535.0f) * ym_gain + (psg_out[1] / 65535.0f) * psg_gain;
            
            state->samples_played++;
        }

        samples_needed -= samples_dequeued;
    }
    
    SDL_PutAudioStreamData(stream, state->audio_buffer, additional_amount);
}

int main(int argc, char *argv[]) {
    ASSERT_MSG(argc == 2, "Usage: %s <vgm_file>", argv[0])
    
    VGMFile *vgm = load_vgm(argv[1]);
    ASSERT_MSG(vgm->ym2612_clock < 0x40000000 || vgm->sn76489_clock < 0x40000000, "Dual chips is not supported");
    
    bool result = SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    ASSERT_MSG(result, "SDL init failed: %s", SDL_GetError());
    
    SDL_Window *window = SDL_CreateWindow("VGM Player", 400, 300, 0);
    ASSERT_MSG(window, "Failed to create window: %s", SDL_GetError());
    
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    ASSERT_MSG(renderer, "Failed to create renderer: %s", SDL_GetError());
    
    PlayerState state = {0};
    state.last_ym_reg = -1;
    state.last_ym_port = -1;
    state.vgm = vgm;
    state.loop_enabled = true;
    
    // Create chip emulators
    state.ym2612 = RN_Create(RNCM_YM2612);
    state.psg = SNG_new(vgm->sn76489_clock, SAMPLE_RATE);
    SNG_reset(state.psg);
    
    // Setup audio
    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_F32LE,
        .channels = 2,
        .freq = SAMPLE_RATE,
    };
    
    SDL_AudioStream *audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, &state);
    ASSERT_MSG(audio_stream, "Failed to open audio: %s", SDL_GetError())
    
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
                        state.wait_clocks = 0;
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
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

        int col = 0;
        #define DRAW_TEXT(...) SDL_RenderDebugTextFormat(renderer, 10, col += 10, __VA_ARGS__);

        // Show VGM info
        DRAW_TEXT("== VGM Player ==");
        col += 10;
        
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        uint32_t loop_duration = vgm->loop_samples / 44100;
        uint32_t loop_offset = vgm->loop_offset / 44100;

        DRAW_TEXT("VGM version: %x.%02x\n", (vgm->version >> 8) & 0xFF, vgm->version & 0xFF);
        DRAW_TEXT("YM2612: %u Hz", vgm->ym2612_clock);
        DRAW_TEXT("SN76489: %u Hz", vgm->sn76489_clock);
        col += 10;
        
        DRAW_TEXT("Loop offset: %02u:%02u", loop_offset / 60, loop_offset % 60);
        DRAW_TEXT("Loop duration: %02u:%02u", loop_duration / 60, loop_duration % 60);
        col += 10;
        
        // Show playback status
        uint32_t total_time = vgm->sample_count / 44100;
        uint32_t current_time = state.samples_played / SAMPLE_RATE;
        DRAW_TEXT("Time: %02u:%02u / %02u:%02u", current_time / 60, current_time % 60, total_time / 60, total_time % 60);
        DRAW_TEXT("Status: %s", state.paused ? "PAUSED" : "PLAYING");
        DRAW_TEXT("Looping: %s", state.loop_enabled ? "ON" : "OFF");
        col += 10;
        
        // Show controls
        DRAW_TEXT("Controls:");
        DRAW_TEXT("  Space: Pause/Resume");
        DRAW_TEXT("  L: Toggle looping");
        DRAW_TEXT("  R: Restart");
        DRAW_TEXT("  Q/Escape: Quit");
        
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