#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 6) uniform CameraUBO
{
    mat4 proj;
    mat4 view;
    mat4 proj_inverse;
    mat4 view_inverse;
    vec4 eye_pos;
} camera_ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 eye_dir;

void main() {
    gl_Position = vec4(inPosition, 1.0);
    eye_dir = camera_ubo.view_inverse * camera_ubo.proj_inverse * gl_Position;
    fragTexCoord = inTexCoord;
}
