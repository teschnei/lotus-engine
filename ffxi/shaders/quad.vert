#version 450

layout(binding = 8) uniform Camera {
    mat4 proj;
    mat4 view;
    mat4 proj_inverse;
    mat4 view_inverse;
    vec3 pos;
} camera;

layout (location = 0) out vec2 outUV;
layout (location = 1) out vec4 outPos;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
	outPos = camera.view_inverse * camera.proj_inverse * vec4(outUV * 2.0f - 1.0f, 1.0f, 1.0f);
}
