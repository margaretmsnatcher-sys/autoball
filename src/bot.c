/*
 * bot.c  -  Bot AI for Autoball.
 *
 * Role-based FSM:
 *   ATTACKER  - chase ball, shoot toward opponent goal
 *   DEFENDER  - guard own goal, intercept
 *   SUPPORT   - collect boost, rotate, shadow play
 *
 * Skill [0..1] scales:
 *   - Reaction time (how often bot re-evaluates)
 *   - Prediction horizon (how far ahead it predicts ball)
 *   - Steering accuracy (adds noise at low skill)
 */

#include "bot.h"
#include "arena.h"
#include "autoball.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ── Internal FSM states ─────────────────────────────────────────────────── */
#define BOT_STATE_IDLE          0
#define BOT_STATE_CHASE_BALL    1
#define BOT_STATE_SHOOT         2
#define BOT_STATE_DEFEND        3
#define BOT_STATE_GET_BOOST     4
#define BOT_STATE_ROTATE        5

/* ── bot_init ────────────────────────────────────────────────────────────── */

void bot_init(BotState *bot, float skill)
{
    memset(bot, 0, sizeof(*bot));
    bot->skill             = CLAMP(skill, 0.0f, 1.0f);
    bot->role              = BOT_ROLE_SUPPORT;
    bot->target_boost_pad  = -1;
    bot->reaction_timer    = BOT_REACTION_TIME * (1.0f - bot->skill * 0.7f);
}

/* ── Ball prediction ─────────────────────────────────────────────────────── */

Vec3 bot_predict_ball(const Ball *ball, float t)
{
    /* Simple ballistic prediction (ignores wall bounces) */
    Vec3 pos = ball->body.pos;
    Vec3 vel = ball->body.vel;
    float gy = GRAVITY;

    pos.x += vel.x * t;
    pos.y += vel.y * t + 0.5f * gy * t * t;
    pos.z += vel.z * t;

    /* Clamp to arena floor */
    if (pos.y < BALL_RADIUS) pos.y = BALL_RADIUS;

    return pos;
}

/* ── Steering helper ─────────────────────────────────────────────────────── */

CarInput bot_steer_to(const Car *car, Vec3 target, bool use_boost, float skill)
{
    CarInput input = {0};

    Vec3 to_target = vec3_sub(target, car->body.pos);
    float dist     = vec3_len(to_target);

    if (dist < 0.5f) {
        /* Already at target */
        input.throttle = 0.0f;
        return input;
    }

    Vec3 dir    = vec3_scale(to_target, 1.0f / dist);
    Vec3 fwd    = quat_rotate(car->body.rot, (Vec3){0,0,1});

    /* Dot product tells us if target is ahead or behind */
    float dot   = vec3_dot(fwd, dir);

    /* Cross product Y component tells us which way to steer */
    Vec3  cross = vec3_cross(fwd, dir);
    float steer = CLAMP(cross.y * 3.0f, -1.0f, 1.0f);

    /* Add skill-based noise */
    if (skill < 0.9f) {
        float noise = (1.0f - skill) * 0.3f;
        steer += ((float)rand() / (float)RAND_MAX - 0.5f) * noise;
        steer  = CLAMP(steer, -1.0f, 1.0f);
    }

    input.steer    = steer;
    input.throttle = (dot > -0.3f) ? 1.0f : -0.5f;  /* reverse if behind */
    input.boost    = use_boost && (dist > 15.0f);

    return input;
}

/* ── Role assignment ─────────────────────────────────────────────────────── */

void bot_assign_roles(Car *team_cars, BotState *team_bots, int count,
                      Ball *ball, TeamID team)
{
    if (count < 1) return;

    /* Determine which side of the field the ball is on */
    float ball_z = ball->body.pos.z;
    bool ball_in_our_half = (team == TEAM_BLUE)
                            ? (ball_z < 0.0f)
                            : (ball_z > 0.0f);

    /* Simple assignment: closest to ball = attacker,
       closest to own goal = defender, rest = support */
    float min_dist_ball = 1e9f;
    float min_dist_goal = 1e9f;
    int   attacker_idx  = 0;
    int   defender_idx  = 0;

    Vec3 own_goal = (team == TEAM_BLUE)
                    ? (Vec3){0, 0, -ARENA_HALF_LEN}
                    : (Vec3){0, 0,  ARENA_HALF_LEN};

    for (int i = 0; i < count; i++) {
        if (team_cars[i].type != PLAYER_BOT) continue;

        float db = vec3_len(vec3_sub(team_cars[i].body.pos, ball->body.pos));
        float dg = vec3_len(vec3_sub(team_cars[i].body.pos, own_goal));

        if (db < min_dist_ball) { min_dist_ball = db; attacker_idx = i; }
        if (dg < min_dist_goal) { min_dist_goal = dg; defender_idx = i; }
    }

    for (int i = 0; i < count; i++) {
        if (team_cars[i].type != PLAYER_BOT) continue;

        /* Always assign an attacker regardless of ball position.
           When ball is in our half the attacker chases it directly.
           When ball is in opponent half the attacker still pushes forward. */
        if (i == attacker_idx) {
            team_bots[i].role = BOT_ROLE_ATTACKER;
        } else if (i == defender_idx && i != attacker_idx) {
            team_bots[i].role = BOT_ROLE_DEFENDER;
        } else {
            team_bots[i].role = BOT_ROLE_SUPPORT;
        }
    }
    (void)ball_in_our_half; /* used for future role weighting */
}

/* ── bot_think ───────────────────────────────────────────────────────────── */

void bot_think(BotState *bot, Car *self, Car *all_cars, int car_count,
               Ball *ball, void *arena_ptr, float dt)
{
    (void)all_cars; (void)car_count;
    Arena *arena = (Arena *)arena_ptr;

    /* Throttle re-evaluation by reaction time */
    bot->reaction_timer -= dt;
    if (bot->reaction_timer > 0.0f) {
        /* Carry out last decided input */
        self->input = bot_steer_to(self, bot->target_pos,
                                   self->input.boost, bot->skill);
        return;
    }
    bot->reaction_timer = BOT_REACTION_TIME * (1.5f - bot->skill);

    /* Predict ball position */
    float pred_t = 0.5f + bot->skill * 1.0f;
    bot->predicted_ball_pos = bot_predict_ball(ball, pred_t);

    Vec3 own_goal = (self->team == TEAM_BLUE)
                    ? (Vec3){0, 0, -ARENA_HALF_LEN}
                    : (Vec3){0, 0,  ARENA_HALF_LEN};

    Vec3 opp_goal = (self->team == TEAM_BLUE)
                    ? (Vec3){0, 0,  ARENA_HALF_LEN}
                    : (Vec3){0, 0, -ARENA_HALF_LEN};

    bool low_boost = (self->boost_amount < 20.0f);

    switch (bot->role) {

    case BOT_ROLE_ATTACKER: {
        if (low_boost && arena) {
            /* Find nearest active boost pad */
            int best = -1;
            float best_dist = 1e9f;
            for (int i = 0; i < BOOST_PAD_COUNT; i++) {
                if (!arena->pads[i].active) continue;
                float d = vec3_len(vec3_sub(self->body.pos, arena->pads[i].pos));
                if (d < best_dist) { best_dist = d; best = i; }
            }
            if (best >= 0) {
                bot->target_pos = arena->pads[best].pos;
                bot->state = BOT_STATE_GET_BOOST;
                break;
            }
        }
        /* Drive toward predicted ball position, aiming at opponent goal */
        Vec3 ball_to_goal = vec3_norm(vec3_sub(opp_goal, ball->body.pos));
        /* Approach ball from behind (relative to goal direction) */
        bot->target_pos = vec3_sub(bot->predicted_ball_pos,
                                   vec3_scale(ball_to_goal, 3.0f));
        bot->state = BOT_STATE_SHOOT;
        break;
    }

    case BOT_ROLE_DEFENDER: {
        /* Position between ball and own goal */
        Vec3 goal_to_ball = vec3_norm(vec3_sub(ball->body.pos, own_goal));
        bot->target_pos = vec3_add(own_goal, vec3_scale(goal_to_ball, 8.0f));
        bot->target_pos.y = 0.5f;
        bot->state = BOT_STATE_DEFEND;
        break;
    }

    case BOT_ROLE_SUPPORT:
    default: {
        if (low_boost && arena) {
            /* Collect nearest large boost pad */
            int best = -1;
            float best_dist = 1e9f;
            for (int i = 0; i < BOOST_PAD_COUNT_LARGE; i++) {
                if (!arena->pads[i].active) continue;
                float d = vec3_len(vec3_sub(self->body.pos, arena->pads[i].pos));
                if (d < best_dist) { best_dist = d; best = i; }
            }
            if (best >= 0) {
                bot->target_pos = arena->pads[best].pos;
                bot->state = BOT_STATE_GET_BOOST;
                break;
            }
        }
        /* Rotate to a good support position */
        Vec3 mid = vec3_scale(vec3_add(ball->body.pos, own_goal), 0.5f);
        mid.x += (self->id % 2 == 0) ? -8.0f : 8.0f;
        mid.y  = 0.5f;
        bot->target_pos = mid;
        bot->state = BOT_STATE_ROTATE;
        break;
    }
    }

    /* Produce input toward target */
    bool use_boost = (bot->role == BOT_ROLE_ATTACKER) && !low_boost;
    self->input = bot_steer_to(self, bot->target_pos, use_boost, bot->skill);

    /* Jump if ball is close and roughly in front */
    Vec3 to_ball = vec3_sub(ball->body.pos, self->body.pos);
    float dist_ball = vec3_len(to_ball);
    Vec3  fwd = quat_rotate(self->body.rot, (Vec3){0,0,1});
    float dot = vec3_dot(vec3_norm(to_ball), fwd);

    if (dist_ball < 4.0f && dot > 0.7f && self->on_ground) {
        self->input.jump = true;
    }
}
