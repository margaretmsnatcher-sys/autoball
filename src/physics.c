/*
 * physics.c  -  Rigid-body physics for Autoball.
 *
 * Conventions:
 *   Y = up
 *   Positive Z = toward orange goal
 *   Positive X = right (from blue's perspective)
 */

#include "physics.h"
#include "autoball.h"
#include <math.h>
#include <string.h>

/* ── Rigid body ──────────────────────────────────────────────────────────── */

void rb_init(RigidBody *rb, Vec3 pos, float mass, float restitution)
{
    memset(rb, 0, sizeof(*rb));
    rb->pos         = pos;
    rb->vel         = vec3_zero();
    rb->ang_vel     = vec3_zero();
    rb->rot         = quat_identity();
    rb->mass        = mass;
    rb->inv_mass    = (mass > 0.0f) ? 1.0f / mass : 0.0f;
    rb->restitution = restitution;
    rb->friction    = 0.3f;
    rb->linear_drag = 0.02f;
    rb->angular_drag= 0.1f;
}

void rb_integrate(RigidBody *rb, Vec3 gravity, float dt)
{
    if (rb->inv_mass == 0.0f) return;   /* static body */

    /* Linear integration (semi-implicit Euler) */
    rb->vel = vec3_add(rb->vel, vec3_scale(gravity, dt));
    rb->vel = vec3_scale(rb->vel, 1.0f - rb->linear_drag * dt);
    rb->pos = vec3_add(rb->pos, vec3_scale(rb->vel, dt));

    /* Angular integration */
    rb->ang_vel = vec3_scale(rb->ang_vel, 1.0f - rb->angular_drag * dt);

    float ang_speed = vec3_len(rb->ang_vel);
    if (ang_speed > 1e-6f) {
        Vec3 axis  = vec3_scale(rb->ang_vel, 1.0f / ang_speed);
        float angle = ang_speed * dt;
        Quat  dq    = quat_from_axis_angle(axis, angle);
        rb->rot     = quat_norm(quat_mul(dq, rb->rot));
    }
}

void rb_apply_impulse(RigidBody *rb, Vec3 impulse, Vec3 contact_world)
{
    if (rb->inv_mass == 0.0f) return;

    /* Linear component */
    rb->vel = vec3_add(rb->vel, vec3_scale(impulse, rb->inv_mass));

    /* Angular component: only apply to ball (small objects), not cars.
       Cars use a separate upright-correction in car_update.
       Threshold: inv_mass > 0.5 means mass < 2kg = ball. */
    if (rb->inv_mass > 0.5f) {
        Vec3 r      = vec3_sub(contact_world, rb->pos);
        Vec3 torque = vec3_cross(r, impulse);
        float inertia_inv = rb->inv_mass * 2.5f;
        rb->ang_vel = vec3_add(rb->ang_vel, vec3_scale(torque, inertia_inv));
    }
}

void rb_apply_force(RigidBody *rb, Vec3 force)
{
    if (rb->inv_mass == 0.0f) return;
    rb->vel = vec3_add(rb->vel, vec3_scale(force, rb->inv_mass));
}

/* ── Collision detection ─────────────────────────────────────────────────── */

CollisionResult collide_sphere_plane(Vec3 center, float radius,
                                     Vec3 plane_point, Vec3 plane_normal)
{
    CollisionResult cr = {0};
    float dist = vec3_dot(vec3_sub(center, plane_point), plane_normal);
    if (dist < radius) {
        cr.hit     = true;
        cr.normal  = plane_normal;
        cr.depth   = radius - dist;
        cr.contact = vec3_sub(center, vec3_scale(plane_normal, radius));
    }
    return cr;
}

CollisionResult collide_sphere_sphere(Vec3 c0, float r0, Vec3 c1, float r1)
{
    CollisionResult cr = {0};
    Vec3  delta = vec3_sub(c1, c0);
    float dist2 = vec3_len2(delta);
    float radsum = r0 + r1;

    if (dist2 < radsum * radsum) {
        float dist = sqrtf(dist2);
        cr.hit     = true;
        cr.normal  = dist > 1e-6f
                     ? vec3_scale(delta, 1.0f / dist)
                     : (Vec3){0, 1, 0};
        cr.depth   = radsum - dist;
        cr.contact = vec3_add(c0, vec3_scale(cr.normal, r0));
    }
    return cr;
}

CollisionResult collide_aabb_plane(Vec3 center, Vec3 half_extents,
                                   Vec3 plane_point, Vec3 plane_normal)
{
    CollisionResult cr = {0};
    /* Project half-extents onto plane normal */
    float r = fabsf(half_extents.x * plane_normal.x)
            + fabsf(half_extents.y * plane_normal.y)
            + fabsf(half_extents.z * plane_normal.z);
    float dist = vec3_dot(vec3_sub(center, plane_point), plane_normal);
    if (dist < r) {
        cr.hit     = true;
        cr.normal  = plane_normal;
        cr.depth   = r - dist;
        cr.contact = vec3_sub(center, vec3_scale(plane_normal, r));
    }
    return cr;
}

CollisionResult collide_sphere_aabb(Vec3 sphere_center, float radius,
                                    Vec3 box_center, Vec3 half_extents)
{
    CollisionResult cr = {0};

    /* Find closest point on AABB to sphere center */
    Vec3 local = vec3_sub(sphere_center, box_center);
    Vec3 closest = {
        CLAMP(local.x, -half_extents.x, half_extents.x),
        CLAMP(local.y, -half_extents.y, half_extents.y),
        CLAMP(local.z, -half_extents.z, half_extents.z)
    };

    Vec3  delta = vec3_sub(local, closest);
    float dist2 = vec3_len2(delta);

    if (dist2 < radius * radius) {
        float dist = sqrtf(dist2);
        cr.hit     = true;
        cr.depth   = radius - dist;
        cr.normal  = dist > 1e-6f
                     ? vec3_scale(delta, 1.0f / dist)
                     : (Vec3){0, 1, 0};
        cr.contact = vec3_add(box_center, closest);
    }
    return cr;
}

/* ── Collision resolution ────────────────────────────────────────────────── */

void resolve_collision(RigidBody *a, RigidBody *b, CollisionResult cr)
{
    if (!cr.hit) return;

    float total_inv = a->inv_mass + b->inv_mass;
    if (total_inv < 1e-6f) return;

    /* Positional correction - push objects apart */
    float correction = (cr.depth + 0.01f) / total_inv;
    a->pos = vec3_sub(a->pos, vec3_scale(cr.normal, correction * a->inv_mass));
    b->pos = vec3_add(b->pos, vec3_scale(cr.normal, correction * b->inv_mass));

    /* Velocity resolution */
    Vec3 rel_vel = vec3_sub(b->vel, a->vel);
    float vel_along_normal = vec3_dot(rel_vel, cr.normal);

    if (vel_along_normal > 0.0f) return;  /* already separating */

    float e = (a->restitution + b->restitution) * 0.5f;
    float j = -(1.0f + e) * vel_along_normal / total_inv;

    Vec3 impulse = vec3_scale(cr.normal, j);
    rb_apply_impulse(a, vec3_neg(impulse), cr.contact);
    rb_apply_impulse(b, impulse,           cr.contact);
}

void resolve_collision_static(RigidBody *a, CollisionResult cr)
{
    if (!cr.hit) return;

    /* Push out of surface */
    a->pos = vec3_add(a->pos, vec3_scale(cr.normal, cr.depth + 0.001f));

    /* Reflect velocity */
    float vel_along = vec3_dot(a->vel, cr.normal);
    if (vel_along < 0.0f) {
        Vec3 reflect = vec3_scale(cr.normal, -(1.0f + a->restitution) * vel_along);
        a->vel = vec3_add(a->vel, reflect);

        /* Friction on tangential component only (not the reflected normal component) */
        Vec3 tang = vec3_sub(a->vel, vec3_scale(cr.normal, vec3_dot(a->vel, cr.normal)));
        a->vel = vec3_sub(a->vel, vec3_scale(tang, a->friction * 0.1f));
    }
}

/* ── Arena collision helpers ─────────────────────────────────────────────── */

void collide_ball_arena(RigidBody *ball, float ball_radius)
{
    /* Floor */
    CollisionResult cr = collide_sphere_plane(ball->pos, ball_radius,
                                              (Vec3){0,0,0}, (Vec3){0,1,0});
    resolve_collision_static(ball, cr);

    /* Ceiling */
    cr = collide_sphere_plane(ball->pos, ball_radius,
                              (Vec3){0, ARENA_CEIL_H, 0}, (Vec3){0,-1,0});
    resolve_collision_static(ball, cr);

    /* Left wall  (X = -ARENA_HALF_WID) */
    cr = collide_sphere_plane(ball->pos, ball_radius,
                              (Vec3){-ARENA_HALF_WID, 0, 0}, (Vec3){1,0,0});
    resolve_collision_static(ball, cr);

    /* Right wall (X = +ARENA_HALF_WID) */
    cr = collide_sphere_plane(ball->pos, ball_radius,
                              (Vec3){ARENA_HALF_WID, 0, 0}, (Vec3){-1,0,0});
    resolve_collision_static(ball, cr);

    /* Blue back wall  (Z = -ARENA_HALF_LEN) */
    cr = collide_sphere_plane(ball->pos, ball_radius,
                              (Vec3){0, 0, -ARENA_HALF_LEN}, (Vec3){0,0,1});
    resolve_collision_static(ball, cr);

    /* Orange back wall (Z = +ARENA_HALF_LEN) */
    cr = collide_sphere_plane(ball->pos, ball_radius,
                              (Vec3){0, 0, ARENA_HALF_LEN}, (Vec3){0,0,-1});
    resolve_collision_static(ball, cr);
}

void collide_car_arena(RigidBody *car, Vec3 car_half_extents)
{
    /* Floor */
    CollisionResult cr = collide_aabb_plane(car->pos, car_half_extents,
                                            (Vec3){0,0,0}, (Vec3){0,1,0});
    resolve_collision_static(car, cr);

    /* Ceiling */
    cr = collide_aabb_plane(car->pos, car_half_extents,
                            (Vec3){0, ARENA_CEIL_H, 0}, (Vec3){0,-1,0});
    resolve_collision_static(car, cr);

    /* Left wall */
    cr = collide_aabb_plane(car->pos, car_half_extents,
                            (Vec3){-ARENA_HALF_WID, 0, 0}, (Vec3){1,0,0});
    resolve_collision_static(car, cr);

    /* Right wall */
    cr = collide_aabb_plane(car->pos, car_half_extents,
                            (Vec3){ARENA_HALF_WID, 0, 0}, (Vec3){-1,0,0});
    resolve_collision_static(car, cr);

    /* Blue back wall */
    cr = collide_aabb_plane(car->pos, car_half_extents,
                            (Vec3){0, 0, -ARENA_HALF_LEN}, (Vec3){0,0,1});
    resolve_collision_static(car, cr);

    /* Orange back wall */
    cr = collide_aabb_plane(car->pos, car_half_extents,
                            (Vec3){0, 0, ARENA_HALF_LEN}, (Vec3){0,0,-1});
    resolve_collision_static(car, cr);
}
