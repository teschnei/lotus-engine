#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(binding = 0) uniform sampler2D positionSampler;
layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D lightSampler;
layout(binding = 3) uniform usampler2D materialIndexSampler;
layout(binding = 4) uniform sampler2D accumulationSampler;
layout(binding = 5) uniform sampler2D revealageSampler;

layout(binding = 6, set = 0) uniform MeshInfo
{
    Mesh m[1024];
} meshInfo;

struct Lights
{
    vec4 diffuse_color;
    vec4 specular_color;
    vec4 ambient_color;
    vec4 fog_color;
    float max_fog;
    float min_fog;
    float brightness;
    float _pad;
};

layout(std430, binding = 7) uniform Light
{
    Lights entity;
    Lights landscape;
    vec3 diffuse_dir;
    float _pad;
    float skybox_altitudes1;
    float skybox_altitudes2;
    float skybox_altitudes3;
    float skybox_altitudes4;
    float skybox_altitudes5;
    float skybox_altitudes6;
    float skybox_altitudes7;
    float skybox_altitudes8;
    vec4 skybox_colors[8];
} light;

layout(binding = 8) uniform Camera {
    mat4 proj;
    mat4 view;
    mat4 proj_inverse;
    mat4 view_inverse;
    vec3 pos;
} camera;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 eye_dir;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 fragPos = texture(positionSampler, fragTexCoord).xyz;
    vec4 accumulation = texture(accumulationSampler, fragTexCoord);
    float revealage = texture(revealageSampler, fragTexCoord).x;

    if (fragPos == vec3(0))
    {
        float dot_up = dot(normalize(eye_dir.xyz), vec3(0.f, -1.f, 0.f));

        if (dot_up < light.skybox_altitudes2)
        {
            float value = (max(dot_up, 0.0) - light.skybox_altitudes1) / (light.skybox_altitudes2 - light.skybox_altitudes1);
            outColor.rgb = mix(light.skybox_colors[0], light.skybox_colors[1], value).xyz;
        }
        else if (dot_up < light.skybox_altitudes3)
        {
            float value = (max(dot_up, 0.0) - light.skybox_altitudes2) / (light.skybox_altitudes3 - light.skybox_altitudes2);
            outColor.rgb = mix(light.skybox_colors[1], light.skybox_colors[2], value).xyz;
        }
        else if (dot_up < light.skybox_altitudes4)
        {
            float value = (max(dot_up, 0.0) - light.skybox_altitudes3) / (light.skybox_altitudes4 - light.skybox_altitudes3);
            outColor.rgb = mix(light.skybox_colors[2], light.skybox_colors[3], value).xyz;
        }
        else if (dot_up < light.skybox_altitudes5)
        {
            float value = (max(dot_up, 0.0) - light.skybox_altitudes4) / (light.skybox_altitudes5 - light.skybox_altitudes4);
            outColor.rgb = mix(light.skybox_colors[3], light.skybox_colors[4], value).xyz;
        }
        else if (dot_up < light.skybox_altitudes6)
        {
            float value = (max(dot_up, 0.0) - light.skybox_altitudes5) / (light.skybox_altitudes6 - light.skybox_altitudes5);
            outColor.rgb = mix(light.skybox_colors[4], light.skybox_colors[5], value).xyz;
        }
        else if (dot_up < light.skybox_altitudes7)
        {
            float value = (max(dot_up, 0.0) - light.skybox_altitudes6) / (light.skybox_altitudes7 - light.skybox_altitudes6);
            outColor.rgb = mix(light.skybox_colors[5], light.skybox_colors[6], value).xyz;
        }
        else if (dot_up < light.skybox_altitudes8)
        {
            float value = (max(dot_up, 0.0) - light.skybox_altitudes7) / (light.skybox_altitudes8 - light.skybox_altitudes7);
            outColor.rgb = mix(light.skybox_colors[6], light.skybox_colors[7], value).xyz;
        }
    }
    else
    {
        vec4 albedo = texture(albedoSampler, fragTexCoord);
        vec4 in_light = texture(lightSampler, fragTexCoord);
        uint material_index = texture(materialIndexSampler, fragTexCoord).r;
        float dist = length(fragPos - camera.pos.xyz);

        vec3 fog = vec3(0.0);
        float max_fog_dist = 0;
        float min_fog_dist = 0;

        if (meshInfo.m[material_index].light_type == 0)
        {
            fog = light.entity.fog_color.rgb;
            max_fog_dist = light.entity.max_fog;
            min_fog_dist = light.entity.min_fog;
        }
        else
        {
            fog = light.landscape.fog_color.rgb;
            max_fog_dist = light.landscape.max_fog;
            min_fog_dist = light.landscape.min_fog;
        }

        if (dist > max_fog_dist)
        {
            outColor = vec4(fog, 1.0);
        }
        else if (dist > min_fog_dist)
        {
            outColor = mix(albedo * in_light, vec4(fog, 1.0), (dist - min_fog_dist) / (max_fog_dist - min_fog_dist));
        }
        else
        {
            outColor = albedo * in_light;
        }
    }

    outColor.rgb = mix(accumulation.rgb / max(accumulation.a, 0.00001), outColor.rgb, revealage);

    outColor.rgb = pow(outColor.rgb, vec3(2.2/1.5));
}

