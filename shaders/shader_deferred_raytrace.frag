#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D posSampler;
layout(binding = 1) uniform sampler2D normalSampler;
layout(binding = 2) uniform sampler2D albedoSampler;

layout(binding = 3) uniform CameraUBO
{
    mat4 proj;
    mat4 view;
} camera_ubo;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {

    vec3 fragPos = texture(posSampler, fragTexCoord).xyz;
    vec3 normal = texture(normalSampler, fragTexCoord).xyz;
    vec4 albedo = texture(albedoSampler, fragTexCoord);

    outColor = albedo;
}

