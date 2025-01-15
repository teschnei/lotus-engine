import lotus;

[vk_binding(0)] Sampler2D position;
[vk_binding(1)] Sampler2D albedo;
[vk_binding(2)] Sampler2D illuminance;
[vk_binding(3)] Sampler2D<uint> materialIndex;
[vk_binding(4)] Sampler2D accumulation;
[vk_binding(5)] Sampler2D revealage;
[vk_binding(6)] StructuredBuffer<lotus::Lights> lights;
[vk_binding(7)] Sampler2D<uint> lightType;
[vk_binding(8)] Sampler2D particle;
[vk_binding(9)] ConstantBuffer<lotus::Camera> camera;

struct Params
{
    float2 uv : UV;
    float4 eyeDir : POSITION;
};

[shader("vertex")]
float4 Vertex(uint index : SV_VertexID, out Params params) : SV_Position
{
	params.uv = float2((index << 1) & 2, index & 2);
	params.eyeDir = mul(camera.view_inverse, mul(camera.proj_inverse, float4(params.uv * 2.0f - 1.0f, 1.0f, 1.0f)));
	return float4(params.uv * 2.0f - 1.0f, 0.0f, 1.0f);
}

[shader("fragment")]
float4 Fragment(Params input) {
    float4 outColor;
    float3 fragPos = position.Sample(input.uv).xyz;
    float4 accumulation = accumulation.Sample(input.uv);
    float revealage = revealage.Sample(input.uv).x;
    float3 particle = particle.Sample(input.uv).xyz;

    if (all(fragPos == float3(0)))
    {
        float dot_up = dot(normalize(input.eyeDir.xyz), float3(0.f, -1.f, 0.f));

        if (dot_up < lights[0].light.skybox_altitudes2)
        {
            float value = (max(dot_up, 0.0) - lights[0].light.skybox_altitudes1) / (lights[0].light.skybox_altitudes2 - lights[0].light.skybox_altitudes1);
            outColor.rgb = lerp(lights[0].light.skybox_colors[0], lights[0].light.skybox_colors[1], value).xyz;
        }
        else if (dot_up < lights[0].light.skybox_altitudes3)
        {
            float value = (max(dot_up, 0.0) - lights[0].light.skybox_altitudes2) / (lights[0].light.skybox_altitudes3 - lights[0].light.skybox_altitudes2);
            outColor.rgb = lerp(lights[0].light.skybox_colors[1], lights[0].light.skybox_colors[2], value).xyz;
        }
        else if (dot_up < lights[0].light.skybox_altitudes4)
        {
            float value = (max(dot_up, 0.0) - lights[0].light.skybox_altitudes3) / (lights[0].light.skybox_altitudes4 - lights[0].light.skybox_altitudes3);
            outColor.rgb = lerp(lights[0].light.skybox_colors[2], lights[0].light.skybox_colors[3], value).xyz;
        }
        else if (dot_up < lights[0].light.skybox_altitudes5)
        {
            float value = (max(dot_up, 0.0) - lights[0].light.skybox_altitudes4) / (lights[0].light.skybox_altitudes5 - lights[0].light.skybox_altitudes4);
            outColor.rgb = lerp(lights[0].light.skybox_colors[3], lights[0].light.skybox_colors[4], value).xyz;
        }
        else if (dot_up < lights[0].light.skybox_altitudes6)
        {
            float value = (max(dot_up, 0.0) - lights[0].light.skybox_altitudes5) / (lights[0].light.skybox_altitudes6 - lights[0].light.skybox_altitudes5);
            outColor.rgb = lerp(lights[0].light.skybox_colors[4], lights[0].light.skybox_colors[5], value).xyz;
        }
        else if (dot_up < lights[0].light.skybox_altitudes7)
        {
            float value = (max(dot_up, 0.0) - lights[0].light.skybox_altitudes6) / (lights[0].light.skybox_altitudes7 - lights[0].light.skybox_altitudes6);
            outColor.rgb = lerp(lights[0].light.skybox_colors[5], lights[0].light.skybox_colors[6], value).xyz;
        }
        else if (dot_up < lights[0].light.skybox_altitudes8)
        {
            float value = (max(dot_up, 0.0) - lights[0].light.skybox_altitudes7) / (lights[0].light.skybox_altitudes8 - lights[0].light.skybox_altitudes7);
            outColor.rgb = lerp(lights[0].light.skybox_colors[6], lights[0].light.skybox_colors[7], value).xyz;
        }
    }
    else
    {
        float4 colour = albedo.Sample(input.uv) * illuminance.Sample(input.uv);
        uint light_type = lightType.Sample(input.uv);
        float dist = length(fragPos - camera.eye_pos.xyz);

        float3 fog = float3(0.0);
        float max_fog_dist = 0;
        float min_fog_dist = 0;

        if (light_type == 0)
        {
            fog = lights[0].light.entity.fog_color.rgb;
            max_fog_dist = lights[0].light.entity.max_fog;
            min_fog_dist = lights[0].light.entity.min_fog;
        }
        else
        {
            fog = lights[0].light.landscape.fog_color.rgb;
            max_fog_dist = lights[0].light.landscape.max_fog;
            min_fog_dist = lights[0].light.landscape.min_fog;
        }

        if (dist > max_fog_dist)
        {
            outColor = float4(fog, 1.0);
        }
        else if (dist > min_fog_dist)
        {
            outColor = lerp(colour, float4(fog, 1.0), (dist - min_fog_dist) / (max_fog_dist - min_fog_dist));
        }
        else
        {
            outColor = colour;
        }
    }

    if (accumulation.a > 0)
        outColor.rgb = lerp(accumulation.rgb / max(accumulation.a, 0.00001), outColor.rgb, revealage);

    outColor.rgb += particle;
    return outColor;
}

