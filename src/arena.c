/*
 * arena.c  -  Arena geometry, boost pads, goal detection.
 *
 * Boost pad layout mirrors standard Rocket League positions (scaled to our
 * arena dimensions).  6 large pads (100 boost, 10s respawn) and 28 small
 * pads (12 boost, 4s respawn).
 */

#include "arena.h"
#include "autoball.h"
#include <string.h>
#include <math.h>

/* ── Boost pad respawn times ─────────────────────────────────────────────── */
#define BOOST_RESPAWN_LARGE  10.0f
#define BOOST_RESPAWN_SMALL   4.0f
#define BOOST_COLLECT_RADIUS  1.8f   /* world units */

/* ── Hardcoded pad positions (world X, Z) ────────────────────────────────── */
/* Large pads: corners + mid-field sides */
static const float large_pad_xz[BOOST_PAD_COUNT_LARGE][2] = {
    { -28.0f, -38.0f },   /* blue  left  corner */
    {  28.0f, -38.0f },   /* blue  right corner */
    { -28.0f,  38.0f },   /* orange left  corner */
    {  28.0f,  38.0f },   /* orange right corner */
    { -28.0f,   0.0f },   /* mid   left  */
    {  28.0f,   0.0f },   /* mid   right */
};

/* Small pads: a representative 28-pad layout */
static const float small_pad_xz[BOOST_PAD_COUNT_SMALL][2] = {
    /* Blue half */
    {   0.0f, -42.0f }, {  -8.0f, -42.0f }, {   8.0f, -42.0f },
    { -16.0f, -36.0f }, {  16.0f, -36.0f },
    {   0.0f, -28.0f }, { -20.0f, -28.0f }, {  20.0f, -28.0f },
    {  -8.0f, -20.0f }, {   8.0f, -20.0f },
    { -24.0f, -14.0f }, {  24.0f, -14.0f },
    {   0.0f, -14.0f },
    /* Center */
    { -16.0f,   0.0f }, {  16.0f,   0.0f },
    {   0.0f,   0.0f },
    /* Orange half (mirror) */
    {   0.0f,  42.0f }, {  -8.0f,  42.0f }, {   8.0f,  42.0f },
    { -16.0f,  36.0f }, {  16.0f,  36.0f },
    {   0.0f,  28.0f }, { -20.0f,  28.0f }, {  20.0f,  28.0f },
    {  -8.0f,  20.0f }, {   8.0f,  20.0f },
    { -24.0f,  14.0f }, {  24.0f,  14.0f },
};

/* ── arena_init ──────────────────────────────────────────────────────────── */

void arena_init(Arena *arena)
{
    memset(arena, 0, sizeof(*arena));

    /* Large pads */
    for (int i = 0; i < BOOST_PAD_COUNT_LARGE; i++) {
        BoostPad *p   = &arena->pads[i];
        p->pos        = (Vec3){ large_pad_xz[i][0], 0.05f, large_pad_xz[i][1] };
        p->radius     = BOOST_COLLECT_RADIUS;
        p->boost_amount = 100.0f;
        p->respawn_timer = 0.0f;
        p->active     = true;
    }

    /* Small pads */
    for (int i = 0; i < BOOST_PAD_COUNT_SMALL; i++) {
        BoostPad *p   = &arena->pads[BOOST_PAD_COUNT_LARGE + i];
        p->pos        = (Vec3){ small_pad_xz[i][0], 0.05f, small_pad_xz[i][1] };
        p->radius     = BOOST_COLLECT_RADIUS * 0.7f;
        p->boost_amount = 12.0f;
        p->respawn_timer = 0.0f;
        p->active     = true;
    }
}

/* ── arena_update ────────────────────────────────────────────────────────── */

void arena_update(Arena *arena, float dt)
{
    for (int i = 0; i < BOOST_PAD_COUNT; i++) {
        BoostPad *p = &arena->pads[i];
        if (!p->active) {
            p->respawn_timer -= dt;
            if (p->respawn_timer <= 0.0f) {
                p->active        = true;
                p->respawn_timer = 0.0f;
            }
        }
    }
}

/* ── arena_check_goal ────────────────────────────────────────────────────── */

GoalResult arena_check_goal(Vec3 ball_pos, float ball_radius)
{
    float half_goal_w = GOAL_HALF_WIDTH;
    float goal_h      = GOAL_HEIGHT;

    /* Ball must be within goal mouth X and Y bounds */
    bool in_x = (fabsf(ball_pos.x) < half_goal_w + ball_radius);
    bool in_y = (ball_pos.y < goal_h + ball_radius && ball_pos.y > -ball_radius);

    if (!in_x || !in_y) return GOAL_NONE;

    /* Orange goal: ball crosses Z = +ARENA_HALF_LEN (blue scores) */
    if (ball_pos.z > ARENA_HALF_LEN) return GOAL_BLUE;

    /* Blue goal: ball crosses Z = -ARENA_HALF_LEN (orange scores) */
    if (ball_pos.z < -ARENA_HALF_LEN) return GOAL_ORANGE;

    return GOAL_NONE;
}

/* ── arena_check_boost ───────────────────────────────────────────────────── */

int arena_check_boost(Arena *arena, Vec3 car_pos, float car_radius)
{
    for (int i = 0; i < BOOST_PAD_COUNT; i++) {
        BoostPad *p = &arena->pads[i];
        if (!p->active) continue;

        float dx = car_pos.x - p->pos.x;
        float dz = car_pos.z - p->pos.z;
        float dist2 = dx*dx + dz*dz;
        float r = p->radius + car_radius;

        if (dist2 < r * r) return i;
    }
    return -1;
}

/* ── arena_collect_boost ─────────────────────────────────────────────────── */

void arena_collect_boost(Arena *arena, int pad_index)
{
    if (pad_index < 0 || pad_index >= BOOST_PAD_COUNT) return;
    BoostPad *p = &arena->pads[pad_index];
    p->active        = false;
    p->respawn_timer = (pad_index < BOOST_PAD_COUNT_LARGE)
                       ? BOOST_RESPAWN_LARGE
                       : BOOST_RESPAWN_SMALL;
}
