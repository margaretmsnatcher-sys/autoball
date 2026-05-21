#ifndef PHYSICS_H
#define PHYSICS_H

/*
 * physics.h  –  Rigid-body physics for Autoball.
 *
 * Handles:
 *   - Ball sphere vs arena wall/floor/ceiling collisions
 *   - Car AABB vs arena collisions
 *   - Car vs ball sphere collision (impulse-based)
 *   - Car vs car collision (simplified)
 *   - Gravity, drag, friction
 *
 * Future: weapon projectile physics (Twisted Metal layer)
 */

#include "autoball_constants.h"
#include "math3d.h"
#include <stdbool.h>

/* ── Rigid body ──────────────────────────────────────────────────────────── */
typedef struct {
    Vec3  pos;          /* world position (center of mass)  */
    Vec3  vel;          /* linear velocity                  */
    Vec3  ang_vel;      /* angular velocity (rad/s)         */
    Quat  rot;          /* orientation quaternion           */
    float mass;         /* kg                               */
    float inv_mass;     /* 1/mass (0 = static)              */
    float restitution;  /* bounciness [0..1]                */
    float friction;     /* surface friction [0..1]          */
    float linear_drag;  /* air resistance coefficient       */
    float angular_drag;
} RigidBody;

/* ── Collision result ────────────────────────────────────────────────────── */
typedef struct {
    bool  hit;
    Vec3  normal;       /* collision normal (points away from surface) */
    float depth;        /* penetration depth                           */
    Vec3  contact;      /* contact point in world space                */
} CollisionResult;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Initialise a rigid body with sane defaults */
void rb_init(RigidBody *rb, Vec3 pos, float mass, float restitution);

/* Integrate one physics sub-step (dt = FIXED_DT / PHYSICS_SUBSTEPS) */
void rb_integrate(RigidBody *rb, Vec3 gravity, float dt);

/* Apply an impulse at a world-space point */
void rb_apply_impulse(RigidBody *rb, Vec3 impulse, Vec3 contact_world);

/* Apply a force (accumulated, cleared each step) */
void rb_apply_force(RigidBody *rb, Vec3 force);

/* ── Collision detection ─────────────────────────────────────────────────── */

/* Sphere vs axis-aligned plane */
CollisionResult collide_sphere_plane(Vec3 center, float radius,
                                     Vec3 plane_point, Vec3 plane_normal);

/* Sphere vs sphere */
CollisionResult collide_sphere_sphere(Vec3 c0, float r0,
                                      Vec3 c1, float r1);

/* AABB vs axis-aligned plane */
CollisionResult collide_aabb_plane(Vec3 center, Vec3 half_extents,
                                   Vec3 plane_point, Vec3 plane_normal);

/* Sphere vs AABB (ball vs car) */
CollisionResult collide_sphere_aabb(Vec3 sphere_center, float radius,
                                    Vec3 box_center, Vec3 half_extents);

/* ── Collision resolution ────────────────────────────────────────────────── */
void resolve_collision(RigidBody *a, RigidBody *b, CollisionResult cr);

/* Resolve collision where b is static (infinite mass) */
void resolve_collision_static(RigidBody *a, CollisionResult cr);

/* ── Arena collision helpers ─────────────────────────────────────────────── */
void collide_ball_arena(RigidBody *ball, float ball_radius);
void collide_car_arena(RigidBody *car, Vec3 car_half_extents);

#endif /* PHYSICS_H */

