#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec4 in_color;

layout(set = 1, binding = 0) uniform WorldUniforms {
    mat4 mvp;
    mat4 model;
    vec4 color;
    vec4 light_dir;
} uni;

layout(location = 0) out vec3 frag_normal_world;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out vec4 frag_light_dir;

void main()
{
    gl_Position = uni.mvp * vec4(in_pos, 1.0);
    frag_normal_world = normalize(mat3(uni.model) * in_normal);
    frag_color        = in_color * uni.color;
    frag_light_dir    = uni.light_dir;
}
