/*
 * main.c  –  Autoball entry point.
 *
 * Loop:
 *   1. Process SDL events
 *   2. Poll human input
 *   3. Fixed-timestep physics + game update
 *   4. Render frame
 *   5. Cap to TARGET_FPS
 */

#include "autoball.h"
#include "hud.h"
#include <stdio.h>
#include <stdlib.h>

/* ── Forward declarations ────────────────────────────────────────────────── */
static bool init_all(SDL_Window **win, Renderer *r, AudioContext *audio,
                     InputContext *input);
static void shutdown_all(SDL_Window *win, Renderer *r, AudioContext *audio,
                         InputContext *input);

/* ── Entry point ─────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    SDL_Window    *window = NULL;
    Renderer       renderer = {0};
    AudioContext   audio    = {0};
    InputContext   input    = {0};
    MatchState     match    = {0};

    if (!init_all(&window, &renderer, &audio, &input)) {
        fprintf(stderr, "Autoball: initialisation failed.\n");
        return EXIT_FAILURE;
    }

    /*
     * Default: human controls car 0 (blue team, slot 0).
     * All other 5 cars are medium-skill bots.
     * Pass -1 to make all bots (spectator / demo mode).
     */
    match_init(&match, 0, BOT_SKILL_MEDIUM);

    /* ── Main loop ───────────────────────────────────────────────────────── */
    bool     running    = true;
    uint64_t prev_ticks = SDL_GetTicks();
    float    accumulator = 0.0f;

    while (running) {
        /* ── Event processing ─────────────────────────────────────────── */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input_handle_event(&input, &event);

            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                renderer_resize(&renderer,
                                event.window.data1,
                                event.window.data2);
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_ESCAPE)
                    running = false;
                if (event.key.scancode == SDL_SCANCODE_F3)
                    ; /* TODO: toggle HUD debug overlay */
                if (event.key.scancode == SDL_SCANCODE_R)
                    match_init(&match, 0, BOT_SKILL_MEDIUM); /* restart */
                break;

            default:
                break;
            }
        }

        /* ── Timing ───────────────────────────────────────────────────── */
        uint64_t now   = SDL_GetTicks();
        float    frame_dt = (float)(now - prev_ticks) / 1000.0f;
        prev_ticks = now;

        /* Clamp to avoid spiral of death on slow frames */
        if (frame_dt > 0.1f) frame_dt = 0.1f;
        accumulator += frame_dt;

        /* ── Fixed-timestep update ────────────────────────────────────── */
        while (accumulator >= FIXED_DT) {
            /* Inject human input into the human car */
            if (match.human_car_id >= 0) {
                match.cars[match.human_car_id].input = input_poll(&input);
            }

            match_update(&match, FIXED_DT);
            accumulator -= FIXED_DT;
        }

        /* ── Render ───────────────────────────────────────────────────── */
        renderer_draw_frame(&renderer, &match);

        /* ── Check match end ──────────────────────────────────────────── */
        if (match_is_over(&match)) {
            /* Non-blocking 3-second postgame pause before restart */
            static uint64_t postgame_start = 0;
            if (postgame_start == 0) postgame_start = SDL_GetTicks();
            if (SDL_GetTicks() - postgame_start >= 3000) {
                postgame_start = 0;
                match_init(&match, 0, BOT_SKILL_MEDIUM);
            }
        }
    }

    shutdown_all(window, &renderer, &audio, &input);
    return EXIT_SUCCESS;
}

/* ── Init / shutdown ─────────────────────────────────────────────────────── */

static bool init_all(SDL_Window **win, Renderer *r, AudioContext *audio,
                     InputContext *input)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    *win = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT,
                            SDL_WINDOW_RESIZABLE);
    if (!*win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    if (!renderer_init(r, *win)) {
        fprintf(stderr, "renderer_init failed.\n");
        return false;
    }

    if (!renderer_build_meshes(r)) {
        fprintf(stderr, "renderer_build_meshes failed.\n");
        return false;
    }

    audio_init(audio);   /* non-fatal if audio fails */
    input_init(input);

    printf("Autoball v%d.%d.%d started.\n",
           AUTOBALL_VERSION_MAJOR,
           AUTOBALL_VERSION_MINOR,
           AUTOBALL_VERSION_PATCH);

    return true;
}

static void shutdown_all(SDL_Window *win, Renderer *r, AudioContext *audio,
                         InputContext *input)
{
    input_shutdown(input);
    audio_shutdown(audio);
    renderer_shutdown(r);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
}


