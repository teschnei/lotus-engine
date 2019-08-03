#version 450
#extension GL_ARB_separate_shader_objects : enable

#include "frag_common.frag"

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = handleColor(texSampler, fragTexCoord, fragColor, fragPos, normal);
    if (outColor.a > 0)
        outColor.a = 1;
    else
        discard;
}

