#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "renuke.h"
#include "emu76489.h"

#define CLAMP(x, low, high) (((x) < (low)) ? (low) : (((x) > (high)) ? (high) : (x)))

#define SAMPLE_RATE RN_SAMPLE_RATE_NTSC
#define BUFFER_SIZE 512

typedef struct {
    uint8_t *data;
    size_t size;
    size_t pos;
    uint32_t version;
    uint32_t sample_count;
    uint32_t loop_offset;
    uint32_t loop_samples;
    uint32_t data_offset;
    uint32_t ym2612_clock;
    uint32_t sn76489_clock;
    uint32_t gd3_offset;
    uint32_t rate;
} VGMFile;

typedef struct {
    RN_Chip *ym2612;
    SNG *psg;
    
    VGMFile *vgm;
    uint32_t samples_played;
    uint32_t wait_samples;
    int paused;
    int loop_enabled;
    int16_t* audio_buffer;
    size_t audio_buffer_size;
} PlayerState;

static uint32_t read_u32_le(const uint8_t *data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *data) {
    return data[0] | (data[1] << 8);
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
    vgm->data = data;
    vgm->size = size;
    
    vgm->version = read_u32_le(data + 0x08);
    vgm->sample_count = read_u32_le(data + 0x18);
    vgm->loop_offset = read_u32_le(data + 0x1C);
    vgm->loop_samples = read_u32_le(data + 0x20);
    vgm->rate = read_u32_le(data + 0x24);
    vgm->sn76489_clock = read_u32_le(data + 0x0C);
    vgm->ym2612_clock = read_u32_le(data + 0x2C);
    vgm->gd3_offset = read_u32_le(data + 0x14);
    
    uint32_t vgm_data_offset = read_u32_le(data + 0x34);
    if (vgm->version >= 0x150 && vgm_data_offset) {
        vgm->data_offset = 0x34 + vgm_data_offset;
    } else {
        vgm->data_offset = 0x40;
    }
    
    vgm->pos = vgm->data_offset;
    
    printf("VGM Version: %x.%02x\n", (vgm->version >> 8) & 0xFF, vgm->version & 0xFF);
    printf("YM2612 Clock: %u Hz\n", vgm->ym2612_clock);
    printf("SN76489 Clock: %u Hz\n", vgm->sn76489_clock);
    printf("Sample Count: %u\n", vgm->sample_count);
    
    return vgm;
}

static void audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    PlayerState *state = (PlayerState*)userdata;
    
    if (state->paused || !state->vgm) {
        return;
    }

    if(state->audio_buffer_size < additional_amount)
    {
        state->audio_buffer = realloc(state->audio_buffer, additional_amount * 2);
    }

    int samples_needed = additional_amount / 4; // stereo 16-bit
    int samples_written = 0;
    int16_t *curr_audio_buffer = state->audio_buffer;
    
    while(samples_written < samples_needed)
    {
        // Process VGM commands if no wait pending
        while (state->wait_samples == 0 && state->vgm->pos < state->vgm->size) {
            uint8_t cmd = state->vgm->data[state->vgm->pos++];
            
            printf("cmd: %02x\n", cmd);

            switch (cmd) {
                case 0x50: // PSG write
                    if (state->vgm->pos < state->vgm->size) {
                        uint8_t val = state->vgm->data[state->vgm->pos++];
                        if (state->psg) {
                            SNG_writeIO(state->psg, val);
                        }
                    }
                    break;
                    
                case 0x52: // YM2612 port 0 write
                case 0x53: // YM2612 port 1 write
                    if (state->vgm->pos + 1 < state->vgm->size) {
                        uint8_t reg = state->vgm->data[state->vgm->pos++];
                        uint8_t val = state->vgm->data[state->vgm->pos++];
                        if (state->ym2612) {
                            uint32_t port = (cmd == 0x52) ? 0 : 2;
                            RN_Write(state->ym2612, port, reg);
                            RN_Clock(state->ym2612, 32);
                            RN_Write(state->ym2612, port + 1, val);
                            RN_Clock(state->ym2612, 32);
                        }
                    }
                    break;
                    
                case 0x61: // Wait n samples
                    if (state->vgm->pos + 1 < state->vgm->size) {
                        state->wait_samples = read_u16_le(&state->vgm->data[state->vgm->pos]);
                        state->vgm->pos += 2;
                    }
                    break;
                    
                case 0x62: // Wait 1/60 seconds
                    state->wait_samples = (uint32_t)(SAMPLE_RATE / 60.0f);
                    break;
                    
                case 0x63: // Wait 1/50 seconds
                    state->wait_samples = (uint32_t)(SAMPLE_RATE / 50.0f);
                    break;
                    
                case 0x66: // End of data
                    if (state->loop_enabled && state->vgm->loop_offset) {
                        state->vgm->pos = 0x1C + state->vgm->loop_offset;
                    } else {
                        state->vgm->pos = state->vgm->size;
                    }
                    break;
                    
                case 0x67: // Data block
                    if (state->vgm->pos + 6 < state->vgm->size) {
                        state->vgm->pos += 2; // Skip 0x66 and type
                        uint32_t block_size = read_u32_le(&state->vgm->data[state->vgm->pos]);
                        state->vgm->pos += 4 + block_size;
                    }
                    break;
                    
                case 0x70 ... 0x7F: // Wait 1-16 samples
                    state->wait_samples = (cmd & 0x0F) + 1;
                    break;
                    
                default:
                    if (cmd >= 0x80 && cmd <= 0x8F) {
                        // YM2612 port 0 address 2A write, then wait n samples
                        state->wait_samples = cmd & 0x0F;
                        // DAC data follows, skip it for now
                        if (state->vgm->pos < state->vgm->size) {
                            state->vgm->pos++;
                        }
                    }
                    break;
            }
        }
        
        int16_t ym_out[2] = {0, 0};
        int32_t psg_out[2] = {0, 0};

        int samples_queued = state->ym2612 ? RN_GetQueuedSamplesCount(state->ym2612) : 1;

        for(int i = 0; i < samples_queued; i++)
        {
            if (state->psg) {
                SNG_calc_stereo(state->psg, psg_out);
            }

            if(state->ym2612) {
                RN_DequeueSample(state->ym2612, ym_out);
            }

            *(curr_audio_buffer++) = CLAMP(psg_out[0] + ym_out[0], -32768, 32767);
            *(curr_audio_buffer++) = CLAMP(psg_out[1] + ym_out[1], -32768, 32767);
            
            if (state->wait_samples > 0) {
                state->wait_samples--;
            }

            samples_written++;
        }

        if (state->ym2612) {
            RN_Clock(state->ym2612, 6);
        }
    }

    state->samples_played += samples_written;

    SDL_PutAudioStreamData(stream, state->audio_buffer, samples_needed * 4);
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
    
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        free(vgm->data);
        free(vgm);
        return 1;
    }
    
    PlayerState state = {0};
    state.vgm = vgm;
    state.loop_enabled = 1;
    
    // Create chip emulators
    if (vgm->ym2612_clock) {
        state.ym2612 = RN_Create(RNCM_YM2612);
        if (state.ym2612) {
            RN_Reset(state.ym2612);
        }
    }
    
    if (vgm->sn76489_clock) {
        state.psg = SNG_new(vgm->sn76489_clock, SAMPLE_RATE);
        if (state.psg) {
            SNG_reset(state.psg);
        }
    }
    
    // Setup audio
    SDL_AudioSpec spec = {
        .format = SDL_AUDIO_S16,
        .channels = 2,
        .freq = SAMPLE_RATE
    };
    
    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, &state);
    if (!stream) {
        fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
        if (state.ym2612) RN_Destroy(state.ym2612);
        if (state.psg) SNG_delete(state.psg);
        free(vgm->data);
        free(vgm);
        SDL_Quit();
        return 1;
    }
    
    SDL_ResumeAudioStreamDevice(stream);
    
    printf("Playing VGM file. Press Enter to quit, Space to pause/resume.\n");
    
    SDL_Event event;
    int running = 1;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = 0;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_SPACE) {
                    state.paused = !state.paused;
                    printf(state.paused ? "Paused\n" : "Resumed\n");
                } else if (event.key.key == SDLK_RETURN) {
                    running = 0;
                }
            }
        }
        
        SDL_Delay(10);
        
        // Check if playback finished
        if (!state.loop_enabled && state.vgm->pos >= state.vgm->size) {
            running = 0;
        }
    }
    
    SDL_DestroyAudioStream(stream);
    if (state.ym2612) RN_Destroy(state.ym2612);
    if (state.psg) SNG_delete(state.psg);
    free(vgm->data);
    free(vgm);
    SDL_Quit();
    
    return 0;
}