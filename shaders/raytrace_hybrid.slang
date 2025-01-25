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
    /*
    float2 uv = float2(float(DispatchRaysIndex().x)/float(DispatchRaysDimensions().x - 1), float(DispatchRaysIndex().y)/float(DispatchRaysDimensions().y - 1));
    float3 position = position.SampleLevel(uv, 0).xyz;
    float3 normal = normal.SampleLevel(uv, 0).xyz;
    float3 face_normal = face_normal.SampleLevel(uv, 0).xyz;
    float4 albedo = albedo.SampleLevel(uv, 0);

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
    uint seed = pbr::rand.tea(DispatchRaysIndex().y * DispatchRaysDimensions().x + DispatchRaysIndex().x, int(clockARB()));

    var payload = lotus::RayPayload(pbr::rand.tea(DispatchRaysIndex().y * DispatchRaysDimensions().x + DispatchRaysIndex().x, int(clockARB())));

    static const int max_samples = 1;
    static const int max_depth = 1;
    float3 wo = normalize(camera.eye_pos.xyz - position);
    var bxdf = pbr::DiffuseBxDF(albedo.xyz);
    var bsdf = pbr::BSDF<pbr::DiffuseBxDF>(bxdf, normal);

    for (int i = 0; i < lights[0].light.light_count; i++)
    {
        float2 u = float2(pbr.rand::rnd(seed), pbr.rand::rnd(seed));
        let l = lotus::TraceLight(lights[0].light_info, trace_origin, normal, u);
        diffuse += l.light * bsdf.f(wo, l.wi);
    }

    float3 accumulation = diffuse;
    RayDesc ray;
    ray.TMin = 0.0;
    ray.TMax = 1000.0;
    for (int samples = 0; samples < max_samples; ++samples)
    {
        float3 weight = float3(1.0);

        if (let new_sample = bsdf.Sample_f(wo, pbr.rand::rnd(payload.seed), float2(pbr.rand::rnd(payload.seed), pbr.rand::rnd(payload.seed))))
        {
            ray.Direction = new_sample.wi;
            ray.Origin = position.xyz;
            for(; payload.depth < max_depth; payload.depth++)
            {
                TraceRay(topLevelAS, 0, 0x01 | 0x02, 0, 0, 1, ray, payload);
                if (let payload_sample = payload.bsdf)
                {
                    accumulation += payload_sample.f * payload.diffuse * weight;
                    weight *= payload_sample.f * payload_sample.pdf_inv;
                    ray.Direction = payload_sample.wi;
                    ray.Origin = payload.origin;
                }
                else
                {
                    break;
                }
            }
        }
    }
    output[DispatchRaysIndex().xy] = (float4(accumulation, 0.0) / float(max_samples)) / M_PI;
    */
}

[shader("miss")]
void MissGI(inout lotus::RayPayload payload)
{
    lotus::MissGI(payload);
}

[shader("miss")]
void MissShadow(inout lotus::LightPayload payload)
{
    lotus::MissShadow(payload);
}
