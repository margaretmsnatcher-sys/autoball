#version 450

layout(location = 0) in vec3 frag_normal_world;
layout(location = 1) in vec4 frag_color;
layout(location = 2) in vec4 frag_light_dir;

layout(location = 0) out vec4 out_color;

void main()
{
    vec3 N = normalize(frag_normal_world);
    vec3 L = normalize(-frag_light_dir.xyz);

    float ambient  = 0.25;
    float diffuse  = max(dot(N, L), 0.0);

    vec3 V = normalize(vec3(0.0, 1.0, 0.5));
    vec3 H = normalize(L + V);
    float specular = pow(max(dot(N, H), 0.0), 32.0) * 0.3;

    float light = ambient + diffuse + specular;
    out_color = vec4(frag_color.rgb * light, frag_color.a);
}
