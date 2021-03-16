#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 3, set = 0) uniform MeshBlock
{
    Mesh mesh;
} mesh;

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
    vec4 texture_colour = texture(texSampler, fragTexCoord);
    vec4 mesh_colour = mesh.mesh.colour;
    vec3 colour = fragColor.rgb *  mesh_colour.rgb * texture_colour.rgb * 4;
    float alpha = fragColor.a * mesh_colour.a * texture_colour.a * 2;

    float a = min(1.0, alpha) * 8.0 + 0.01;
    float b = -(1.0 - gl_FragCoord.z) * 0.95 + 1.0;
    float w = clamp(a * a * a * 1e8 * b * b * b, 1e-2, 3e2);
    outAccumulation.rgb = colour * w;
    outAccumulation.a = alpha * w;
    outRevealage = alpha;
}

