#version 460
#extension GL_NV_ray_tracing : require

layout(location = 0) rayPayloadInNV HitValue
{
    float intersection_dist;
} hitValue;

void main()
{
    hitValue.intersection_dist = gl_RayTmaxNV;
}
