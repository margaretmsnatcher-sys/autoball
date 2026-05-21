#ifndef AUDIO_H
#define AUDIO_H

/*
 * audio.h  –  Minimal audio stub using SDL3 audio.
 *
 * Sounds:
 *   - Ball hit (soft / hard)
 *   - Goal scored
 *   - Boost pickup
 *   - Car jump
 *   - Crowd ambience (loop)
 *
 * All sounds are procedurally generated (sine/noise) for the prototype.
 * Replace with loaded WAV/OGG files later.
 */

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef enum {
    SFX_BALL_HIT_SOFT = 0,
    SFX_BALL_HIT_HARD,
    SFX_GOAL,
    SFX_BOOST_PICKUP,
    SFX_CAR_JUMP,
    SFX_DEMO,
    SFX_COUNT
} SFXId;

typedef struct {
    SDL_AudioDeviceID device;
    SDL_AudioStream  *streams[SFX_COUNT];
    float             master_volume;
    bool              enabled;
} AudioContext;

bool audio_init(AudioContext *ctx);
void audio_shutdown(AudioContext *ctx);
void audio_play(AudioContext *ctx, SFXId sfx, float volume, float pitch);
void audio_set_master_volume(AudioContext *ctx, float vol);

#endif /* AUDIO_H */
