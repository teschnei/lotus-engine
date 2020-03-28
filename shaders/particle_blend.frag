#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec4 outParticle;

layout(push_constant) uniform PushConstant
{
    uint material_index;
} push;

void main() {
    vec4 tex = texture(texSampler, fragTexCoord);
    outParticle.rgb = fragColor.rgb * tex.rgb;
    outParticle.a = (tex.r + tex.g + tex.b) / 3.0;
    //if (outParticle.a <= 1.f/32.f)
    //    discard;
}

