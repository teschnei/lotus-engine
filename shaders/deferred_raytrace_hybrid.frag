#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : require

layout(binding = 0) uniform sampler2D positionSampler;
layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D lightSampler;
layout(binding = 3) uniform usampler2D materialIndexSampler;

struct Mesh
{
    uint vec_index_offset;
    uint tex_offset;
    float specular_exponent;
    float specular_intensity;
    uint light_type;
};

layout(binding = 4, set = 0) uniform MeshInfo
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

layout(std430, binding = 5) uniform Light
{
    Lights entity;
    Lights landscape;
    vec3 diffuse_dir;
    float _pad;
    float skybox_altitudes[8];
    vec4 skybox_colors[8];
} light;

layout(binding = 6) uniform Camera {
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
    vec4 albedo = texture(albedoSampler, fragTexCoord);
    vec4 in_light = texture(lightSampler, fragTexCoord);
    uint material_index = texture(materialIndexSampler, fragTexCoord).r;
    float dist = length(fragPos - camera.pos.xyz);

    if (fragPos == vec3(0))
    {
        float dot_up = dot(normalize(eye_dir.xyz), vec3(0.f, -1.f, 0.f));
        for (int i = 1; i < 8; i++)
        {
            if (dot_up < light.skybox_altitudes[i])
            {
                float value = (max(dot_up, 0.0) - light.skybox_altitudes[i-1]) / (light.skybox_altitudes[i] - light.skybox_altitudes[i-1]);
                outColor.rgb = mix(light.skybox_colors[i-1], light.skybox_colors[i], value).rgb;
                return;
            }
        }
    }

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

    outColor.rgb = pow(outColor.rgb, vec3(2.2/1.5));
}

