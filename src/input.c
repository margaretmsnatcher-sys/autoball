/*
 * input.c  -  Keyboard and gamepad input -> CarInput.
 */

#include "input.h"
#include "autoball.h"
#include <string.h>

void input_init(InputContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->deadzone = 0.15f;

    /* Check for already-connected gamepads */
    int count = 0;
    SDL_JoystickID *pads = SDL_GetGamepads(&count);
    if (pads && count > 0) {
        ctx->gamepad = SDL_OpenGamepad(pads[0]);
        ctx->gamepad_connected = (ctx->gamepad != NULL);
    }
    if (pads) SDL_free(pads);
}

void input_shutdown(InputContext *ctx)
{
    if (ctx->gamepad) {
        SDL_CloseGamepad(ctx->gamepad);
        ctx->gamepad = NULL;
    }
}

void input_handle_event(InputContext *ctx, SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!ctx->gamepad_connected) {
            ctx->gamepad = SDL_OpenGamepad(event->gdevice.which);
            ctx->gamepad_connected = (ctx->gamepad != NULL);
        }
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        if (ctx->gamepad) {
            SDL_CloseGamepad(ctx->gamepad);
            ctx->gamepad = NULL;
            ctx->gamepad_connected = false;
        }
        break;

    default:
        break;
    }
}

static float axis_value(SDL_Gamepad *gp, SDL_GamepadAxis axis, float deadzone)
{
    if (!gp) return 0.0f;
    float v = SDL_GetGamepadAxis(gp, axis) / 32767.0f;
    if (v >  deadzone) return (v - deadzone) / (1.0f - deadzone);
    if (v < -deadzone) return (v + deadzone) / (1.0f - deadzone);
    return 0.0f;
}

CarInput input_poll(InputContext *ctx)
{
    CarInput in = {0};

    /* ── Keyboard ────────────────────────────────────────────────────────── */
    const bool *keys = SDL_GetKeyboardState(NULL);

    float kb_throttle = 0.0f;
    if (keys[SDL_SCANCODE_W]) kb_throttle += 1.0f;
    if (keys[SDL_SCANCODE_S]) kb_throttle -= 1.0f;

    float kb_steer = 0.0f;
    if (keys[SDL_SCANCODE_D]) kb_steer += 1.0f;
    if (keys[SDL_SCANCODE_A]) kb_steer -= 1.0f;

    float kb_pitch = 0.0f;
    if (keys[SDL_SCANCODE_UP])   kb_pitch += 1.0f;
    if (keys[SDL_SCANCODE_DOWN]) kb_pitch -= 1.0f;

    float kb_roll = 0.0f;
    if (keys[SDL_SCANCODE_LEFT])  kb_roll -= 1.0f;
    if (keys[SDL_SCANCODE_RIGHT]) kb_roll += 1.0f;

    in.throttle  = kb_throttle;
    in.steer     = kb_steer;
    in.pitch     = kb_pitch;
    in.roll      = kb_roll;
    in.jump      = keys[SDL_SCANCODE_SPACE] != 0;
    in.boost     = keys[SDL_SCANCODE_LSHIFT] != 0;
    in.handbrake = keys[SDL_SCANCODE_X] != 0;

    /* ── Gamepad (overrides keyboard if connected) ───────────────────────── */
    if (ctx->gamepad_connected && ctx->gamepad) {
        float dz = ctx->deadzone;

        float gp_throttle = axis_value(ctx->gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, dz)
                          - axis_value(ctx->gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER,  dz);
        float gp_steer    = axis_value(ctx->gamepad, SDL_GAMEPAD_AXIS_LEFTX,  dz);
        float gp_pitch    = axis_value(ctx->gamepad, SDL_GAMEPAD_AXIS_RIGHTY, dz);
        float gp_roll     = axis_value(ctx->gamepad, SDL_GAMEPAD_AXIS_RIGHTX, dz);

        bool gp_jump  = SDL_GetGamepadButton(ctx->gamepad, SDL_GAMEPAD_BUTTON_SOUTH) != 0;
        bool gp_boost = SDL_GetGamepadButton(ctx->gamepad, SDL_GAMEPAD_BUTTON_EAST)  != 0;
        bool gp_brake = SDL_GetGamepadButton(ctx->gamepad, SDL_GAMEPAD_BUTTON_WEST)  != 0;

        /* Gamepad takes priority */
        in.throttle  = gp_throttle;
        in.steer     = gp_steer;
        in.pitch     = gp_pitch;
        in.roll      = gp_roll;
        in.jump      = gp_jump;
        in.boost     = gp_boost;
        in.handbrake = gp_brake;
    }

    return in;
}
