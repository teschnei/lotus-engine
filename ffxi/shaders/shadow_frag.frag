#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;

void main() {
    float alpha = texture(texSampler, fragTexCoord).a;
    if (alpha == 0)
    {
        discard;
    }
}
