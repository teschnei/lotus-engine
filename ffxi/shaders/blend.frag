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
layout(location = 4) in vec4 pos;
layout(location = 5) in vec4 prevPos;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outFaceNormal;
layout(location = 3) out vec4 outAlbedo;
layout(location = 4) out uint outMaterialIndex;
layout(location = 5) out uint outLightType;
layout(location = 6) out vec4 outMotionVector;

layout(push_constant) uniform PushConstant
{
    uint mesh_index;
} push;

void main() {
    outPosition = vec4(fragPos, 1.0);
    outNormal = vec4(normal, 1.0);
    vec3 dx = dFdx(fragPos);
    vec3 dy = dFdy(fragPos);
    vec3 cross_vec = normalize(cross(dx, dy));
    if ((dot(cross_vec, normal)) < 0)
        cross_vec = -cross_vec;

    Mesh mesh = meshInfo.m[push.mesh_index];
    Materials materials = Materials(mesh.material);
    Material mat = materials.m;

    outFaceNormal = vec4(cross_vec, 1.0);
    outAlbedo = texture(textures[mat.texture_index], fragTexCoord);
    uint bc2_alpha = uint((outAlbedo.a * 255.f)) >> 4;
    if (bc2_alpha == 0)
        discard;
    outAlbedo.a = float(bc2_alpha) / 8.0;
    outAlbedo *= fragColor;
    outMaterialIndex = push.mesh_index;
    outLightType = mat.light_type;
    vec2 curScreenPos = (pos.xy / pos.w) * 0.5 + 0.5;
    vec2 prevScreenPos = (prevPos.xy / prevPos.w) * 0.5 + 0.5;
    outMotionVector.xy = vec2(curScreenPos - prevScreenPos);
    outMotionVector.zw = vec2(pos.z, prevPos.z);
}

