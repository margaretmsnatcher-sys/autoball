/*
 * ball.c  -  Ball entity update.
 */

#include "ball.h"
#include "autoball.h"
#include "physics.h"
#include <string.h>

void ball_init(Ball *ball)
{
    memset(ball, 0, sizeof(*ball));
    ball->radius       = BALL_RADIUS;
    ball->last_touch_id = -1;
    rb_init(&ball->body,
            (Vec3){0, BALL_RADIUS + 0.1f, 0},
            BALL_MASS,
            BALL_RESTITUTION);
    ball->body.linear_drag  = 0.015f;
    ball->body.angular_drag = 0.05f;
    ball->body.friction     = 0.4f;
    ball->in_play = false;
}

void ball_reset(Ball *ball)
{
    ball->body.pos     = (Vec3){0, BALL_RADIUS + 0.1f, 0};
    ball->body.vel     = vec3_zero();
    ball->body.ang_vel = vec3_zero();
    ball->body.rot     = quat_identity();
    ball->last_touch_id = -1;
    ball->last_touch_t  = 0.0f;
    ball->in_play       = false;
}

void ball_update(Ball *ball, float dt)
{
    Vec3 gravity = {0, GRAVITY, 0};

    /* Sub-step physics for better accuracy */
    float sub_dt = dt / (float)PHYSICS_SUBSTEPS;
    for (int i = 0; i < PHYSICS_SUBSTEPS; i++) {
        rb_integrate(&ball->body, gravity, sub_dt);
        collide_ball_arena(&ball->body, ball->radius);
    }
}
