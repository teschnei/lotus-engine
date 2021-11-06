#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(binding = 0) uniform sampler2D albedoSampler;
layout(binding = 1) uniform sampler2D lightSampler;
layout(binding = 2) uniform sampler2D particleSampler;

layout(std430, binding = 3) buffer readonly Light
{
    LightBuffer light;
    LightInfo light_info[];
} light;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragPos;

layout(location = 0) out vec4 outColor;

void main() {

    vec4 albedo = texture(albedoSampler, fragTexCoord);
    vec4 post_light = texture(lightSampler, fragTexCoord);
    vec3 particle = texture(particleSampler, fragTexCoord).rgb;

    outColor = albedo * post_light;

    /*if (albedo.a > light.light.landscape.min_fog && albedo.a < light.light.landscape.max_fog)
    {
        outColor.rgb = mix(outColor.rgb, light.light.landscape.fog_color.rgb, (albedo.a - light.light.landscape.min_fog) / (light.light.landscape.max_fog - light.light.landscape.min_fog));   
    }
    */
    outColor.rgb += particle;

    float exposure = 1.0;
    outColor.rgb = vec3(1.0) - exp(-outColor.rgb * exposure);

    //outColor.rgb = pow(outColor.rgb, vec3(1.0/2.2));
}

