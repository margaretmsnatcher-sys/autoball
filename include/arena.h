#ifndef ARENA_H
#define ARENA_H

/*
 * arena.h  –  The playing field geometry and goal detection.
 *
 * The arena is axis-aligned:
 *   X: -ARENA_HALF_WID .. +ARENA_HALF_WID  (left/right)
 *   Y:  0             .. ARENA_CEIL_H      (floor/ceiling)
 *   Z: -ARENA_HALF_LEN .. +ARENA_HALF_LEN  (blue goal / orange goal)
 *
 * Goals are centered on the Z walls, width = 7.3, height = 6.4 (world units).
 */

#include "autoball_constants.h"
#include "math3d.h"
#include <stdbool.h>

#define GOAL_HALF_WIDTH  3.65f
#define GOAL_HEIGHT      6.4f

typedef enum {
    GOAL_NONE   = -1,
    GOAL_BLUE   =  0,   /* ball entered orange's goal (Z > +ARENA_HALF_LEN) */
    GOAL_ORANGE =  1    /* ball entered blue's goal   (Z < -ARENA_HALF_LEN) */
} GoalResult;

/* Boost pad positions (standard 34 pads, simplified to 6 large + 28 small) */
#define BOOST_PAD_COUNT_LARGE  6
#define BOOST_PAD_COUNT_SMALL 28
#define BOOST_PAD_COUNT       (BOOST_PAD_COUNT_LARGE + BOOST_PAD_COUNT_SMALL)

typedef struct {
    Vec3  pos;
    float radius;
    float boost_amount;   /* 100 for large, 12 for small */
    float respawn_timer;  /* 0 = available               */
    bool  active;
} BoostPad;

typedef struct {
    BoostPad pads[BOOST_PAD_COUNT];
} Arena;

void  arena_init(Arena *arena);
void  arena_update(Arena *arena, float dt);

/* Check if ball center is inside a goal mouth */
GoalResult arena_check_goal(Vec3 ball_pos, float ball_radius);

/* Check if a car is overlapping a boost pad; returns pad index or -1 */
int   arena_check_boost(Arena *arena, Vec3 car_pos, float car_radius);

/* Collect a boost pad (starts respawn timer) */
void  arena_collect_boost(Arena *arena, int pad_index);

#endif /* ARENA_H */

