#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT HitValue
{
    float intersection_dist;
} hitValue;

void main()
{
    hitValue.intersection_dist = gl_RayTmaxEXT;
}
