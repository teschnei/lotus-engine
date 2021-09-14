#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(binding = 1) uniform sampler2D texSampler;

layout(binding = 4, set = 0) uniform MaterialBlock
{
    Material material;
} material;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outFaceNormal;
layout(location = 3) out vec4 outAlbedo;
layout(location = 4) out uint outMaterialIndex;
layout(location = 5) out uint outLightType;

layout(push_constant) uniform PushConstant
{
    uint material_index;
} push;

void main() {
    outPosition = vec4(fragPos, 1.0);
    outNormal = vec4(normal, 1.0);
    vec3 dx = dFdx(fragPos);
    vec3 dy = dFdy(fragPos);
    vec3 cross_vec = normalize(cross(dx, dy));
    if ((dot(cross_vec, normal)) < 0)
        cross_vec = -cross_vec;

    outFaceNormal = vec4(cross_vec, 1.0);
    outAlbedo = texture(texSampler, fragTexCoord);
    outAlbedo.rgb *= fragColor;
    if (outAlbedo.a > 1.f/32.f)
        outAlbedo.a = 1;
    else
        outAlbedo.a = 0;
    outMaterialIndex = push.material_index;
    outLightType = material.material.light_type;
}
