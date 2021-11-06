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
} hitValue;

layout(std430, binding = 6, set = 1) buffer readonly Light
{
    LightBuffer light;
    uint light_count;
    LightInfo light_info[];
} light;

void main()
{
    hitValue.BRDF = vec3(1.0) / M_PI;
    hitValue.diffuse = light.light.landscape.ambient_color.rgb;// * light.light.landscape.brightness;
    hitValue.depth = 10;
    return;
}