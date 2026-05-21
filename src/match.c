/*
 * match.c  -  Match state machine.
 *
 * Phases:
 *   PREGAME   -> KICKOFF  (after brief countdown)
 *   KICKOFF   -> PLAY     (ball released)
 *   PLAY      -> GOAL     (goal scored)
 *   GOAL      -> KICKOFF  (reset + countdown)
 *   PLAY      -> OVERTIME (time runs out, tied)
 *   OVERTIME  -> POSTGAME (first goal wins)
 *   PLAY/OT   -> POSTGAME (time + score)
 */

#include "match.h"
#include "autoball.h"
#include "physics.h"
#include <string.h>
#include <stdio.h>

#define KICKOFF_COUNTDOWN  3.0f
#define GOAL_PAUSE         3.0f
#define PREGAME_PAUSE      2.0f

/* ── match_init ──────────────────────────────────────────────────────────── */

void match_init(MatchState *match, int human_car_id, float bot_skill)
{
    memset(match, 0, sizeof(*match));

    match->human_car_id  = human_car_id;
    match->time_remaining = MATCH_DURATION;
    match->phase          = MATCH_PHASE_PREGAME;
    match->phase_timer    = PREGAME_PAUSE;
    match->kickoff_team   = TEAM_BLUE;
    match->last_scorer    = -1;
    match->last_assister  = -1;

    /* Init arena */
    arena_init(&match->arena);

    /* Init ball */
    ball_init(&match->ball);

    /* Init cars */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        TeamID     team = (i < TEAM_SIZE) ? TEAM_BLUE : TEAM_ORANGE;
        PlayerType type = (i == human_car_id) ? PLAYER_HUMAN : PLAYER_BOT;

        Vec3  spawn_pos;
        float spawn_yaw;
        car_get_spawn_pos(i, team, &spawn_pos, &spawn_yaw);
        car_init(&match->cars[i], i, team, type, spawn_pos);
        car_reset(&match->cars[i], spawn_pos, spawn_yaw);

        if (type == PLAYER_BOT) {
            bot_init(&match->bots[i], bot_skill);
        }
    }

    /* Assign initial bot roles */
    bot_assign_roles(match->cars,
                     match->bots,
                     TEAM_SIZE,
                     &match->ball,
                     TEAM_BLUE);
    bot_assign_roles(match->cars + TEAM_SIZE,
                     match->bots + TEAM_SIZE,
                     TEAM_SIZE,
                     &match->ball,
                     TEAM_ORANGE);
}

/* ── match_do_kickoff ────────────────────────────────────────────────────── */

void match_do_kickoff(MatchState *match)
{
    ball_reset(&match->ball);

    /* Reset all cars to spawn positions */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        TeamID team = match->cars[i].team;
        Vec3   pos;
        float  yaw;
        car_get_spawn_pos(i, team, &pos, &yaw);
        car_reset(&match->cars[i], pos, yaw);
    }

    match->phase       = MATCH_PHASE_KICKOFF;
    match->phase_timer = KICKOFF_COUNTDOWN;
}

/* ── match_on_goal ───────────────────────────────────────────────────────── */

void match_on_goal(MatchState *match, GoalResult goal)
{
    if (goal == GOAL_NONE) return;

    int scoring_team = (int)goal;
    match->score[scoring_team]++;

    /* Credit last touch */
    match->last_scorer   = match->ball.last_touch_id;
    match->last_assister = -1;

    if (match->last_scorer >= 0) {
        match->cars[match->last_scorer].goals++;
    }

    printf("GOAL! %s scores! (%d - %d)\n",
           scoring_team == TEAM_BLUE ? "Blue" : "Orange",
           match->score[TEAM_BLUE],
           match->score[TEAM_ORANGE]);

    match->phase       = MATCH_PHASE_GOAL;
    match->phase_timer = GOAL_PAUSE;
    match->ball.in_play = false;

    /* Next kickoff goes to the team that was scored on */
    match->kickoff_team = (scoring_team == TEAM_BLUE) ? TEAM_ORANGE : TEAM_BLUE;
}

/* ── match_update ────────────────────────────────────────────────────────── */

void match_update(MatchState *match, float dt)
{
    match->phase_timer -= dt;

    switch (match->phase) {

    case MATCH_PHASE_PREGAME:
        if (match->phase_timer <= 0.0f) {
            match_do_kickoff(match);
        }
        break;

    case MATCH_PHASE_KICKOFF:
        if (match->phase_timer <= 0.0f) {
            match->phase = MATCH_PHASE_PLAY;
            match->ball.in_play = true;
        }
        /* Cars can move during kickoff countdown */
        /* Fall through to update cars */
        /* (no break) */

    case MATCH_PHASE_PLAY:
    case MATCH_PHASE_OVERTIME: {

        /* Tick match clock (only during play) */
        if (match->phase == MATCH_PHASE_PLAY) {
            match->time_remaining -= dt;
            if (match->time_remaining <= 0.0f) {
                match->time_remaining = 0.0f;
                if (match->score[TEAM_BLUE] != match->score[TEAM_ORANGE]) {
                    match->phase = MATCH_PHASE_POSTGAME;
                    return;
                } else if (OVERTIME_ENABLED) {
                    match->phase   = MATCH_PHASE_OVERTIME;
                    match->overtime = true;
                    printf("OVERTIME!\n");
                } else {
                    match->phase = MATCH_PHASE_POSTGAME;
                    return;
                }
            }
        }

        /* ── Bot AI ──────────────────────────────────────────────────── */
        /* Re-assign roles every 5 seconds */
        match->role_timer += dt;
        if (match->role_timer > 5.0f) {
            match->role_timer = 0.0f;
            bot_assign_roles(match->cars,
                             match->bots,
                             TEAM_SIZE,
                             &match->ball,
                             TEAM_BLUE);
            bot_assign_roles(match->cars + TEAM_SIZE,
                             match->bots + TEAM_SIZE,
                             TEAM_SIZE,
                             &match->ball,
                             TEAM_ORANGE);
        }

        for (int i = 0; i < MAX_PLAYERS; i++) {
            Car *car = &match->cars[i];
            if (car->type == PLAYER_BOT) {
                bot_think(&match->bots[i], car,
                          match->cars, MAX_PLAYERS,
                          &match->ball,
                          &match->arena,
                          dt);
            }
        }

        /* ── Update cars ─────────────────────────────────────────────── */
        for (int i = 0; i < MAX_PLAYERS; i++) {
            car_update(&match->cars[i], dt);
        }

        /* ── Car vs ball collision (only when ball is in play) ──────────── */
        if (match->ball.in_play) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                Car *car = &match->cars[i];
                CollisionResult cr = collide_sphere_aabb(
                    match->ball.body.pos, match->ball.radius,
                    car->body.pos, car->half_extents);

                if (cr.hit) {
                    resolve_collision(&match->ball.body, &car->body, cr);
                    match->ball.last_touch_id = car->id;
                    match->ball.last_touch_t  = match->time_remaining;
                    car->shots++;
                }
            }
        }

        /* ── Car vs car collision ─────────────────────────────────────── */
        for (int i = 0; i < MAX_PLAYERS; i++) {
            for (int j = i + 1; j < MAX_PLAYERS; j++) {
                Car *a = &match->cars[i];
                Car *b = &match->cars[j];
                CollisionResult cr = collide_sphere_aabb(
                    a->body.pos, CAR_HALF_LEN,
                    b->body.pos, b->half_extents);
                if (cr.hit) {
                    resolve_collision(&a->body, &b->body, cr);
                }
            }
        }

        /* ── Update ball ─────────────────────────────────────────────── */
        if (match->ball.in_play) {
            ball_update(&match->ball, dt);
        }

        /* ── Boost pad pickups ────────────────────────────────────────── */
        arena_update(&match->arena, dt);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            Car *car = &match->cars[i];
            int pad = arena_check_boost(&match->arena,
                                        car->body.pos,
                                        CAR_HALF_LEN * 0.5f);
            if (pad >= 0) {
                car_add_boost(car, match->arena.pads[pad].boost_amount);
                arena_collect_boost(&match->arena, pad);
            }
        }

        /* ── Goal check ──────────────────────────────────────────────── */
        if (match->ball.in_play) {
            GoalResult goal = arena_check_goal(match->ball.body.pos,
                                               match->ball.radius);
            if (goal != GOAL_NONE) {
                match_on_goal(match, goal);
                /* In overtime, first goal ends the match */
                if (match->overtime) {
                    match->phase = MATCH_PHASE_POSTGAME;
                }
            }
        }

        break;
    }

    case MATCH_PHASE_GOAL:
        if (match->phase_timer <= 0.0f) {
            match_do_kickoff(match);
        }
        break;

    case MATCH_PHASE_POSTGAME:
        /* Nothing to update; main loop handles restart */
        break;
    }
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

int match_is_over(const MatchState *match)
{
    return match->phase == MATCH_PHASE_POSTGAME;
}

Car *match_get_car(MatchState *match, int id)
{
    if (id < 0 || id >= MAX_PLAYERS) return NULL;
    return &match->cars[id];
}

