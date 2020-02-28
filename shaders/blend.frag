#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

void main() {
    outPosition = vec4(fragPos, 1.0);
    outNormal = vec4(normal, 1.0);
    outAlbedo = texture(texSampler, fragTexCoord);
    outAlbedo.rgb *= fragColor;
    if (outAlbedo.a > 1.f/32.f)
        outAlbedo.a = 1;
    else
        discard;
}

