#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT Shadow
{
    vec4 light;
    vec4 shadow;
} shadow;

void main()
{
    shadow.shadow = shadow.light;
    shadow.shadow.a = 1.0;
}
