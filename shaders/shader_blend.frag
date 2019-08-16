#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "frag_common.frag"

layout(binding = 1) uniform sampler2DArray shadowSampler;
layout(binding = 2) uniform sampler2D texSampler;

layout(binding = 3) uniform UBO
{
    vec4 cascade_splits;
    mat4 cascade_view_proj[4];
    mat4 inverse_view;
    vec3 light_dir;
} ubo;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec3 fragViewPos;

layout(location = 0) out vec4 outColor;

const mat4 bias = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

void main() {
    uint cascade_index = 0;
    for (uint i = 0; i < 4 - 1; ++i)
    {
        if(fragViewPos.z < ubo.cascade_splits[i]) {
            cascade_index = i + 1;
        }
    }

    vec4 shadow_coord = (bias * ubo.cascade_view_proj[cascade_index]) * vec4(fragPos, 1.0);

    float shadow = 1.0;
    vec4 shadowCoord = shadow_coord / shadow_coord.w;

    if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
    {
        float distance = texture(shadowSampler, vec3(shadowCoord.st, cascade_index)).r;
        if (shadowCoord.w > 0 && distance < shadowCoord.z - 0.0005) {
            shadow = 0.5;
        }
    }

    outColor = handleColor(texSampler, fragTexCoord, fragColor, fragPos, normal, ubo.light_dir);
    outColor = outColor * shadow;
    if (outColor.a > 0)
        outColor.a = 1;
    else
        discard;
}

