#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants
{
    uvec2 extent;
} push;

layout(binding = 0) uniform Element
{
    uint x;
    uint y;
    uint width;
    uint height;
    vec4 bg_colour;
    float alpha;
} element;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 outUV;

void main() {
    //coord * (width/WIDTH, height/HEIGHT)
    //coord + (x - width/2), (y - height/2)
    // (x - (WIDTH / 2) + (width / 2) / (WIDTH / 2)
    // (x / (WIDTH / 2)) - 1 + (width / 2) / (WIDTH / 2)
    // (2x / (WIDTH)) - 1 + (width/WIDTH)
    // ((2x + width) / WIDTH) - 1
    //matrix:
    /*
      width/WIDTH   0    0    ((2x + width) / WIDTH) - 1
          0   height/HEIGHT 0 ((2y + height) / HEIGHT) - 1
          0         0    1          0
          0         0    0          1
      */
    gl_Position = vec4(inPosition.x * (element.width / float(push.extent.x)) + ((2 * element.x + element.width) / float(push.extent.x)) - 1,
                    inPosition.y * (element.height / float(push.extent.y)) + ((2 * element.y + element.height) / float(push.extent.y)) - 1,
                    0.0, 1.0);
    outUV = inUV;
}
