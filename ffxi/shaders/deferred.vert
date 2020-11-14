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

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 eye_dir;

void main() {
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
    eye_dir = camera_ubo.view_inverse * camera_ubo.proj_inverse * gl_Position;
}
