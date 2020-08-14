#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D albedoSampler;
layout(binding = 1) uniform sampler2D lightSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragPos;

layout(location = 0) out vec4 outColor;

void main() {

    vec4 albedo = texture(albedoSampler, fragTexCoord);
    vec4 light = texture(lightSampler, fragTexCoord);

    outColor = albedo * light;
    outColor.rgb = pow(outColor.rgb, vec3(2.2/1.5));
}

