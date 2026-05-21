#ifndef BOT_H
#define BOT_H

/*
 * bot.h  –  Bot AI for Autoball.
 *
 * Each bot evaluates the game state each tick and produces a CarInput.
 * The AI uses a simple role-based system:
 *   - Attacker: chase ball, attempt shots on goal
 *   - Defender: guard own goal, intercept
 *   - Support:  collect boost, rotate, shadow play
 *
 * Skill level scales reaction time, prediction accuracy, and decision quality.
 *
 * Future: weapon usage AI for Twisted Metal layer.
 */

#include "autoball_constants.h"
#include "car.h"
#include "ball.h"
#include "math3d.h"
#include <stdbool.h>

/* ── Bot role ────────────────────────────────────────────────────────────── */
typedef enum {
    BOT_ROLE_ATTACKER = 0,
    BOT_ROLE_DEFENDER = 1,
    BOT_ROLE_SUPPORT  = 2
} BotRole;

/* ── Bot state ───────────────────────────────────────────────────────────── */
typedef struct {
    BotRole role;
    float   skill;          /* 0..1 (easy=0.3, medium=0.6, hard=0.9) */

    /* Prediction */
    Vec3    predicted_ball_pos;   /* where bot thinks ball will be */
    float   prediction_time;      /* how far ahead to predict      */

    /* Targeting */
    Vec3    target_pos;           /* current movement target       */
    bool    going_for_ball;
    bool    going_for_boost;
    int     target_boost_pad;     /* index into arena boost pads   */

    /* Timing */
    float   reaction_timer;       /* countdown before acting       */
    float   role_timer;           /* time in current role          */

    /* State machine */
    int     state;                /* internal FSM state            */
} BotState;

/* ── API ─────────────────────────────────────────────────────────────────── */

void bot_init(BotState *bot, float skill);

/*
 * Compute the CarInput for this bot given the full game state.
 * cars[0..MAX_PLAYERS-1], ball, arena boost pads.
 */
void bot_think(BotState *bot, Car *self, Car *all_cars, int car_count,
               Ball *ball, void *arena, float dt);

/* Assign roles to all bots on a team (called when roles need rebalancing) */
void bot_assign_roles(Car *team_cars, BotState *team_bots, int count,
                      Ball *ball, TeamID team);

/* Predict ball position t seconds in the future (simple ballistic) */
Vec3 bot_predict_ball(const Ball *ball, float t);

/* Steer car toward a world-space target, returns CarInput */
CarInput bot_steer_to(const Car *car, Vec3 target, bool use_boost,
                      float skill);

#endif /* BOT_H */

