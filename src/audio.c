/*
 * audio.c  -  SDL3 audio stubs.
 *
 * Prototype: procedurally generated tones via SDL_PutAudioStreamData.
 * Phase 2: load WAV/OGG files.
 */

#include "audio.h"
#include "autoball.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SAMPLE_RATE  44100
#define CHANNELS     2

/* Generate a simple sine-wave burst into a buffer */
static void gen_sine(float *buf, int samples, float freq, float vol, int sr)
{
    for (int i = 0; i < samples; i++) {
        float t = (float)i / (float)sr;
        float v = sinf(2.0f * (float)M_PI * freq * t) * vol
                * (1.0f - (float)i / (float)samples);  /* fade out */
        buf[i * CHANNELS + 0] = v;
        buf[i * CHANNELS + 1] = v;
    }
}

bool audio_init(AudioContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->master_volume = 0.7f;
    ctx->enabled       = false;   /* disabled until streams are set up */

    SDL_AudioSpec spec = {
        .format   = SDL_AUDIO_F32,
        .channels = CHANNELS,
        .freq     = SAMPLE_RATE,
    };

    ctx->device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!ctx->device) {
        fprintf(stderr, "audio_init: SDL_OpenAudioDevice failed: %s\n",
                SDL_GetError());
        return false;
    }

    /* Create one stream per SFX slot */
    for (int i = 0; i < SFX_COUNT; i++) {
        ctx->streams[i] = SDL_CreateAudioStream(&spec, &spec);
        if (ctx->streams[i]) {
            SDL_BindAudioStream(ctx->device, ctx->streams[i]);
        }
    }

    ctx->enabled = true;
    return true;
}

void audio_shutdown(AudioContext *ctx)
{
    for (int i = 0; i < SFX_COUNT; i++) {
        if (ctx->streams[i]) {
            SDL_DestroyAudioStream(ctx->streams[i]);
            ctx->streams[i] = NULL;
        }
    }
    if (ctx->device) {
        SDL_CloseAudioDevice(ctx->device);
        ctx->device = 0;
    }
}

void audio_play(AudioContext *ctx, SFXId sfx, float volume, float pitch)
{
    if (!ctx->enabled) return;
    if (sfx < 0 || sfx >= SFX_COUNT) return;
    if (!ctx->streams[sfx]) return;

    /* Procedural tones per SFX type */
    static const float freqs[SFX_COUNT] = {
        220.0f,   /* BALL_HIT_SOFT */
        440.0f,   /* BALL_HIT_HARD */
        880.0f,   /* GOAL          */
        660.0f,   /* BOOST_PICKUP  */
        330.0f,   /* CAR_JUMP      */
        110.0f,   /* DEMO          */
    };

    float freq = freqs[sfx] * pitch;
    int   dur_samples = (int)(SAMPLE_RATE * 0.15f);  /* 150ms */
    float *buf = malloc(dur_samples * CHANNELS * sizeof(float));
    if (!buf) return;

    gen_sine(buf, dur_samples, freq, volume * ctx->master_volume, SAMPLE_RATE);
    SDL_PutAudioStreamData(ctx->streams[sfx],
                           buf,
                           dur_samples * CHANNELS * (int)sizeof(float));
    free(buf);
}

void audio_set_master_volume(AudioContext *ctx, float vol)
{
    ctx->master_volume = CLAMP(vol, 0.0f, 1.0f);
}
