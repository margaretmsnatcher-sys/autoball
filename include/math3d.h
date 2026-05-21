#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define DEG2RAD(d) ((d) * (float)M_PI / 180.0f)
#define RAD2DEG(r) ((r) * 180.0f / (float)M_PI)
#define CLAMP(v,lo,hi) ((v)<(lo)?(lo):((v)>(hi)?(hi):(v)))
#define LERP(a,b,t)    ((a) + ((b)-(a))*(t))
#define SIGN(x)        ((x) > 0.0f ? 1.0f : ((x) < 0.0f ? -1.0f : 0.0f))

/* ── Vec2 ─────────────────────────────────────────────────────────────────── */
typedef struct { float x, y; } Vec2;

static inline Vec2 vec2(float x, float y)          { return (Vec2){x,y}; }
static inline Vec2 vec2_add(Vec2 a, Vec2 b)         { return (Vec2){a.x+b.x, a.y+b.y}; }
static inline Vec2 vec2_sub(Vec2 a, Vec2 b)         { return (Vec2){a.x-b.x, a.y-b.y}; }
static inline Vec2 vec2_scale(Vec2 v, float s)      { return (Vec2){v.x*s, v.y*s}; }
static inline float vec2_dot(Vec2 a, Vec2 b)        { return a.x*b.x + a.y*b.y; }
static inline float vec2_len(Vec2 v)                { return sqrtf(v.x*v.x + v.y*v.y); }
static inline Vec2 vec2_norm(Vec2 v) {
    float l = vec2_len(v);
    return l > 1e-6f ? (Vec2){v.x/l, v.y/l} : (Vec2){0,0};
}

/* ── Vec3 ─────────────────────────────────────────────────────────────────── */
typedef struct { float x, y, z; } Vec3;

static inline Vec3 vec3(float x, float y, float z)  { return (Vec3){x,y,z}; }
static inline Vec3 vec3_zero(void)                   { return (Vec3){0,0,0}; }
static inline Vec3 vec3_add(Vec3 a, Vec3 b)          { return (Vec3){a.x+b.x, a.y+b.y, a.z+b.z}; }
static inline Vec3 vec3_sub(Vec3 a, Vec3 b)          { return (Vec3){a.x-b.x, a.y-b.y, a.z-b.z}; }
static inline Vec3 vec3_scale(Vec3 v, float s)       { return (Vec3){v.x*s, v.y*s, v.z*s}; }
static inline Vec3 vec3_neg(Vec3 v)                  { return (Vec3){-v.x,-v.y,-v.z}; }
static inline float vec3_dot(Vec3 a, Vec3 b)         { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline float vec3_len(Vec3 v)                 { return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
static inline float vec3_len2(Vec3 v)                { return v.x*v.x + v.y*v.y + v.z*v.z; }
static inline Vec3 vec3_norm(Vec3 v) {
    float l = vec3_len(v);
    return l > 1e-6f ? (Vec3){v.x/l, v.y/l, v.z/l} : (Vec3){0,0,0};
}
static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y*b.z - a.z*b.y,
        a.z*b.x - a.x*b.z,
        a.x*b.y - a.y*b.x
    };
}
static inline Vec3 vec3_lerp(Vec3 a, Vec3 b, float t) {
    return (Vec3){LERP(a.x,b.x,t), LERP(a.y,b.y,t), LERP(a.z,b.z,t)};
}
static inline Vec3 vec3_reflect(Vec3 v, Vec3 n) {
    /* r = v - 2*(v·n)*n */
    float d = 2.0f * vec3_dot(v, n);
    return vec3_sub(v, vec3_scale(n, d));
}

/* ── Vec4 ─────────────────────────────────────────────────────────────────── */
typedef struct { float x, y, z, w; } Vec4;
static inline Vec4 vec4(float x, float y, float z, float w) { return (Vec4){x,y,z,w}; }

/* ── Mat4 (column-major, matches GLSL/HLSL convention) ───────────────────── */
typedef struct { float m[16]; } Mat4;

Mat4 mat4_identity(void);
Mat4 mat4_mul(Mat4 a, Mat4 b);
Mat4 mat4_translate(Vec3 t);
Mat4 mat4_scale(Vec3 s);
Mat4 mat4_rotate_x(float rad);
Mat4 mat4_rotate_y(float rad);
Mat4 mat4_rotate_z(float rad);
Mat4 mat4_perspective(float fov_y_rad, float aspect, float near, float far);
Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up);
Vec3 mat4_transform_point(Mat4 m, Vec3 p);
Vec3 mat4_transform_dir(Mat4 m, Vec3 d);

/* ── Quaternion ──────────────────────────────────────────────────────────── */
typedef struct { float x, y, z, w; } Quat;

static inline Quat quat_identity(void)              { return (Quat){0,0,0,1}; }
Quat quat_from_axis_angle(Vec3 axis, float rad);
Quat quat_mul(Quat a, Quat b);
Quat quat_norm(Quat q);
Vec3 quat_rotate(Quat q, Vec3 v);
Mat4 quat_to_mat4(Quat q);
Quat quat_slerp(Quat a, Quat b, float t);

/* ── AABB ─────────────────────────────────────────────────────────────────── */
typedef struct {
    Vec3 min;
    Vec3 max;
} AABB;

static inline bool aabb_overlaps(AABB a, AABB b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

#endif /* MATH3D_H */
