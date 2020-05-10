#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

struct Mesh
{
    int vec_index_offset;
    int tex_offset;
    float specular1;
    float specular2;
    vec4 color;
    uint light_type;
    uint indices;
};

layout(binding = 3, set = 0) uniform MeshInfo
{
    Mesh m[1024];
} meshInfo;

layout(binding = 4, set = 0) uniform EntityIndex
{
    uint index;
} entity;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec4 outAccumulation;
layout(location = 1) out float outRevealage;

layout(push_constant) uniform PushConstant
{
    uint mesh_index;
} push;

void main() {
    vec4 tex = texture(texSampler, fragTexCoord);
    vec4 particle_color = meshInfo.m[entity.index + push.mesh_index].color;
    float tex_a = (tex.r + tex.g + tex.b) * (1.0 / 3.0);
    if (tex_a < 1.f/16.f)
        discard;
    float alpha = tex_a;
    float a = min(1.0, alpha) * 8.0 + 0.01;
    float b = -(1.0 - gl_FragCoord.z) * 0.95 + 1.0;
    float w = clamp(a * a * a * 1e8 * b * b * b, 1e-2, 3e2);
    outAccumulation.rgb = tex.rgb * w;
    outAccumulation.a = alpha * w;
    outRevealage = alpha;
}

