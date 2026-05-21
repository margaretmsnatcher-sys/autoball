/*
 * math3d.c  –  Matrix and quaternion implementations.
 */

#include "math3d.h"
#include <string.h>

/* ── Mat4 ─────────────────────────────────────────────────────────────────── */

Mat4 mat4_identity(void)
{
    Mat4 m = {0};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

Mat4 mat4_mul(Mat4 a, Mat4 b)
{
    Mat4 r = {0};
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

Mat4 mat4_translate(Vec3 t)
{
    Mat4 m = mat4_identity();
    m.m[12] = t.x;
    m.m[13] = t.y;
    m.m[14] = t.z;
    return m;
}

Mat4 mat4_scale(Vec3 s)
{
    Mat4 m = mat4_identity();
    m.m[0]  = s.x;
    m.m[5]  = s.y;
    m.m[10] = s.z;
    return m;
}

Mat4 mat4_rotate_x(float rad)
{
    Mat4 m = mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    m.m[5]  =  c;  m.m[9]  = -s;
    m.m[6]  =  s;  m.m[10] =  c;
    return m;
}

Mat4 mat4_rotate_y(float rad)
{
    Mat4 m = mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    m.m[0]  =  c;  m.m[8]  =  s;
    m.m[2]  = -s;  m.m[10] =  c;
    return m;
}

Mat4 mat4_rotate_z(float rad)
{
    Mat4 m = mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    m.m[0]  =  c;  m.m[4]  = -s;
    m.m[1]  =  s;  m.m[5]  =  c;
    return m;
}

Mat4 mat4_perspective(float fov_y_rad, float aspect, float near_z, float far_z)
{
    Mat4 m = {0};
    float f = 1.0f / tanf(fov_y_rad * 0.5f);
    float range_inv = 1.0f / (near_z - far_z);

    m.m[0]  = f / aspect;
    m.m[5]  = f;
    m.m[10] = (near_z + far_z) * range_inv;
    m.m[11] = -1.0f;
    m.m[14] = 2.0f * near_z * far_z * range_inv;
    return m;
}

Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up)
{
    Vec3 f = vec3_norm(vec3_sub(target, eye));
    Vec3 r = vec3_norm(vec3_cross(f, up));
    Vec3 u = vec3_cross(r, f);

    Mat4 m = mat4_identity();
    m.m[0]  =  r.x;  m.m[4]  =  r.y;  m.m[8]  =  r.z;
    m.m[1]  =  u.x;  m.m[5]  =  u.y;  m.m[9]  =  u.z;
    m.m[2]  = -f.x;  m.m[6]  = -f.y;  m.m[10] = -f.z;
    m.m[12] = -vec3_dot(r, eye);
    m.m[13] = -vec3_dot(u, eye);
    m.m[14] =  vec3_dot(f, eye);
    return m;
}

Vec3 mat4_transform_point(Mat4 m, Vec3 p)
{
    float x = m.m[0]*p.x + m.m[4]*p.y + m.m[8] *p.z + m.m[12];
    float y = m.m[1]*p.x + m.m[5]*p.y + m.m[9] *p.z + m.m[13];
    float z = m.m[2]*p.x + m.m[6]*p.y + m.m[10]*p.z + m.m[14];
    float w = m.m[3]*p.x + m.m[7]*p.y + m.m[11]*p.z + m.m[15];
    if (fabsf(w) > 1e-6f) { x /= w; y /= w; z /= w; }
    return (Vec3){x, y, z};
}

Vec3 mat4_transform_dir(Mat4 m, Vec3 d)
{
    return (Vec3){
        m.m[0]*d.x + m.m[4]*d.y + m.m[8] *d.z,
        m.m[1]*d.x + m.m[5]*d.y + m.m[9] *d.z,
        m.m[2]*d.x + m.m[6]*d.y + m.m[10]*d.z
    };
}

/* ── Quaternion ──────────────────────────────────────────────────────────── */

Quat quat_from_axis_angle(Vec3 axis, float rad)
{
    float half = rad * 0.5f;
    float s    = sinf(half);
    Vec3  a    = vec3_norm(axis);
    return (Quat){ a.x*s, a.y*s, a.z*s, cosf(half) };
}

Quat quat_mul(Quat a, Quat b)
{
    return (Quat){
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };
}

Quat quat_norm(Quat q)
{
    float l = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    if (l < 1e-6f) return quat_identity();
    return (Quat){ q.x/l, q.y/l, q.z/l, q.w/l };
}

Vec3 quat_rotate(Quat q, Vec3 v)
{
    /* Rodrigues' rotation formula via quaternion sandwich product */
    Vec3 qv = {q.x, q.y, q.z};
    Vec3 t  = vec3_scale(vec3_cross(qv, v), 2.0f);
    return vec3_add(vec3_add(v, vec3_scale(t, q.w)), vec3_cross(qv, t));
}

Mat4 quat_to_mat4(Quat q)
{
    float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
    float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
    float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;

    Mat4 m = mat4_identity();
    m.m[0]  = 1.0f - 2.0f*(yy+zz);
    m.m[1]  =        2.0f*(xy+wz);
    m.m[2]  =        2.0f*(xz-wy);
    m.m[4]  =        2.0f*(xy-wz);
    m.m[5]  = 1.0f - 2.0f*(xx+zz);
    m.m[6]  =        2.0f*(yz+wx);
    m.m[8]  =        2.0f*(xz+wy);
    m.m[9]  =        2.0f*(yz-wx);
    m.m[10] = 1.0f - 2.0f*(xx+yy);
    return m;
}

Quat quat_slerp(Quat a, Quat b, float t)
{
    float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;

    /* Ensure shortest path */
    if (dot < 0.0f) {
        b = (Quat){-b.x,-b.y,-b.z,-b.w};
        dot = -dot;
    }

    if (dot > 0.9995f) {
        /* Linear interpolation for nearly identical quaternions */
        Quat r = {
            a.x + t*(b.x-a.x),
            a.y + t*(b.y-a.y),
            a.z + t*(b.z-a.z),
            a.w + t*(b.w-a.w)
        };
        return quat_norm(r);
    }

    float theta_0 = acosf(dot);
    float theta   = theta_0 * t;
    float sin_t0  = sinf(theta_0);
    float sa = sinf(theta_0 - theta) / sin_t0;
    float sb = sinf(theta)           / sin_t0;

    return (Quat){
        sa*a.x + sb*b.x,
        sa*a.y + sb*b.y,
        sa*a.z + sb*b.z,
        sa*a.w + sb*b.w
    };
}
