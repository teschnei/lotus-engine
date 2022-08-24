#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(binding = 2, set = 1) uniform sampler2D textures[];

layout(binding = 3, set = 1) uniform MaterialInfo
{
    Material m;
} materials[];

layout(binding = 4, set = 1) buffer readonly MeshInfo
{
    Mesh m[];
} meshInfo;

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
    Mesh meshinfo = meshInfo.m[push.mesh_index];
    Material mat = materials[meshinfo.material_index].m;

    vec4 texture_colour = texture(textures[mat.texture_index], fragTexCoord);
    vec4 mesh_colour = meshinfo.colour;
    vec3 colour = fragColor.rgb *  mesh_colour.rgb * texture_colour.rgb * 4;
    float alpha = fragColor.a * mesh_colour.a * texture_colour.a * 2;

    float a = min(1.0, alpha) * 8.0 + 0.01;
    float b = -(1.0 - gl_FragCoord.z) * 0.95 + 1.0;
    float w = clamp(a * a * a * 1e8 * b * b * b, 1e-2, 3e2);
    outAccumulation.rgb = colour * w;
    outAccumulation.a = alpha * w;
    outRevealage = alpha;
}

