#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(location = 0) rayPayloadInEXT HitValue
{
    vec3 BRDF;
    vec3 diffuse;
    vec3 normal;
    int depth;
    uint seed;
    float weight;
    vec3 origin;
    vec3 direction;
    float distance;
} hitValue;

layout(std430, binding = 4, set = 1) buffer readonly Light
{
    LightBuffer light;
    uint light_count;
    LightInfo light_info[];
} light;

void main()
{
    hitValue.BRDF = vec3(1.0);
    hitValue.depth = 10;
    hitValue.normal = vec3(1.0);
    hitValue.distance = gl_RayTmaxEXT;
    float dot_up = dot(gl_WorldRayDirectionEXT, vec3(0.f, -1.f, 0.f));

    if (dot_up < light.light.skybox_altitudes2)
    {
        float value = (max(dot_up, 0.0) - light.light.skybox_altitudes1) / (light.light.skybox_altitudes2 - light.light.skybox_altitudes1);
        hitValue.diffuse = mix(light.light.skybox_colors[0], light.light.skybox_colors[1], value).xyz;
        return;
    }
    if (dot_up < light.light.skybox_altitudes3)
    {
        float value = (max(dot_up, 0.0) - light.light.skybox_altitudes2) / (light.light.skybox_altitudes3 - light.light.skybox_altitudes2);
        hitValue.diffuse = mix(light.light.skybox_colors[1], light.light.skybox_colors[2], value).xyz;
        return;
    }
    if (dot_up < light.light.skybox_altitudes4)
    {
        float value = (max(dot_up, 0.0) - light.light.skybox_altitudes3) / (light.light.skybox_altitudes4 - light.light.skybox_altitudes3);
        hitValue.diffuse = mix(light.light.skybox_colors[2], light.light.skybox_colors[3], value).xyz;
        return;
    }
    if (dot_up < light.light.skybox_altitudes5)
    {
        float value = (max(dot_up, 0.0) - light.light.skybox_altitudes4) / (light.light.skybox_altitudes5 - light.light.skybox_altitudes4);
        hitValue.diffuse = mix(light.light.skybox_colors[3], light.light.skybox_colors[4], value).xyz;
        return;
    }
    if (dot_up < light.light.skybox_altitudes6)
    {
        float value = (max(dot_up, 0.0) - light.light.skybox_altitudes5) / (light.light.skybox_altitudes6 - light.light.skybox_altitudes5);
        hitValue.diffuse = mix(light.light.skybox_colors[4], light.light.skybox_colors[5], value).xyz;
        return;
    }
    if (dot_up < light.light.skybox_altitudes7)
    {
        float value = (max(dot_up, 0.0) - light.light.skybox_altitudes6) / (light.light.skybox_altitudes7 - light.light.skybox_altitudes6);
        hitValue.diffuse = mix(light.light.skybox_colors[5], light.light.skybox_colors[6], value).xyz;
        return;
    }
    if (dot_up < light.light.skybox_altitudes8)
    {
        float value = (max(dot_up, 0.0) - light.light.skybox_altitudes7) / (light.light.skybox_altitudes8 - light.light.skybox_altitudes7);
        hitValue.diffuse = mix(light.light.skybox_colors[6], light.light.skybox_colors[7], value).xyz;
        return;
    }
}