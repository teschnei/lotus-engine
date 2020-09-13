#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 inUV;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform Element
{
    uint x;
    uint y;
    uint width;
    uint height;
    vec4 bg_colour;
    float alpha;
} element;

layout(binding = 1) uniform sampler2D texture_sampler;

void main() {
    vec4 tex = texture(texture_sampler, inUV);
    outColor = mix(element.bg_colour, tex, tex.a);
    outColor.a *= element.alpha;
}
