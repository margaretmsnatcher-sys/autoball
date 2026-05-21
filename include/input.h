#ifndef INPUT_H
#define INPUT_H

/*
 * input.h  –  Keyboard / gamepad input mapping to CarInput.
 *
 * Keyboard defaults:
 *   W/S        = throttle forward/reverse
 *   A/D        = steer left/right
 *   Space      = jump
 *   Left Shift = boost
 *   X          = handbrake/powerslide
 *   Up/Down    = pitch (air)
 *   Left/Right = roll  (air)
 */

#include "car.h"
#include <SDL3/SDL.h>

typedef struct {
    /* Keyboard state snapshot */
    const bool *keys;

    /* Gamepad (SDL_Gamepad) */
    SDL_Gamepad *gamepad;
    bool         gamepad_connected;

    /* Deadzone for analog sticks */
    float deadzone;
} InputContext;

void     input_init(InputContext *ctx);
void     input_shutdown(InputContext *ctx);
void     input_handle_event(InputContext *ctx, SDL_Event *event);
CarInput input_poll(InputContext *ctx);

#endif /* INPUT_H */
