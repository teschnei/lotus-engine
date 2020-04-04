#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D texSampler;

struct Mesh
{
    int vec_index_offset;
    int tex_offset;
    float specular1;
    float specular2;
    vec4 color;
    uint light_type;
};

layout(binding = 3, set = 0) uniform MeshInfo
{
    Mesh m[1024];
} meshInfo;

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
    vec4 particle_color = meshInfo.m[push.material_index].color;
    outParticle.rgb = particle_color.rgb + tex.rgb;
    outParticle.a = ((tex.r + tex.g + tex.b) / 3.0) * particle_color.a;
    //if (outParticle.a <= 1.f/32.f)
    //    discard;
}

