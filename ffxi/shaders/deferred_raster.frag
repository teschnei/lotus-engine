#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(binding = 0) uniform sampler2D posSampler;
layout(binding = 1) uniform sampler2D normalSampler;
layout(binding = 2) uniform sampler2D albedoSampler;
layout(binding = 3) uniform usampler2D materialSampler;
layout(binding = 4) uniform sampler2D accumulationSampler;
layout(binding = 5) uniform sampler2D revealageSampler;

layout(binding = 6) uniform CameraUBO
{
    mat4 proj;
    mat4 view;
    mat4 proj_inverse;
    mat4 view_inverse;
    vec4 eye_pos;
} camera_ubo;

layout(std430, binding = 7) buffer readonly Light
{
    LightBuffer light;
    uint light_count;
    LightInfo light_info[];
} light;

layout(binding = 8) uniform CascadeUBO
{
    vec4 cascade_splits;
    mat4 cascade_view_proj[4];
    mat4 inverse_view;
} cascade_ubo;

layout(binding = 9) uniform sampler2DArray shadowSampler;

layout(binding = 10) uniform MaterialInfo
{
    Material m;
} materials[1024];

layout(binding = 11) uniform MeshInfo
{
    Mesh m[1024];
} meshInfo;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 eye_dir;

layout(location = 0) out vec4 outColor;

const mat4 bias = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

void main() {

    vec3 fragPos = texture(posSampler, fragTexCoord).xyz;
    vec4 accumulation = texture(accumulationSampler, fragTexCoord);
    float revealage = texture(revealageSampler, fragTexCoord).r;

    if (fragPos == vec3(0))
    {
        float dot_up = dot(normalize(eye_dir.xyz), vec3(0.f, -1.f, 0.f));

        if (dot_up < light.light.skybox_altitudes2)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes1) / (light.light.skybox_altitudes2 - light.light.skybox_altitudes1);
            outColor.rgb = mix(light.light.skybox_colors[0], light.light.skybox_colors[1], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes3)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes2) / (light.light.skybox_altitudes3 - light.light.skybox_altitudes2);
            outColor.rgb = mix(light.light.skybox_colors[1], light.light.skybox_colors[2], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes4)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes3) / (light.light.skybox_altitudes4 - light.light.skybox_altitudes3);
            outColor.rgb = mix(light.light.skybox_colors[2], light.light.skybox_colors[3], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes5)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes4) / (light.light.skybox_altitudes5 - light.light.skybox_altitudes4);
            outColor.rgb = mix(light.light.skybox_colors[3], light.light.skybox_colors[4], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes6)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes5) / (light.light.skybox_altitudes6 - light.light.skybox_altitudes5);
            outColor.rgb = mix(light.light.skybox_colors[4], light.light.skybox_colors[5], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes7)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes6) / (light.light.skybox_altitudes7 - light.light.skybox_altitudes6);
            outColor.rgb = mix(light.light.skybox_colors[5], light.light.skybox_colors[6], value).xyz;
        }
        else if (dot_up < light.light.skybox_altitudes8)
        {
            float value = (max(dot_up, 0.0) - light.light.skybox_altitudes7) / (light.light.skybox_altitudes8 - light.light.skybox_altitudes7);
            outColor.rgb = mix(light.light.skybox_colors[6], light.light.skybox_colors[7], value).xyz;
        }
    }
    else
    {
        vec3 normal = texture(normalSampler, fragTexCoord).xyz;
        vec4 albedo = texture(albedoSampler, fragTexCoord);
        uint mesh_index = texture(materialSampler, fragTexCoord).r;
        float dist = length(fragPos - camera_ubo.eye_pos.xyz);

        vec3 ambient_color = vec3(0.0);
        vec3 specular_color = vec3(0.0);
        vec3 diffuse_color = vec3(0.0);
        vec3 fog = vec3(0.0);
        float max_fog_dist = 0;
        float min_fog_dist = 0;
        float brightness = 0;

        if (materials[meshInfo.m[mesh_index].material_index].m.light_type == 0)
        {
            ambient_color = light.light.entity.ambient_color.rgb;
            specular_color = light.light.entity.specular_color.rgb;
            diffuse_color = light.light.entity.diffuse_color.rgb;
            fog = light.light.entity.fog_color.rgb;
            max_fog_dist = light.light.entity.max_fog;
            min_fog_dist = light.light.entity.min_fog;
            brightness = light.light.entity.brightness;
        }
        else
        {
            ambient_color = light.light.landscape.ambient_color.rgb;
            specular_color = light.light.landscape.specular_color.rgb;
            diffuse_color = light.light.landscape.diffuse_color.rgb;
            fog = light.light.landscape.fog_color.rgb;
            max_fog_dist = light.light.landscape.max_fog;
            min_fog_dist = light.light.landscape.min_fog;
            brightness = light.light.landscape.brightness;
        }

        if (dist > max_fog_dist)
        {
            outColor.rgb = fog;
        }
        else
        {
            vec3 fragViewPos = (camera_ubo.view * vec4(fragPos, 1.0)).xyz;

            uint cascade_index = 0;
            for (uint i = 0; i < 4 - 1; ++i)
            {
                if(fragViewPos.z < cascade_ubo.cascade_splits[i]) {
                    cascade_index = i + 1;
                }
            }

            vec4 shadow_coord = (bias * cascade_ubo.cascade_view_proj[cascade_index]) * vec4(fragPos, 1.0);

            bool shadow = false;
            vec4 shadowCoord = shadow_coord / shadow_coord.w;

            if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
            {
                float distance = texture(shadowSampler, vec3(shadowCoord.st, cascade_index)).r;
                if (shadowCoord.w > 0 && distance < shadowCoord.z - 0.0005) {
                    shadow = true;
                }
            }

            vec3 normalized_normal = normalize(normal);

            float dot_product = dot(-light.light.diffuse_dir, normalized_normal);

            vec3 ambient = ambient_color;
            vec3 diffuse = vec3(0.0);

            if (!shadow)
            {
                //TODO: get material for specular
                diffuse = vec3(max(dot_product, 0.0)) * diffuse_color * brightness;
            }
            
            vec3 out_light = diffuse + ambient;
            outColor.rgb = albedo.rgb * out_light;

            if (dist > min_fog_dist)
            {
                outColor.rgb = mix(outColor.rgb, fog, (dist - min_fog_dist) / (max_fog_dist - min_fog_dist));
            }
        }
    }

    outColor.rgb = mix(accumulation.rgb / max(accumulation.a, 0.00001), outColor.rgb, revealage);

    outColor.rgb = pow(outColor.rgb, vec3(2.2/1.5));
}

