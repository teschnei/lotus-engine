import lotus;
import lotus.raytrace;
import pbr;

[vk_binding(0,1)] public RWTexture2D output;
[vk_binding(1,1)] public Sampler2D position;
[vk_binding(2,1)] public Sampler2D normal;
[vk_binding(3,1)] public Sampler2D face_normal;
[vk_binding(4,1)] public Sampler2D albedo;
[vk_binding(5,1)] public Sampler2D<uint> material_index;
[vk_binding(7,1)] public ConstantBuffer<lotus::Camera> camera;

[shader("raygeneration")]
void Raygen()
{
    float2 uv = float2(float(DispatchRaysIndex().x)/float(DispatchRaysDimensions().x - 1), float(DispatchRaysIndex().y)/float(DispatchRaysDimensions().y - 1));
    float3 position = position.Sample(uv).xyz;
    float3 normal = normal.Sample(uv).xyz;
    float3 face_normal = face_normal.Sample(uv).xyz;
    float4 albedo = albedo.Sample(uv);

    uint mesh_index = material_index.Sample(uv).r;
    lotus::Mesh mesh = meshes[mesh_index];

    float dot_product = dot(-lights[0].light.diffuse_dir, normal);

    float3 diffuse_color = float3(0.0);
    float brightness = 0;

    lotus::Material material = *(lotus::Material*)mesh.material;

    if (material.light_type == 0)
    {
        diffuse_color = lights[0].light.entity.diffuse_color.rgb;
        brightness = lights[0].light.entity.brightness;
    }
    else
    {
        diffuse_color = lights[0].light.landscape.diffuse_color.rgb;
        brightness = lights[0].light.landscape.brightness;
    }

    float3 trace_origin = position + face_normal * 0.001;
    float3 diffuse = float3(0.0);
    uint seed = lotus::rand.tea(DispatchRaysIndex().y * DispatchRaysDimensions().x + DispatchRaysIndex().x, int(clockARB()));

    for (int i = 0; i < lights[0].light.light_count; i++)
    {
        float2 u = float2(lotus.rand::rnd(seed), lotus.rand::rnd(seed));
        diffuse += lotus::TraceLight(lights[0].light_info, trace_origin, normal, u);
    }

    float3 tangent, bitangent;
    pbr::createCoordinateSystem(normal, tangent, bitangent);

    lotus::RayPayload payload;

    payload.depth = 0;
    payload.seed = seed;
    payload.weight = 0.0;

    const int max_samples = 1;
    float3 BRDF = (albedo.xyz / M_PI);
    float3 accumulation = diffuse;
    RayDesc ray;
    ray.TMin = 0.0;
    ray.TMax = 1000.0;
    for (int samples = 0; samples < max_samples; ++samples)
    {
        //BRDF * M_PI
        float3 weight = albedo.rgb;
        float pdf;
        payload.origin = position.xyz;
        float3 sample = pbr::SampleCosineHemisphere(float2(lotus.rand::rnd(payload.seed), lotus.rand::rnd(payload.seed)));
        payload.direction = normalize(sample.x * tangent + sample.y * bitangent + sample.z * normal);
        for(; payload.depth < 1; payload.depth++)
        {
            ray.Origin = payload.origin;
            ray.Direction = payload.direction;
            TraceRay(topLevelAS, 0, 0x01 | 0x02, 0, 0, 1, ray, payload);
            float3 factor = payload.BRDF * weight;
            accumulation += payload.diffuse * factor;
            weight *= factor;
        }
    }

    output[DispatchRaysIndex().xy] = (float4(accumulation, 0.0) / float(max_samples)) / M_PI;
}

[shader("miss")]
void MissGI(inout lotus::RayPayload payload)
{
    lotus::MissGI(payload);
}

[shader("miss")]
void MissShadow(inout lotus::ShadowPayload payload)
{
    lotus::MissShadow(payload);
}
