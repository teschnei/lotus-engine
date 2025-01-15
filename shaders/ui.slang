struct PushConstants
{
    uint2 extent;
};

[vk_push_constant]
uniform PushConstants push;

struct Element
{
    uint x;
    uint y;
    uint width;
    uint height;
    float4 bg_colour;
    float alpha;
};

[[vk_binding(0)]] ConstantBuffer<Element> element;
[[vk_binding(1)]] Sampler2D texture;

[shader("vertex")]
void Vertex(float2 inPos, float2 inUV, out float4 pos : SV_Position, out float2 outUV : UV) {
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
    pos = float4(inPos.x * (element.width / float(push.extent.x)) + ((2 * element.x + element.width) / float(push.extent.x)) - 1,
                    inPos.y * (element.height / float(push.extent.y)) + ((2 * element.y + element.height) / float(push.extent.y)) - 1,
                    0.0, 1.0);
    outUV = inUV;
}

[shader("fragment")]
void Fragment(float2 uv : UV, out float4 colour) {
    float4 tex = texture.Sample(uv);
    colour = lerp(element.bg_colour, tex, tex.a);
    colour.a *= element.alpha;
}
