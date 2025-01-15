import lotus;

[vk_binding(0)] Sampler2D albedoSampler;
[vk_binding(1)] Sampler2D lightSampler;
[vk_binding(2)] Sampler2D particleSampler;
[vk_binding(3)] StructuredBuffer<lotus::Lights> lights;
[vk_binding(9)] ConstantBuffer<lotus::Camera> camera;

struct Params
{
    float2 uv : UV;
    float4 pos : POSITION;
};

[shader("vertex")]
float4 Vertex(uint index : SV_VertexID, out Params params) : SV_Position
{
	params.uv = float2((index << 1) & 2, index & 2);
	params.pos = mul(camera.view_inverse, mul(camera.proj_inverse, float4(params.uv * 2.0f - 1.0f, 1.0f, 1.0f)));
	return float4(params.uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

[shader("fragment")]
float4 Fragment(Params params) {

    float4 albedo = albedoSampler.Sample(params.uv);
    float4 post_light = lightSampler.Sample(params.uv);
    float3 particle = particleSampler.Sample(params.uv).rgb;

    float4 outColor = albedo * post_light;

    if (albedo.a > lights[0].light.landscape.min_fog && albedo.a < lights[0].light.landscape.max_fog)
    {
        outColor.rgb = lerp(outColor.rgb, lights[0].light.landscape.fog_color.rgb, (albedo.a - lights[0].light.landscape.min_fog) / (lights[0].light.landscape.max_fog - lights[0].light.landscape.min_fog));
    }

    outColor.rgb += particle;

    //float exposure = 1.0;
    //outColor.rgb = float3(1.0) - exp(-outColor.rgb * exposure);

    return outColor;
}

