#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(binding = 1) uniform cascadeUBO
{
    mat4[4] cascade_view_proj;
} cascade_ubo;

layout(push_constant) uniform PushConstants
{
    uint cascade;
} push_constants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in mat4 instanceModelMat;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    fragTexCoord = inTexCoord;
    gl_Position = cascade_ubo.cascade_view_proj[push_constants.cascade] * ubo.model * instanceModelMat * vec4(inPosition, 1.0);
}
