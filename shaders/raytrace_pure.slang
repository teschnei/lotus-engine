import lotus;
import lotus.raytrace;
import pbr;

[vk_binding(0,1)] public RWTexture2D albedo;
[vk_binding(1,1)] public RWTexture2D normal;
[vk_binding(2,1)] public RWTexture2D light;
[vk_binding(3,1)] public RWTexture2D particle;
[vk_binding(4,1)] public RWTexture2D motion_vector;
[vk_binding(5,1)] public ConstantBuffer<lotus::Camera> camera[2];

[shader("raygeneration")]
void Raygen()
{
    let center = float2(DispatchRaysIndex().xy) + float2(0.5);
    let in_uv = center / float2(DispatchRaysDimensions().xy);
    float2 d = in_uv * 2.0 - 1.0;

    float4 origin = mul(camera[0].view_inverse, float4(0, 0, 0, 1));
    float4 target = mul(camera[0].proj_inverse, float4(d.x, d.y, 1, 1));
    float4 dir = mul(camera[0].view_inverse, float4(normalize(target.xyz), 0));

    float3 accumulation = float3(0.0);
    float3 f = float3(0.0);

    uint seed = pbr::rand.tea(DispatchRaysIndex().y * DispatchRaysDimensions().x + DispatchRaysIndex().x, int(clockARB()));
    var payload = lotus::RayPayload();

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.TMin = 0.001;
    ray.Direction = dir.xyz;
    ray.TMax = 1000.0;

    float3 hit_normal = float3(0);
    float3 pos = float3(0);
    float3 pos_prev = float3(0);
    float3 particle_colour = float3(0);
    float distance = 0;
    bool blend_f = false;
    bool anyNonSpecular = false;
    uint id = 0;

    static const int max_samples = 1;
    static const int max_depth = 2;
    int real_samples = 0;

    for (int samples = 0; samples < max_samples; ++samples)
    {
        payload.distance = 0.0;
        ray.Origin = origin.xyz;
        ray.Direction = dir.xyz;
        float3 weight = 1.0;
        for(payload.depth = 0; payload.depth < max_depth; payload.depth++)
        {
            if (payload.depth == 0)
            {
                TraceRay(topLevelAS, 0, 0x01 | 0x02 | 0x10 | 0x20, 0, 0, 0, ray, payload);
                hit_normal = payload.normal;
                pos = payload.intersection;
                pos_prev = payload.prev_pos;
                particle_colour = payload.particle;
                distance = payload.distance;
                id = payload.id;
            }
            else
            {
                TraceRay(topLevelAS, 0, 0x01 | 0x02 | 0x20, 0, 0, 1, ray, payload);
            }
            var bsdf = pbr::BSDF(payload.bxdf, payload.normal);
            if (anyNonSpecular)
            {
                bsdf.Regularize();
            }
            if (let sample = bsdf.Sample_f(-ray.Direction, pbr.rand::rnd(seed), float2(pbr.rand::rnd(seed), pbr.rand::rnd(seed))))
            {
                if (payload.depth == 0)
                {
                    blend_f = blend_f || (sample.flags & pbr::BSDFFlags.Glossy);
                    f += sample.f;
                    real_samples++;
                }
                if (payload.distance < ray.TMax)
                {
                    float3 new_origin = payload.intersection + (payload.face_normal * 0.001 * (sample.flags & pbr::BSDFFlags.Transmission ? -1 : 1));
                    for (int i = 0; i < lights[0].light.light_count; i++)
                    {
                        float2 u = float2(pbr.rand::rnd(seed), pbr.rand::rnd(seed));
                        let l = lotus::TraceLight(lights[0].light_info, new_origin, payload.normal, u);
                        accumulation += l.light * bsdf.f(-ray.Direction, l.wi) * weight;
                    }
                    weight *= sample.f * sample.pdf_inv * abs(dot(sample.wi, payload.normal));
                    ray.Direction = sample.wi;
                    ray.Origin = new_origin;
                    anyNonSpecular |= ((sample.flags & pbr::BSDFFlags.Specular) == 0);
                }
                else
                {
                    accumulation += weight * sample.f * sample.pdf_inv;
                    payload.depth = 10;
                }
            }
            else
            {
                payload.depth = 10;
            }
        }
    }

    float3 light_total = (accumulation / float3(real_samples));
    f /= float3(real_samples);
    light_total = clamp(light_total, float3(0), float3(10));

    if (!blend_f && all(f > 0))
    {
        light_total /= f;
    }

    float4 pos_proj = mul(camera[0].proj, mul(camera[0].view, float4(pos, 1.0)));
    float4 prev_pos_proj = mul(camera[1].proj, mul(camera[1].view, float4(pos_prev, 1.0)));

    float2 curScreenPos = (pos_proj.xy / pos_proj.w) * 0.5 + 0.5;
    float2 prevScreenPos = (prev_pos_proj.xy / prev_pos_proj.w) * 0.5 + 0.5;

    albedo[DispatchRaysIndex().xy] = blend_f ? float4(1.0, 1.0, 1.0, distance) : float4(f, distance);
    normal[DispatchRaysIndex().xy] = float4(hit_normal, blend_f ? 1.0 : 0.0);
    light[DispatchRaysIndex().xy] = float4(light_total, id);
    particle[DispatchRaysIndex().xy] = float4(particle_colour, 1.0);
    motion_vector[DispatchRaysIndex().xy] = float4(float2(curScreenPos - prevScreenPos), pos_proj.z, prev_pos_proj.z);
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
