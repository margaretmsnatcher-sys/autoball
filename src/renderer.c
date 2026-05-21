/*
 * renderer.c  -  SDL3 GPU renderer for Autoball.
 *
 * Rendering pipeline:
 *   1. Acquire command buffer
 *   2. Begin render pass (color + depth)
 *   3. Bind world pipeline
 *   4. Draw arena (floor, walls, goals)
 *   5. Draw boost pads
 *   6. Draw ball
 *   7. Draw cars
 *   8. End render pass
 *   9. Submit command buffer
 *
 * All geometry is procedurally generated and uploaded once at startup.
 * Shaders are loaded from shaders/compiled/ as pre-compiled SPIR-V.
 */

#include "renderer.h"
#include <SDL3_shadercross/SDL_shadercross.h>
#include "autoball.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── Shader loading ──────────────────────────────────────────────────────── */

static SDL_GPUShader *load_shader(SDL_GPUDevice *gpu,
                                  const char *base_path,
                                  SDL_GPUShaderStage stage,
                                  Uint32 num_uniform_buffers,
                                  Uint32 num_samplers)
{
    /* Load SPIR-V from disk */
    char spv_path[512];
    snprintf(spv_path, sizeof(spv_path), "%s.spv", base_path);

    size_t spv_size = 0;
    void  *spv_code = SDL_LoadFile(spv_path, &spv_size);
    if (!spv_code) {
        fprintf(stderr, "Failed to load SPIR-V: %s (%s)\n", spv_path, SDL_GetError());
        return NULL;
    }

    SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(gpu);
    SDL_GPUShader *shader = NULL;

    if (supported & SDL_GPU_SHADERFORMAT_SPIRV) {
        /* Vulkan: use SPIR-V directly */
        SDL_GPUShaderCreateInfo info = {
            .code                = spv_code,
            .code_size           = spv_size,
            .entrypoint          = "main",
            .format              = SDL_GPU_SHADERFORMAT_SPIRV,
            .stage               = stage,
            .num_uniform_buffers = num_uniform_buffers,
            .num_samplers        = num_samplers,
        };
        shader = SDL_CreateGPUShader(gpu, &info);
        if (!shader)
            fprintf(stderr, "SDL_CreateGPUShader (SPIRV) failed: %s\n", SDL_GetError());
    } else {
        /* D3D12: cross-compile SPIR-V -> DXIL at runtime via SDL_shadercross */
        SDL_ShaderCross_Init();

        SDL_ShaderCross_SPIRV_Info sc_info;
        SDL_zero(sc_info);
        sc_info.bytecode      = (const Uint8 *)spv_code;
        sc_info.bytecode_size = spv_size;
        sc_info.entrypoint    = "main";
        sc_info.shader_stage  = (stage == SDL_GPU_SHADERSTAGE_VERTEX)
                                ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX
                                : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
        sc_info.props         = 0;

        SDL_ShaderCross_GraphicsShaderResourceInfo res_info;
        SDL_zero(res_info);
        res_info.num_uniform_buffers = num_uniform_buffers;
        res_info.num_samplers        = num_samplers;

        shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(gpu, &sc_info, &res_info, 0);
        if (!shader)
            fprintf(stderr, "SDL_ShaderCross compile failed (%s): %s\n", spv_path, SDL_GetError());

        SDL_ShaderCross_Quit();
    }

    SDL_free(spv_code);
    return shader;
}

/* ── Mesh helpers ────────────────────────────────────────────────────────── */

static bool upload_mesh(SDL_GPUDevice *gpu, GPUMesh *mesh,
                        Vertex *verts, uint32_t vert_count,
                        uint16_t *indices, uint32_t idx_count)
{
    size_t vsize = vert_count * sizeof(Vertex);
    size_t isize = idx_count  * sizeof(uint16_t);

    /* Vertex buffer */
    SDL_GPUBufferCreateInfo vbci = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size  = (Uint32)vsize
    };
    mesh->vertex_buf = SDL_CreateGPUBuffer(gpu, &vbci);
    if (!mesh->vertex_buf) return 0;

    /* Index buffer */
    SDL_GPUBufferCreateInfo ibci = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size  = (Uint32)isize
    };
    mesh->index_buf = SDL_CreateGPUBuffer(gpu, &ibci);
    if (!mesh->index_buf) return 0;

    /* Upload via transfer buffer */
    SDL_GPUTransferBufferCreateInfo tbci = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = (Uint32)(vsize + isize)
    };
    SDL_GPUTransferBuffer *tb = SDL_CreateGPUTransferBuffer(gpu, &tbci);
    if (!tb) return 0;

    void *map = SDL_MapGPUTransferBuffer(gpu, tb, false);
    memcpy(map, verts, vsize);
    memcpy((char *)map + vsize, indices, isize);
    SDL_UnmapGPUTransferBuffer(gpu, tb);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation src_v = { .transfer_buffer = tb, .offset = 0 };
    SDL_GPUBufferRegion dst_v = { .buffer = mesh->vertex_buf, .offset = 0, .size = (Uint32)vsize };
    SDL_UploadToGPUBuffer(cp, &src_v, &dst_v, false);

    SDL_GPUTransferBufferLocation src_i = { .transfer_buffer = tb, .offset = (Uint32)vsize };
    SDL_GPUBufferRegion dst_i = { .buffer = mesh->index_buf, .offset = 0, .size = (Uint32)isize };
    SDL_UploadToGPUBuffer(cp, &src_i, &dst_i, false);

    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(gpu, tb);

    mesh->vertex_count = vert_count;
    mesh->index_count  = idx_count;
    return 1;
}

static void free_mesh(SDL_GPUDevice *gpu, GPUMesh *mesh)
{
    if (mesh->vertex_buf) SDL_ReleaseGPUBuffer(gpu, mesh->vertex_buf);
    if (mesh->index_buf)  SDL_ReleaseGPUBuffer(gpu, mesh->index_buf);
    memset(mesh, 0, sizeof(*mesh));
}


/* ── Procedural mesh generators ──────────────────────────────────────────── */

/* Flat quad on XZ plane */
static bool build_floor(SDL_GPUDevice *gpu, GPUMesh *mesh,
                         float hw, float hd, float y, float r, float g, float b)
{
    Vertex verts[4] = {
        {{ -hw, y, -hd }, {0,1,0}, {0,0}, {r,g,b,1}},
        {{  hw, y, -hd }, {0,1,0}, {1,0}, {r,g,b,1}},
        {{  hw, y,  hd }, {0,1,0}, {1,1}, {r,g,b,1}},
        {{ -hw, y,  hd }, {0,1,0}, {0,1}, {r,g,b,1}},
    };
    uint16_t idx[] = {0,1,2, 0,2,3};
    return upload_mesh(gpu, mesh, verts, 4, idx, 6);
}

/* Axis-aligned wall quad */
static bool build_wall_quad(SDL_GPUDevice *gpu, GPUMesh *mesh,
                              Vertex v0, Vertex v1, Vertex v2, Vertex v3)
{
    Vertex verts[4] = {v0, v1, v2, v3};
    uint16_t idx[] = {0,1,2, 0,2,3};
    return upload_mesh(gpu, mesh, verts, 4, idx, 6);
}

/* UV sphere for the ball */
static bool build_sphere(SDL_GPUDevice *gpu, GPUMesh *mesh,
                          float radius, int stacks, int slices,
                          float r, float g, float b)
{
    int vert_count = (stacks + 1) * (slices + 1);
    int idx_count  = stacks * slices * 6;

    Vertex   *verts   = malloc(vert_count * sizeof(Vertex));
    uint16_t *indices = malloc(idx_count  * sizeof(uint16_t));
    if (!verts || !indices) { free(verts); free(indices); return 0; }

    int vi = 0;
    for (int i = 0; i <= stacks; i++) {
        float phi = (float)M_PI * i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * (float)M_PI * j / slices;
            float x = sinf(phi) * cosf(theta);
            float y = cosf(phi);
            float z = sinf(phi) * sinf(theta);
            verts[vi].pos[0]    = x * radius;
            verts[vi].pos[1]    = y * radius;
            verts[vi].pos[2]    = z * radius;
            verts[vi].normal[0] = x;
            verts[vi].normal[1] = y;
            verts[vi].normal[2] = z;
            verts[vi].uv[0]     = (float)j / slices;
            verts[vi].uv[1]     = (float)i / stacks;
            verts[vi].color[0]  = r;
            verts[vi].color[1]  = g;
            verts[vi].color[2]  = b;
            verts[vi].color[3]  = 1.0f;
            vi++;
        }
    }

    int ii = 0;
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            uint16_t a = (uint16_t)(i * (slices + 1) + j);
            uint16_t b2 = (uint16_t)(a + slices + 1);
            indices[ii++] = a;     indices[ii++] = b2;    indices[ii++] = a + 1;
            indices[ii++] = a + 1; indices[ii++] = b2;    indices[ii++] = b2 + 1;
        }
    }

    bool ok = upload_mesh(gpu, mesh, verts, (uint32_t)vert_count,
                          indices, (uint32_t)idx_count);
    free(verts);
    free(indices);
    return ok;
}

/* Box mesh for cars */
static bool build_box(SDL_GPUDevice *gpu, GPUMesh *mesh,
                       float hx, float hy, float hz,
                       float r, float g, float b)
{
    /* 6 faces * 4 verts = 24 verts, 6 faces * 2 tris * 3 = 36 indices */
    float x = hx, y = hy, z = hz;
    float nx[6][3] = {{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}};

    Vertex verts[24];
    /* Front (-Z) */
    verts[0]  = (Vertex){{-x,-y,-z},{0,0,-1},{0,0},{r,g,b,1}};
    verts[1]  = (Vertex){{ x,-y,-z},{0,0,-1},{1,0},{r,g,b,1}};
    verts[2]  = (Vertex){{ x, y,-z},{0,0,-1},{1,1},{r,g,b,1}};
    verts[3]  = (Vertex){{-x, y,-z},{0,0,-1},{0,1},{r,g,b,1}};
    /* Back (+Z) */
    verts[4]  = (Vertex){{ x,-y, z},{0,0,1},{0,0},{r,g,b,1}};
    verts[5]  = (Vertex){{-x,-y, z},{0,0,1},{1,0},{r,g,b,1}};
    verts[6]  = (Vertex){{-x, y, z},{0,0,1},{1,1},{r,g,b,1}};
    verts[7]  = (Vertex){{ x, y, z},{0,0,1},{0,1},{r,g,b,1}};
    /* Left (-X) */
    verts[8]  = (Vertex){{-x,-y, z},{-1,0,0},{0,0},{r,g,b,1}};
    verts[9]  = (Vertex){{-x,-y,-z},{-1,0,0},{1,0},{r,g,b,1}};
    verts[10] = (Vertex){{-x, y,-z},{-1,0,0},{1,1},{r,g,b,1}};
    verts[11] = (Vertex){{-x, y, z},{-1,0,0},{0,1},{r,g,b,1}};
    /* Right (+X) */
    verts[12] = (Vertex){{ x,-y,-z},{1,0,0},{0,0},{r,g,b,1}};
    verts[13] = (Vertex){{ x,-y, z},{1,0,0},{1,0},{r,g,b,1}};
    verts[14] = (Vertex){{ x, y, z},{1,0,0},{1,1},{r,g,b,1}};
    verts[15] = (Vertex){{ x, y,-z},{1,0,0},{0,1},{r,g,b,1}};
    /* Bottom (-Y) */
    verts[16] = (Vertex){{-x,-y,-z},{0,-1,0},{0,0},{r,g,b,1}};
    verts[17] = (Vertex){{ x,-y,-z},{0,-1,0},{1,0},{r,g,b,1}};
    verts[18] = (Vertex){{ x,-y, z},{0,-1,0},{1,1},{r,g,b,1}};
    verts[19] = (Vertex){{-x,-y, z},{0,-1,0},{0,1},{r,g,b,1}};
    /* Top (+Y) */
    verts[20] = (Vertex){{-x, y, z},{0,1,0},{0,0},{r,g,b,1}};
    verts[21] = (Vertex){{ x, y, z},{0,1,0},{1,0},{r,g,b,1}};
    verts[22] = (Vertex){{ x, y,-z},{0,1,0},{1,1},{r,g,b,1}};
    verts[23] = (Vertex){{-x, y,-z},{0,1,0},{0,1},{r,g,b,1}};

    (void)nx; /* suppress unused warning */

    uint16_t idx[36];
    for (int f = 0; f < 6; f++) {
        uint16_t base = (uint16_t)(f * 4);
        idx[f*6+0] = base+0; idx[f*6+1] = base+1; idx[f*6+2] = base+2;
        idx[f*6+3] = base+0; idx[f*6+4] = base+2; idx[f*6+5] = base+3;
    }

    return upload_mesh(gpu, mesh, verts, 24, idx, 36);
}


/* ── renderer_init ───────────────────────────────────────────────────────── */

int renderer_init(Renderer *r, SDL_Window *window)
{
    memset(r, 0, sizeof(*r));
    r->window = window;

    SDL_GetWindowSize(window, &r->width, &r->height);

    /* Create GPU device - prefer Vulkan on Windows, fallback to D3D12 */
    r->gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_DXBC, true, NULL);
    if (!r->gpu) {
        fprintf(stderr, "SDL_CreateGPUDevice failed: %s\n", SDL_GetError());
        return 0;
    }

    if (!SDL_ClaimWindowForGPUDevice(r->gpu, window)) {
        fprintf(stderr, "SDL_ClaimWindowForGPUDevice failed: %s\n", SDL_GetError());
        return 0;
    }

    /* Load shaders */
    SDL_GPUShader *vert = load_shader(r->gpu,
        "shaders/compiled/world.vert",
        SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *frag = load_shader(r->gpu,
        "shaders/compiled/world.frag",
        SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);

    if (!vert || !frag) {
        fprintf(stderr, "Shader load failed. Make sure shaders are compiled.\n");
        if (vert) SDL_ReleaseGPUShader(r->gpu, vert);
        if (frag) SDL_ReleaseGPUShader(r->gpu, frag);
        return 0;
    }

    /* Vertex buffer layout */
    SDL_GPUVertexBufferDescription vbd = {
        .slot               = 0,
        .pitch              = sizeof(Vertex),
        .input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0,
    };

    SDL_GPUVertexAttribute attrs[] = {
        { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .offset = offsetof(Vertex, pos)    },
        { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
          .offset = offsetof(Vertex, normal) },
        { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(Vertex, uv)     },
        { .location = 3, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .offset = offsetof(Vertex, color)  },
    };

    SDL_GPUTextureFormat sc_fmt = SDL_GetGPUSwapchainTextureFormat(r->gpu, window);
    fprintf(stderr, "DEBUG: swapchain format = %d, shader formats = 0x%x\n",
            (int)sc_fmt, (int)SDL_GetGPUShaderFormats(r->gpu));

    SDL_GPUColorTargetDescription color_target_desc;
    SDL_zero(color_target_desc);
    color_target_desc.format = sc_fmt;

    SDL_GPUGraphicsPipelineCreateInfo pci;
    SDL_zero(pci);
    pci.vertex_shader   = vert;
    pci.fragment_shader = frag;
    pci.vertex_input_state.vertex_buffer_descriptions = &vbd;
    pci.vertex_input_state.num_vertex_buffers         = 1;
    pci.vertex_input_state.vertex_attributes          = attrs;
    pci.vertex_input_state.num_vertex_attributes      = 4;
    pci.primitive_type                                = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pci.rasterizer_state.fill_mode                    = SDL_GPU_FILLMODE_FILL;
    pci.rasterizer_state.cull_mode                    = SDL_GPU_CULLMODE_BACK;
    pci.rasterizer_state.front_face                   = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pci.depth_stencil_state.enable_depth_test         = true;
    pci.depth_stencil_state.enable_depth_write        = true;
    pci.depth_stencil_state.compare_op                = SDL_GPU_COMPAREOP_LESS;
    pci.target_info.num_color_targets                 = 1;
    pci.target_info.color_target_descriptions         = &color_target_desc;
    pci.target_info.has_depth_stencil_target          = true;
    pci.target_info.depth_stencil_format              = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    r->pipeline_world = SDL_CreateGPUGraphicsPipeline(r->gpu, &pci);
    SDL_ReleaseGPUShader(r->gpu, vert);
    SDL_ReleaseGPUShader(r->gpu, frag);

    if (!r->pipeline_world) {
        fprintf(stderr, "SDL_CreateGPUGraphicsPipeline failed: %s\n", SDL_GetError());
        return 0;
    }

    /* Create depth texture */
    renderer_resize(r, r->width, r->height);

    /* Init camera */
    camera_init(&r->camera);

    return 1;
}

void renderer_resize(Renderer *r, int w, int h)
{
    r->width  = w;
    r->height = h;

    if (r->depth_texture) {
        SDL_ReleaseGPUTexture(r->gpu, r->depth_texture);
        r->depth_texture = NULL;
    }

    SDL_GPUTextureCreateInfo tci = {
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width                = (Uint32)w,
        .height               = (Uint32)h,
        .layer_count_or_depth = 1,
        .num_levels           = 1,
    };
    r->depth_texture = SDL_CreateGPUTexture(r->gpu, &tci);
}

void renderer_shutdown(Renderer *r)
{
    if (!r->gpu) return;

    free_mesh(r->gpu, &r->mesh_arena_floor);
    free_mesh(r->gpu, &r->mesh_arena_walls);
    free_mesh(r->gpu, &r->mesh_arena_goals);
    free_mesh(r->gpu, &r->mesh_ball);
    free_mesh(r->gpu, &r->mesh_car);
    free_mesh(r->gpu, &r->mesh_boost_pad_large);
    free_mesh(r->gpu, &r->mesh_boost_pad_small);

    if (r->depth_texture)  SDL_ReleaseGPUTexture(r->gpu, r->depth_texture);
    if (r->pipeline_world) SDL_ReleaseGPUGraphicsPipeline(r->gpu, r->pipeline_world);

    SDL_ReleaseWindowFromGPUDevice(r->gpu, r->window);
    SDL_DestroyGPUDevice(r->gpu);
    memset(r, 0, sizeof(*r));
}

/* ── renderer_build_meshes ───────────────────────────────────────────────── */

int renderer_build_meshes(Renderer *r)
{
    SDL_GPUDevice *gpu = r->gpu;

    /* Arena floor */
    if (!build_floor(gpu, &r->mesh_arena_floor,
                     ARENA_HALF_WID, ARENA_HALF_LEN, 0.0f,
                     0.15f, 0.35f, 0.15f)) return 0;

    /* Ball */
    if (!build_sphere(gpu, &r->mesh_ball, BALL_RADIUS, 16, 16,
                      0.9f, 0.9f, 0.9f)) return 0;

    /* Car (generic box - team color applied via uniform) */
    if (!build_box(gpu, &r->mesh_car,
                   CAR_HALF_LEN, CAR_HALF_HGT, CAR_HALF_WID,
                   1.0f, 1.0f, 1.0f)) return 0;

    /* Boost pad large (flat disc approximated as thin box) */
    if (!build_box(gpu, &r->mesh_boost_pad_large,
                   1.2f, 0.05f, 1.2f,
                   1.0f, 0.8f, 0.0f)) return 0;

    /* Boost pad small */
    if (!build_box(gpu, &r->mesh_boost_pad_small,
                   0.6f, 0.05f, 0.6f,
                   0.0f, 0.8f, 1.0f)) return 0;

    /* Arena walls - build as 4 separate quads combined into one mesh */
    /* For simplicity, build each wall as a separate draw; reuse mesh_car */
    /* (walls are drawn with scale transforms) */

    return 1;
}


/* ── Camera ──────────────────────────────────────────────────────────────── */

void camera_init(Camera *cam)
{
    cam->pos      = (Vec3){0, 12.0f, -25.0f};
    cam->target   = (Vec3){0, 0, 0};
    cam->up       = (Vec3){0, 1, 0};
    cam->fov_y    = 70.0f;
    cam->near_z   = 0.5f;
    cam->far_z    = 300.0f;
    cam->yaw      = 0.0f;
    cam->pitch    = -0.35f;
    cam->distance = 25.0f;
}

void camera_chase(Camera *cam, Vec3 car_pos, Vec3 car_fwd,
                  Vec3 ball_pos, float dt)
{
    /* Blend between car-behind and ball-look */
    Vec3 behind = vec3_sub(car_pos, vec3_scale(car_fwd, cam->distance));
    behind.y = car_pos.y + 8.0f;

    /* Smooth follow */
    float alpha = 1.0f - expf(-8.0f * dt);
    cam->pos    = vec3_lerp(cam->pos, behind, alpha);

    /* Look at a point between car and ball */
    Vec3 look_at = vec3_lerp(car_pos, ball_pos, 0.4f);
    look_at.y   += 1.0f;
    cam->target  = vec3_lerp(cam->target, look_at, alpha);
}

Mat4 camera_view(const Camera *cam)
{
    return mat4_look_at(cam->pos, cam->target, cam->up);
}

Mat4 camera_proj(const Camera *cam, float aspect)
{
    return mat4_perspective(DEG2RAD(cam->fov_y), aspect,
                            cam->near_z, cam->far_z);
}

/* ── Draw helpers ────────────────────────────────────────────────────────── */

static void draw_mesh(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass,
                      GPUMesh *mesh, Mat4 model, Vec4 color,
                      Mat4 vp, Vec3 light_dir)
{
    if (!mesh->vertex_buf || mesh->index_count == 0) return;

    Mat4 mvp = mat4_mul(vp, model);

    WorldUniforms uni;
    memcpy(uni.mvp,       mvp.m,   sizeof(uni.mvp));
    memcpy(uni.model,     model.m, sizeof(uni.model));
    uni.color[0]     = color.x;
    uni.color[1]     = color.y;
    uni.color[2]     = color.z;
    uni.color[3]     = color.w;
    uni.light_dir[0] = light_dir.x;
    uni.light_dir[1] = light_dir.y;
    uni.light_dir[2] = light_dir.z;
    uni.light_dir[3] = 0.0f;

    SDL_PushGPUVertexUniformData(cmd, 0, &uni, sizeof(uni));

    SDL_GPUBufferBinding vb = { .buffer = mesh->vertex_buf, .offset = 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

    SDL_GPUBufferBinding ib = { .buffer = mesh->index_buf, .offset = 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    SDL_DrawGPUIndexedPrimitives(pass, mesh->index_count, 1, 0, 0, 0);
}

/* ── renderer_draw_frame ─────────────────────────────────────────────────── */

void renderer_draw_frame(Renderer *r, const MatchState *match)
{
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(r->gpu);
    if (!cmd) return;

    SDL_GPUTexture *swapchain = NULL;
    Uint32 sw, sh;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, r->window,
                                               &swapchain, &sw, &sh)) {
        SDL_CancelGPUCommandBuffer(cmd);
        return;
    }

    if (!swapchain) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }

    /* Update camera to chase human car (or car 0 if all bots) */
    int cam_car = (match->human_car_id >= 0) ? match->human_car_id : 0;
    Vec3 car_pos = match->cars[cam_car].body.pos;
    Vec3 car_fwd = quat_rotate(match->cars[cam_car].body.rot, (Vec3){0,0,1});

    /* Cast away const for camera update (camera is mutable state) */
    camera_chase((Camera *)&r->camera, car_pos, car_fwd,
                 match->ball.body.pos, FIXED_DT);

    float aspect = (sh > 0) ? (float)sw / (float)sh : 1.0f;
    Mat4 view = camera_view(&r->camera);
    Mat4 proj = camera_proj(&r->camera, aspect);
    Mat4 vp   = mat4_mul(proj, view);

    Vec3 light_dir = vec3_norm((Vec3){0.5f, -1.0f, 0.3f});

    /* ── Render pass ─────────────────────────────────────────────────────── */
    SDL_GPUColorTargetInfo color_target = {
        .texture     = swapchain,
        .load_op     = SDL_GPU_LOADOP_CLEAR,
        .store_op    = SDL_GPU_STOREOP_STORE,
        .clear_color = { 0.05f, 0.05f, 0.1f, 1.0f },
    };

    SDL_GPUDepthStencilTargetInfo depth_target = {
        .texture          = r->depth_texture,
        .load_op     = SDL_GPU_LOADOP_CLEAR,
        .store_op    = SDL_GPU_STOREOP_DONT_CARE,
        .clear_depth = 1.0f,
        .cycle       = false,
    };

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd,
                                                     &color_target, 1,
                                                     &depth_target);
    SDL_BindGPUGraphicsPipeline(pass, r->pipeline_world);

    /* ── Arena floor ─────────────────────────────────────────────────────── */
    draw_mesh(cmd, pass, &r->mesh_arena_floor,
              mat4_identity(),
              (Vec4){0.15f, 0.35f, 0.15f, 1.0f},
              vp, light_dir);

    /* ── Arena walls (drawn as scaled boxes) ─────────────────────────────── */
    float wh = ARENA_WALL_H * 0.5f;
    float wt = 0.5f;   /* wall thickness */

    /* Left wall */
    Mat4 wall_l = mat4_mul(
        mat4_translate((Vec3){-ARENA_HALF_WID - wt, wh, 0}),
        mat4_scale((Vec3){wt, wh, ARENA_HALF_LEN}));
    draw_mesh(cmd, pass, &r->mesh_car, wall_l,
              (Vec4){0.3f, 0.3f, 0.35f, 1.0f}, vp, light_dir);

    /* Right wall */
    Mat4 wall_r = mat4_mul(
        mat4_translate((Vec3){ARENA_HALF_WID + wt, wh, 0}),
        mat4_scale((Vec3){wt, wh, ARENA_HALF_LEN}));
    draw_mesh(cmd, pass, &r->mesh_car, wall_r,
              (Vec4){0.3f, 0.3f, 0.35f, 1.0f}, vp, light_dir);

    /* Blue back wall */
    Mat4 wall_b = mat4_mul(
        mat4_translate((Vec3){0, wh, -ARENA_HALF_LEN - wt}),
        mat4_scale((Vec3){ARENA_HALF_WID, wh, wt}));
    draw_mesh(cmd, pass, &r->mesh_car, wall_b,
              (Vec4){0.2f, 0.2f, 0.5f, 1.0f}, vp, light_dir);

    /* Orange back wall */
    Mat4 wall_o = mat4_mul(
        mat4_translate((Vec3){0, wh, ARENA_HALF_LEN + wt}),
        mat4_scale((Vec3){ARENA_HALF_WID, wh, wt}));
    draw_mesh(cmd, pass, &r->mesh_car, wall_o,
              (Vec4){0.5f, 0.3f, 0.1f, 1.0f}, vp, light_dir);

    /* ── Boost pads ──────────────────────────────────────────────────────── */
    for (int i = 0; i < BOOST_PAD_COUNT; i++) {
        const BoostPad *pad = &match->arena.pads[i];
        if (!pad->active) continue;

        Mat4 pad_m = mat4_translate(pad->pos);
        GPUMesh *pm = (i < BOOST_PAD_COUNT_LARGE)
                      ? &r->mesh_boost_pad_large
                      : &r->mesh_boost_pad_small;
        Vec4 pc = (i < BOOST_PAD_COUNT_LARGE)
                  ? (Vec4){1.0f, 0.8f, 0.0f, 1.0f}
                  : (Vec4){0.0f, 0.8f, 1.0f, 1.0f};
        draw_mesh(cmd, pass, pm, pad_m, pc, vp, light_dir);
    }

    /* ── Ball ────────────────────────────────────────────────────────────── */
    {
        Mat4 ball_rot = quat_to_mat4(match->ball.body.rot);
        Mat4 ball_t   = mat4_translate(match->ball.body.pos);
        Mat4 ball_m   = mat4_mul(ball_t, ball_rot);
        draw_mesh(cmd, pass, &r->mesh_ball, ball_m,
                  (Vec4){0.95f, 0.95f, 0.95f, 1.0f}, vp, light_dir);
    }

    /* ── Cars ────────────────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const Car *car = &match->cars[i];
        Mat4 car_rot = quat_to_mat4(car->body.rot);
        Mat4 car_t   = mat4_translate(car->body.pos);
        Mat4 car_m   = mat4_mul(car_t, car_rot);
        Vec4 car_col = (Vec4){ car->color.x, car->color.y, car->color.z, 1.0f };
        draw_mesh(cmd, pass, &r->mesh_car, car_m, car_col, vp, light_dir);
    }

    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
}








