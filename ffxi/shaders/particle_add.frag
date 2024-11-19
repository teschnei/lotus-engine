#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2 : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "common.glsl"

layout(set = 2, binding = eMeshInfo) buffer readonly MeshInfo
{
    Mesh m[];
} meshInfo;
layout(set = 2, binding = eTextures) uniform sampler2D textures[];

layout(buffer_reference, scalar) buffer Materials { Material m; };

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
    Mesh mesh = meshInfo.m[push.mesh_index];
    Materials materials = Materials(mesh.material);
    Material mat = materials.m;

    vec4 texture_colour = texture(textures[mat.texture_index], fragTexCoord);
    uint bc2_alpha = uint((texture_colour.a * 255.f)) >> 4;
    if (bc2_alpha == 0)
        discard;
    float texture_alpha = float(bc2_alpha) / 8.0;
    vec4 mesh_colour = mesh.colour;
    vec3 colour = fragColor.rgb * mesh_colour.rgb * texture_colour.rgb;
    float alpha = fragColor.a * mesh_colour.a * texture_alpha;
    outParticle = vec4(colour * alpha, 1.0);
}

