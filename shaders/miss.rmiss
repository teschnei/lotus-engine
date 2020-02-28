#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) rayPayloadInNV HitValue
{
    vec3 color;
    uint max_index;
    float min_dist;
} hitValue;

struct Lights
{
    vec4 diffuse;
    vec4 specular;
    vec4 ambient;
    vec4 fog_color;
    float max_fog;
    float min_fog;
    float brightness;
    float _pad;
};

layout(std430, binding = 2, set = 1) uniform Light
{
    Lights entity;
    Lights landscape;
    vec3 diffuse_dir;
    float _pad;
    float skybox_altitudes[8];
    vec4 skybox_colors[8];
} light;

void main()
{
    float dot_up = dot(gl_WorldRayDirectionNV, vec3(0.f, -1.f, 0.f));
    for (int i = 1; i < 8; i++)
    {
        if (dot_up < light.skybox_altitudes[i])
        {
            float value = (max(dot_up, 0.0) - light.skybox_altitudes[i-1]) / (light.skybox_altitudes[i] - light.skybox_altitudes[i-1]);
            hitValue.color = mix(light.skybox_colors[i-1], light.skybox_colors[i], value).xyz;
            break;
        }
    }
}