#ifndef BALL_H
#define BALL_H

/*
 * ball.h  –  The ball entity.
 */

#include "autoball_constants.h"
#include "math3d.h"
#include "physics.h"

typedef struct {
    RigidBody body;
    float     radius;
    bool      in_play;      /* false during kickoff countdown */
    int       last_touch_id; /* car id that last touched ball */
    float     last_touch_t;  /* time of last touch            */
} Ball;

void ball_init(Ball *ball);
void ball_reset(Ball *ball);   /* center of arena, zero velocity */
void ball_update(Ball *ball, float dt);

#endif /* BALL_H */

