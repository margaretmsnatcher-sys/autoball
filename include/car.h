#ifndef CAR_H
#define CAR_H

/*
 * car.h  –  Car entity: state, input, movement, boost.
 *
 * Each car is driven either by a human (via InputState) or a bot (via BotState).
 * The car controller converts steering intent into physics forces/impulses.
 *
 * Future: weapon hardpoints, armor, health (Twisted Metal layer).
 */

#include "autoball_constants.h"
#include "math3d.h"
#include "physics.h"
#include <stdbool.h>
#include <stdint.h>

/* ── Team / player identity ──────────────────────────────────────────────── */
typedef enum {
    TEAM_BLUE = 0,
    TEAM_ORANGE = 1
} TeamID;

typedef enum {
    PLAYER_HUMAN = 0,
    PLAYER_BOT   = 1
} PlayerType;

/* ── Car input (produced by human input or bot AI each frame) ────────────── */
typedef struct {
    float throttle;     /* -1..1  (forward/reverse)  */
    float steer;        /* -1..1  (left/right)        */
    float pitch;        /* -1..1  (nose up/down, air) */
    float roll;         /* -1..1  (barrel roll, air)  */
    bool  jump;         /* jump button pressed        */
    bool  boost;        /* boost button held          */
    bool  handbrake;    /* powerslide                 */
    bool  jump_held;    /* for double-jump detection  */
} CarInput;

/* ── Car state ───────────────────────────────────────────────────────────── */
typedef struct {
    /* Physics */
    RigidBody  body;
    Vec3       half_extents;    /* CAR_HALF_LEN, CAR_HALF_HGT, CAR_HALF_WID */

    /* Driving state */
    bool       on_ground;
    bool       can_jump;        /* reset on landing */
    bool       jumped;          /* first jump used  */
    bool       double_jumped;   /* second jump used */
    float      jump_timer;      /* time since jump  */
    float      boost_amount;    /* 0..100           */
    bool       is_supersonic;   /* speed >= CAR_BOOST_SPEED */

    /* Wheel contact points (4 wheels, simplified) */
    Vec3       wheel_pos[4];    /* local space       */
    bool       wheel_contact[4];

    /* Identity */
    int        id;              /* 0..MAX_PLAYERS-1  */
    TeamID     team;
    PlayerType type;
    char       name[32];

    /* Accumulated input for this frame */
    CarInput   input;

    /* Visual */
    Vec3       color;           /* team color tint   */

    /* Score / stats */
    int        goals;
    int        assists;
    int        saves;
    int        shots;
    int        demos;           /* demolitions       */

    /* Future: Twisted Metal combat */
    /* WeaponSlot weapons[WEAPON_SLOTS]; */
    /* float health; */
    /* float armor;  */
} Car;

/* ── API ─────────────────────────────────────────────────────────────────── */

void car_init(Car *car, int id, TeamID team, PlayerType type, Vec3 spawn_pos);
void car_reset(Car *car, Vec3 spawn_pos, float spawn_yaw);
void car_update(Car *car, float dt);
void car_apply_input(Car *car, float dt);

/* Boost management */
void car_add_boost(Car *car, float amount);
bool car_consume_boost(Car *car, float amount);

/* Jump logic */
void car_do_jump(Car *car);
void car_do_double_jump(Car *car);

/* Compute the car's forward direction in world space */
Vec3 car_forward(const Car *car);
Vec3 car_right(const Car *car);
Vec3 car_up(const Car *car);

/* Get the world-space AABB for collision */
AABB car_get_aabb(const Car *car);

/* Spawn positions for 3v3 */
void car_get_spawn_pos(int id, TeamID team, Vec3 *out_pos, float *out_yaw);

#endif /* CAR_H */

