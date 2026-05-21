/*
 * car.c  -  Car entity: controller, physics, boost, jump, air control.
 *
 * The car sits on 4 virtual wheel contact points.  When all 4 are touching
 * the ground the car is "on_ground" and normal driving forces apply.
 * In the air, pitch/roll/yaw torques are applied instead.
 */

#include "car.h"
#include "autoball.h"
#include "physics.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ── Spawn positions for 3v3 ─────────────────────────────────────────────── */
/*
 * Blue  team (negative Z half): cars 0,1,2
 * Orange team (positive Z half): cars 3,4,5
 */
static const float spawn_x[TEAM_SIZE]   = { -5.0f, 0.0f,  5.0f };
static const float spawn_z_blue         = -20.0f;
static const float spawn_z_orange       =  20.0f;

void car_get_spawn_pos(int id, TeamID team, Vec3 *out_pos, float *out_yaw)
{
    int slot = id % TEAM_SIZE;
    float z  = (team == TEAM_BLUE) ? spawn_z_blue : spawn_z_orange;
    float yaw = (team == TEAM_BLUE) ? 0.0f : (float)M_PI;  /* face center */

    *out_pos = (Vec3){ spawn_x[slot], CAR_HALF_HGT + 0.05f, z };
    *out_yaw = yaw;
}

/* ── car_init ────────────────────────────────────────────────────────────── */

void car_init(Car *car, int id, TeamID team, PlayerType type, Vec3 spawn_pos)
{
    memset(car, 0, sizeof(*car));

    car->id   = id;
    car->team = team;
    car->type = type;

    car->half_extents = (Vec3){ CAR_HALF_LEN, CAR_HALF_HGT, CAR_HALF_WID };

    rb_init(&car->body, spawn_pos, CAR_MASS, 0.3f);
    car->body.linear_drag  = 0.05f;
    car->body.angular_drag = 0.8f;
    car->body.friction     = 0.6f;

    car->boost_amount = 33.0f;   /* start with 1/3 boost */
    car->can_jump     = true;
    car->on_ground    = false;

    /* Team colors */
    if (team == TEAM_BLUE)
        car->color = (Vec3){ 0.2f, 0.4f, 1.0f };
    else
        car->color = (Vec3){ 1.0f, 0.5f, 0.1f };

    /* Default name */
    if (type == PLAYER_HUMAN)
        snprintf(car->name, sizeof(car->name), "Player");
    else
        snprintf(car->name, sizeof(car->name), "Bot%d", id);

    /* Wheel offsets (local space) */
    car->wheel_pos[0] = (Vec3){ -CAR_HALF_LEN * 0.8f, -CAR_HALF_HGT, -CAR_HALF_WID * 0.9f };
    car->wheel_pos[1] = (Vec3){  CAR_HALF_LEN * 0.8f, -CAR_HALF_HGT, -CAR_HALF_WID * 0.9f };
    car->wheel_pos[2] = (Vec3){ -CAR_HALF_LEN * 0.8f, -CAR_HALF_HGT,  CAR_HALF_WID * 0.9f };
    car->wheel_pos[3] = (Vec3){  CAR_HALF_LEN * 0.8f, -CAR_HALF_HGT,  CAR_HALF_WID * 0.9f };
}

void car_reset(Car *car, Vec3 spawn_pos, float spawn_yaw)
{
    car->body.pos     = spawn_pos;
    car->body.vel     = vec3_zero();
    car->body.ang_vel = vec3_zero();
    car->body.rot     = quat_from_axis_angle((Vec3){0,1,0}, spawn_yaw);

    car->on_ground     = false;
    car->can_jump      = true;
    car->jumped        = false;
    car->double_jumped = false;
    car->jump_timer    = 0.0f;
    car->boost_amount  = 33.0f;
    car->is_supersonic = false;

    memset(&car->input, 0, sizeof(car->input));
}

/* ── Direction helpers ───────────────────────────────────────────────────── */

Vec3 car_forward(const Car *car)
{
    return quat_rotate(car->body.rot, (Vec3){0, 0, 1});
}

Vec3 car_right(const Car *car)
{
    return quat_rotate(car->body.rot, (Vec3){1, 0, 0});
}

Vec3 car_up(const Car *car)
{
    return quat_rotate(car->body.rot, (Vec3){0, 1, 0});
}

AABB car_get_aabb(const Car *car)
{
    Vec3 he = car->half_extents;
    return (AABB){
        .min = vec3_sub(car->body.pos, he),
        .max = vec3_add(car->body.pos, he)
    };
}

/* ── Ground detection ────────────────────────────────────────────────────── */

static void update_ground_contact(Car *car)
{
    int contacts = 0;
    for (int i = 0; i < 4; i++) {
        /* Transform wheel to world space (simplified: ignore rotation) */
        Vec3 w = vec3_add(car->body.pos, quat_rotate(car->body.rot, car->wheel_pos[i]));
        car->wheel_contact[i] = (w.y <= 0.05f);
        if (car->wheel_contact[i]) contacts++;
    }
    bool was_on_ground = car->on_ground;
    car->on_ground = (contacts >= 2);

    /* Landing: restore jump */
    if (!was_on_ground && car->on_ground) {
        car->can_jump      = true;
        car->jumped        = false;
        car->double_jumped = false;
        car->jump_timer    = 0.0f;
    }
}

/* ── Jump ────────────────────────────────────────────────────────────────── */

void car_do_jump(Car *car)
{
    if (!car->on_ground || !car->can_jump) return;
    car->body.vel.y += CAR_JUMP_IMPULSE;
    car->jumped     = true;
    car->can_jump   = false;
    car->jump_timer = 0.0f;
}

void car_do_double_jump(Car *car)
{
    if (car->on_ground || car->double_jumped) return;
    if (car->jump_timer > 1.5f) return;   /* too late for double jump */

    /* Double jump: impulse in car-up direction */
    Vec3 up = car_up(car);
    car->body.vel = vec3_add(car->body.vel, vec3_scale(up, CAR_JUMP_IMPULSE));
    car->double_jumped = true;
}

/* ── Boost ───────────────────────────────────────────────────────────────── */

void car_add_boost(Car *car, float amount)
{
    car->boost_amount += amount;
    if (car->boost_amount > 100.0f) car->boost_amount = 100.0f;
}

bool car_consume_boost(Car *car, float amount)
{
    if (car->boost_amount < amount) return false;
    car->boost_amount -= amount;
    return true;
}

/* ── car_apply_input ─────────────────────────────────────────────────────── */

void car_apply_input(Car *car, float dt)
{
    CarInput *in = &car->input;
    Vec3 fwd   = car_forward(car);
    Vec3 right = car_right(car);

    if (car->on_ground) {
        /* ── Ground driving ─────────────────────────────────────────────── */

        /* Throttle: force along forward axis */
        float speed = vec3_dot(car->body.vel, fwd);
        float max_spd = in->boost ? CAR_BOOST_SPEED : CAR_MAX_SPEED;

        if (fabsf(in->throttle) > 0.05f) {
            float accel = in->boost ? CAR_BOOST_ACCEL : 15.0f;
            if (fabsf(speed) < max_spd) {
                Vec3 force = vec3_scale(fwd, in->throttle * accel);
                rb_apply_force(&car->body, force);
            }
        }

        /* Boost consumption */
        if (in->boost && in->throttle > 0.0f) {
            car_consume_boost(car, 33.3f * dt);   /* ~3s full boost */
        }

        /* Steering: yaw rotation */
        if (fabsf(in->steer) > 0.05f) {
            float steer_rate = CAR_TURN_SPEED * in->steer;
            /* Scale steering by speed (less turn at low speed) */
            float speed_factor = CLAMP(fabsf(speed) / 5.0f, 0.2f, 1.0f);
            if (in->handbrake) speed_factor = 1.5f;   /* powerslide */
            Quat yaw_delta = quat_from_axis_angle((Vec3){0,1,0},
                                                  -steer_rate * speed_factor * dt);
            car->body.rot = quat_norm(quat_mul(yaw_delta, car->body.rot));
        }

        /* Handbrake: kill lateral velocity */
        if (in->handbrake) {
            Vec3 lat = vec3_scale(right, vec3_dot(car->body.vel, right));
            car->body.vel = vec3_sub(car->body.vel, vec3_scale(lat, 0.85f));
        }

        /* Jump */
        if (in->jump && !in->jump_held) {
            car_do_jump(car);
        }

    } else {
        /* ── Air control ────────────────────────────────────────────────── */
        float air_torque = 4.0f;

        /* Pitch */
        if (fabsf(in->pitch) > 0.05f) {
            Vec3 pitch_axis = car_right(car);
            car->body.ang_vel = vec3_add(car->body.ang_vel,
                vec3_scale(pitch_axis, in->pitch * air_torque * dt));
        }

        /* Roll */
        if (fabsf(in->roll) > 0.05f) {
            Vec3 roll_axis = car_forward(car);
            car->body.ang_vel = vec3_add(car->body.ang_vel,
                vec3_scale(roll_axis, in->roll * air_torque * dt));
        }

        /* Yaw (steer in air) */
        if (fabsf(in->steer) > 0.05f) {
            car->body.ang_vel = vec3_add(car->body.ang_vel,
                vec3_scale((Vec3){0,1,0}, -in->steer * air_torque * 0.5f * dt));
        }

        /* Air boost */
        if (in->boost && in->throttle > 0.0f) {
            if (car_consume_boost(car, 33.3f * dt)) {
                Vec3 boost_force = vec3_scale(fwd, CAR_BOOST_ACCEL);
                rb_apply_force(&car->body, boost_force);
            }
        }

        /* Double jump */
        if (in->jump && !in->jump_held && car->jumped && !car->double_jumped) {
            car_do_double_jump(car);
        }
    }

    /* Track jump_held for edge detection */
    car->input.jump_held = in->jump;
}

/* ── car_update ──────────────────────────────────────────────────────────── */

void car_update(Car *car, float dt)
{
    Vec3 gravity = {0, -CAR_GRAVITY, 0};

    /* Apply input once per frame (not per sub-step - avoids 4x force multiplication) */
    car_apply_input(car, dt);

    float sub_dt = dt / (float)PHYSICS_SUBSTEPS;
    for (int i = 0; i < PHYSICS_SUBSTEPS; i++) {
        rb_integrate(&car->body, gravity, sub_dt);
        collide_car_arena(&car->body, car->half_extents);
        update_ground_contact(car);
    }

    /* Clamp to ground (prevent sinking) */
    if (car->body.pos.y < CAR_HALF_HGT) {
        car->body.pos.y = CAR_HALF_HGT;
        if (car->body.vel.y < 0.0f) car->body.vel.y = 0.0f;
    }

    /* Jump timer */
    if (car->jumped && !car->on_ground) {
        car->jump_timer += dt;
    }

    /* Supersonic check */
    float spd = vec3_len(car->body.vel);
    car->is_supersonic = (spd >= CAR_BOOST_SPEED * 0.95f);
}
