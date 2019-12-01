#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2) uniform ModelUBO {
    mat4 model;
    mat3 model_IT;
} model;

layout(binding = 3) uniform CascadeUBO
{
    vec4 cascade_splits;
    mat4 cascade_view_proj[4];
    mat4 inverse_view;
} cascade_ubo;

layout(push_constant) uniform PushConstants
{
    uint cascade;
} push_constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    fragTexCoord = inTexCoord;
    gl_Position = cascade_ubo.cascade_view_proj[push_constants.cascade] * model.model * vec4(inPosition, 1.0);
}
