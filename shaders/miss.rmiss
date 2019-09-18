#version 460
#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV HitValue
{
    vec3 color;
    uint max_index;
    float min_dist;
} hitValue;

void main()
{
    hitValue.color = vec3(0.0, 0.1, 0.3);
}