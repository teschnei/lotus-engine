#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    mat4 proj_inverse;
    mat4 view_inverse;
    vec4 eye_pos;
} camera[2];

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in mat4 instanceModelMat;
layout(location = 8) in mat4 instanceModelMat_T;
layout(location = 12) in mat3 instanceModelMat_IT;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 normal;
layout(location = 4) out vec4 pos;
layout(location = 5) out vec4 prevPos;

void main() {
    pos = camera[0].proj * camera[0].view * instanceModelMat * vec4(inPosition, 1.0);
    prevPos = camera[1].proj * camera[1].view * instanceModelMat * vec4(inPosition, 1.0);
    gl_Position = pos;
    fragColor = vec4(inColor, 1.0);
    fragTexCoord = inTexCoord;

    fragPos = (instanceModelMat * vec4(inPosition, 1.0)).xyz;
    normal = normalize(instanceModelMat_IT * inNormal);
}
