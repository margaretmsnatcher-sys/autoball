#ifndef RENDERER_H
#define RENDERER_H

#include "autoball_constants.h"
#include "math3d.h"
#include "match.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

/* ── Vertex layout ───────────────────────────────────────────────────────── */
typedef struct {
    float pos[3];
    float normal[3];
    float uv[2];
    float color[4];
} Vertex;

/* ── GPU mesh ────────────────────────────────────────────────────────────── */
typedef struct {
    SDL_GPUBuffer *vertex_buf;
    SDL_GPUBuffer *index_buf;
    uint32_t       vertex_count;
    uint32_t       index_count;
} GPUMesh;

/* ── Camera ──────────────────────────────────────────────────────────────── */
typedef struct {
    Vec3  pos;
    Vec3  target;
    Vec3  up;
    float fov_y;
    float near_z;
    float far_z;
    float yaw;
    float pitch;
    float distance;
} Camera;

/* ── Renderer context ────────────────────────────────────────────────────── */
typedef struct {
    SDL_Window              *window;
    SDL_GPUDevice           *gpu;
    SDL_GPUGraphicsPipeline *pipeline_world;
    SDL_GPUGraphicsPipeline *pipeline_hud;
    SDL_GPUTexture          *depth_texture;
    GPUMesh mesh_arena_floor;
    GPUMesh mesh_arena_walls;
    GPUMesh mesh_arena_goals;
    GPUMesh mesh_ball;
    GPUMesh mesh_car;
    GPUMesh mesh_boost_pad_large;
    GPUMesh mesh_boost_pad_small;
    SDL_GPUBuffer           *uniform_buf;
    Camera  camera;
    int     width;
    int     height;
} Renderer;

/* ── Uniform structs ─────────────────────────────────────────────────────── */
typedef struct {
    float mvp[16];
    float model[16];
    float color[4];
    float light_dir[4];
} WorldUniforms;

/* ── API ─────────────────────────────────────────────────────────────────── */
int  renderer_init(Renderer *r, SDL_Window *window);
void renderer_shutdown(Renderer *r);
void renderer_resize(Renderer *r, int w, int h);
int  renderer_build_meshes(Renderer *r);
void renderer_draw_frame(Renderer *r, const MatchState *match);

void camera_init(Camera *cam);
void camera_chase(Camera *cam, Vec3 car_pos, Vec3 car_fwd,
                  Vec3 ball_pos, float dt);
Mat4 camera_view(const Camera *cam);
Mat4 camera_proj(const Camera *cam, float aspect);

#endif /* RENDERER_H */
