#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform CameraUBO {
    mat4 proj;
    mat4 view;
    mat4 proj_inverse;
    mat4 view_inverse;
    vec4 eye_pos;
} camera;

layout(binding = 2) uniform ModelUBO {
    mat4 model;
    mat3 model_IT;
    vec4 _pad;
    mat4 model_prev;
} model;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3, set = 1) in vec3 inPrevPosition;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 normal;
layout(location = 4) out vec4 pos;
layout(location = 5) out vec4 prevPos;

void main() {
    pos = camera.proj * camera.view * model.model * vec4(inPosition, 1.0);
    prevPos = camera.proj * camera.view * model.model_prev * vec4(inPrevPosition, 1.0);
    gl_Position = pos;
    fragColor = vec4(1.0);
    fragTexCoord = inTexCoord;

    fragPos = (model.model * vec4(inPosition, 1.0)).xyz;
    normal = normalize(model.model_IT * inNormal);
}
