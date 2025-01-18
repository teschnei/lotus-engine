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
    float3 pos = float3(0.0);
    float3 pos_prev = float3(0.0);
    float3 hit_normal = float3(0.0);
    float3 particle_colour = float3(0.0);
    float3 diffuse = float3(0.0);
    float distance = 0.0;

    var payload = lotus::RayPayload(lotus::rand.tea(DispatchRaysIndex().y * DispatchRaysDimensions().x + DispatchRaysIndex().x, int(clockARB())));

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.TMin = 0.001;
    ray.Direction = dir.xyz;
    ray.TMax = 1000.0;

    TraceRay(topLevelAS, 0, 0x01 | 0x02 | 0x10 | 0x20, 0, 0, 0, ray, payload);

    static const int max_samples = 1;
    static const int max_depth = 2;
    if (let bsdf = payload.bsdf)
    {
        f = bsdf.f;
        diffuse = payload.diffuse;
        hit_normal = payload.normal;
        pos = payload.origin;
        pos_prev = payload.prev_pos;
        particle_colour = payload.particle;
        distance = payload.distance;
        float3 initial_origin = payload.origin;

        if (payload.depth < max_depth)
        {
            for (int samples = 0; samples < max_samples; ++samples)
            {
                float3 weight = float(1.0); //accum will get multiplied by bsdf.f in the deferred shader
                payload.depth = 1;
                payload.distance = 0.0;
                ray.Direction = bsdf.wi;
                ray.Origin = initial_origin;
                for(; payload.depth < max_depth; payload.depth++)
                {
                    TraceRay(topLevelAS, 0, 0x01 | 0x02 | 0x20, 0, 0, 1, ray, payload);
                    if (let bsdf = payload.bsdf)
                    {
                        accumulation += bsdf.f * payload.diffuse * weight;
                        weight *= bsdf.f * bsdf.pdf_inv;
                        ray.Direction = bsdf.wi;
                        ray.Origin = payload.origin;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
    }

    float3 light_total = (accumulation / float(max_samples)) + diffuse;

    float4 pos_proj = mul(camera[0].proj, mul(camera[0].view, float4(pos, 1.0)));
    float4 prev_pos_proj = mul(camera[1].proj, mul(camera[1].view, float4(pos_prev, 1.0)));

    float2 curScreenPos = (pos_proj.xy / pos_proj.w) * 0.5 + 0.5;
    float2 prevScreenPos = (prev_pos_proj.xy / prev_pos_proj.w) * 0.5 + 0.5;

    albedo[DispatchRaysIndex().xy] = float4(f, distance);
    normal[DispatchRaysIndex().xy] = float4(hit_normal, 0.0);
    light[DispatchRaysIndex().xy] = float4(light_total, 1.0);
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
