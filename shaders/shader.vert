#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in mat4 instanceModelMat;
layout(location = 8) in mat3 instanceModelMat_IT;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 normal;

void main() {
    gl_Position = ubo.proj * ubo.view * instanceModelMat * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;

    fragPos = (instanceModelMat * vec4(inPosition, 1.0)).xyz;
    normal = normalize(instanceModelMat_IT * inNormal);
}
