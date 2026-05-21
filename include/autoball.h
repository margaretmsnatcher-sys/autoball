#ifndef AUTOBALL_H
#define AUTOBALL_H

/*
 * autoball.h  -  Top-level include for game-logic modules.
 * Renderer and HUD are NOT included here to avoid SDL3 GPU type pollution
 * in pure-logic translation units.  Include renderer.h / hud.h directly
 * in files that need them (main.c, renderer.c, hud.c).
 */

#include "autoball_constants.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "math3d.h"
#include "physics.h"
#include "arena.h"
#include "car.h"
#include "ball.h"
#include "bot.h"
#include "match.h"
#include "input.h"
#include "audio.h"

#endif /* AUTOBALL_H */
