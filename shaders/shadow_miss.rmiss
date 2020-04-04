#version 460
#extension GL_NV_ray_tracing : require

layout(location = 1) rayPayloadInNV Shadow 
{
    bool shadowed;
    vec3 color;
} shadow;

void main()
{
    shadow.shadowed = false;
}