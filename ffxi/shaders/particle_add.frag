#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 3, set = 0) buffer readonly MeshBlock
{
    Mesh m[1024];
} mesh;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec4 outAccumulation;
layout(location = 1) out float outRevealage;
layout(location = 2) out vec4 outParticle;

layout(push_constant) uniform PushConstant
{
    uint mesh_index;
} push;

void main() {
    vec4 texture_colour = texture(texSampler, fragTexCoord);
    uint bc2_alpha = uint((texture_colour.a * 255.f)) >> 4;
    if (bc2_alpha == 0)
        discard;
    float texture_alpha = float(bc2_alpha) / 8.0;
    vec4 mesh_colour = mesh.m[push.mesh_index].colour;
    vec3 colour = fragColor.rgb * mesh_colour.rgb * texture_colour.rgb;
    float alpha = fragColor.a * mesh_colour.a * texture_alpha;
    outParticle = vec4(colour * alpha, 1.0);
}

