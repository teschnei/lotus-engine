#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "common.glsl"

layout(binding = 0) uniform sampler2D positionSampler;
layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D lightSampler;
layout(binding = 3) uniform usampler2D materialIndexSampler;
layout(binding = 4) uniform sampler2D accumulationSampler;
layout(binding = 5) uniform sampler2D revealageSampler;

layout(std430, binding = 6) buffer readonly Light
{
    LightBuffer light;
    uint light_count;
    LightInfo light_info[];
} light;

layout(binding = 7) uniform usampler2D lightTypeSampler;
layout(binding = 8) uniform sampler2D particleSampler;

layout(binding = 9) uniform Camera {
    mat4 proj;
    mat4 view;
    mat4 proj_inverse;
    mat4 view_inverse;
    vec4 eye_pos;
} camera;


layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 eye_dir;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 fragPos = texture(positionSampler, fragTexCoord).xyz;
    vec4 accumulation = texture(accumulationSampler, fragTexCoord);
    float revealage = texture(revealageSampler, fragTexCoord).x;
    vec3 particle = texture(particleSampler, fragTexCoord).xyz;

    if (fragPos == vec3(0))
    {
        float dot_up = dot(normalize(eye_dir.xyz), vec3(0.f, -1.f, 0.f));

        if (dot_up < light.light.skybox_altitudes2)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes1) / (light.light.skybox_altitudes2 - light.light.skybox_altitudes1);
            outColor.rgb = mix(light.light.skybox_colors[0], light.light.skybox_colors[1], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes3)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes2) / (light.light.skybox_altitudes3 - light.light.skybox_altitudes2);
            outColor.rgb = mix(light.light.skybox_colors[1], light.light.skybox_colors[2], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes4)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes3) / (light.light.skybox_altitudes4 - light.light.skybox_altitudes3);
            outColor.rgb = mix(light.light.skybox_colors[2], light.light.skybox_colors[3], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes5)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes4) / (light.light.skybox_altitudes5 - light.light.skybox_altitudes4);
            outColor.rgb = mix(light.light.skybox_colors[3], light.light.skybox_colors[4], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes6)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes5) / (light.light.skybox_altitudes6 - light.light.skybox_altitudes5);
            outColor.rgb = mix(light.light.skybox_colors[4], light.light.skybox_colors[5], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes7)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes6) / (light.light.skybox_altitudes7 - light.light.skybox_altitudes6);
            outColor.rgb = mix(light.light.skybox_colors[5], light.light.skybox_colors[6], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes8)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes7) / (light.light.skybox_altitudes8 - light.light.skybox_altitudes7);
            outColor.rgb = mix(light.light.skybox_colors[6], light.light.skybox_colors[7], value).xyz;
        }
    }
    else
    {
        vec4 colour = texture(albedoSampler, fragTexCoord) * texture(lightSampler, fragTexCoord);
        uint light_type = texture(lightTypeSampler, fragTexCoord).r;
        float dist = length(fragPos - camera.eye_pos.xyz);

        vec3 fog = vec3(0.0);
        float max_fog_dist = 0;
        float min_fog_dist = 0;

        if (light_type == 0)
        {
            fog = light.light.entity.fog_color.rgb;
            max_fog_dist = light.light.entity.max_fog;
            min_fog_dist = light.light.entity.min_fog;
        }
        else
        {
            fog = light.light.landscape.fog_color.rgb;
            max_fog_dist = light.light.landscape.max_fog;
            min_fog_dist = light.light.landscape.min_fog;
        }

        if (dist > max_fog_dist)
        {
            outColor = vec4(fog, 1.0);
        }
        else if (dist > min_fog_dist)
        {
            outColor = mix(colour, vec4(fog, 1.0), (dist - min_fog_dist) / (max_fog_dist - min_fog_dist));
        }
        else
        {
            outColor = colour;
        }
    }

    if (accumulation.a > 0)
        outColor.rgb = mix(accumulation.rgb / max(accumulation.a, 0.00001), outColor.rgb, revealage);

    outColor.rgb += particle;
}

