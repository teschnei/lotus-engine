#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D posSampler;
layout(binding = 1) uniform sampler2D normalSampler;
layout(binding = 2) uniform sampler2D albedoSampler;

layout(binding = 3) uniform CameraUBO
{
    mat4 proj;
    mat4 view;
    mat4 proj_inverse;
    mat4 view_inverse;
} camera_ubo;

layout(binding = 4) uniform LightUBO
{
    vec3 diffuse_dir;
    float pad;
    vec3 diffuse_color;
    float pad2;
    vec3 ambient_color;
    float min_fog;
    vec3 fog_color;
    float max_fog;
    float skybox_altitudes[8];
    vec3 skybox_colors[8];
} light_ubo;


layout(binding = 5) uniform CascadeUBO
{
    vec4 cascade_splits;
    mat4 cascade_view_proj[4];
    mat4 inverse_view;
} cascade_ubo;

layout(binding = 6) uniform sampler2DArray shadowSampler;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

const mat4 bias = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.5, 0.5, 0.0, 1.0);

void main() {

    vec3 fragPos = texture(posSampler, fragTexCoord).xyz;
    vec3 normal = texture(normalSampler, fragTexCoord).xyz;
    vec4 albedo = texture(albedoSampler, fragTexCoord);

    vec3 fragViewPos = (camera_ubo.view * vec4(fragPos, 1.0)).xyz;

    uint cascade_index = 0;
    for (uint i = 0; i < 4 - 1; ++i)
    {
        if(fragViewPos.z < cascade_ubo.cascade_splits[i]) {
            cascade_index = i + 1;
        }
    }

    vec4 shadow_coord = (bias * cascade_ubo.cascade_view_proj[cascade_index]) * vec4(fragPos, 1.0);

    float shadow = 1.0;
    vec4 shadowCoord = shadow_coord / shadow_coord.w;

    if (shadowCoord.z > -1.0 && shadowCoord.z < 1.0)
    {
        float distance = texture(shadowSampler, vec3(shadowCoord.st, cascade_index)).r;
        if (shadowCoord.w > 0 && distance < shadowCoord.z - 0.0005) {
            shadow = 0.5;
        }
    }

    outColor = albedo;
    vec3 norm = normalize(normal);
    float theta = clamp(dot(norm, -light_ubo.diffuse_dir), 0.5, 1);
    outColor.rgb = outColor.rgb * theta;
    outColor = outColor * shadow;
}

