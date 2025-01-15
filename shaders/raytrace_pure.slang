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

    lotus::RayPayload payload;

    payload.normal = float3(1.0);
    payload.depth = 0;
    payload.seed = lotus::rand.tea(DispatchRaysIndex().y * DispatchRaysDimensions().x + DispatchRaysIndex().x, int(clockARB()));
    payload.weight = 0.0;
    payload.particle = float3(0.0);
    payload.distance = 0.0;
    payload.origin = float3(0);
    payload.prev_pos = float3(0);

    RayDesc ray;
    ray.Origin = origin.xyz;
    ray.TMin = 0.001;
    ray.Direction = dir.xyz;
    ray.TMax = 1000.0;

    TraceRay(topLevelAS, 0, 0x01 | 0x02 | 0x10 | 0x20, 0, 0, 0, ray, payload);
    float3 hit_normal = payload.normal;
    float3 diffuse = payload.diffuse;
    float3 BRDF = payload.BRDF;
    float3 gi_origin = payload.origin;
    float3 pos = payload.origin;
    float3 pos_prev = payload.prev_pos;
    float3 gi_dir = payload.direction;
    float pdf = payload.weight;
    //initial weight is 1.0 because the initial BRDF was pulled out into a separate buffer
    float3 gi_weight = float3(1.0);
    float3 particle_colour = payload.particle;
    float dist = payload.distance;
    static const int max_samples = 1;
    static const int max_depth = 2;
    float3 accumulation = float3(0.0);

    if (payload.depth < max_depth)
    {
        for (int samples = 0; samples < max_samples; ++samples)
        {
            float3 weight = gi_weight;
            payload.depth = 1;
            payload.origin = gi_origin;
            payload.direction = gi_dir;
            payload.distance = 0.0;
            for(; payload.depth < max_depth; payload.depth++)
            {
                ray.Origin = payload.origin;
                ray.Direction = payload.direction;
                TraceRay(topLevelAS, 0, 0x01 | 0x02 | 0x20, 0, 0, 0, ray, payload);
                weight *= payload.BRDF;
                accumulation += payload.diffuse * weight;
            }
        }
    }

    float3 light_total = (accumulation * pdf / float(max_samples)) + diffuse;

    float4 pos_proj = mul(camera[0].proj, mul(camera[0].view, float4(pos, 1.0)));
    float4 prev_pos_proj = mul(camera[1].proj, mul(camera[1].view, float4(pos_prev, 1.0)));

    float2 curScreenPos = (pos_proj.xy / pos_proj.w) * 0.5 + 0.5;
    float2 prevScreenPos = (prev_pos_proj.xy / prev_pos_proj.w) * 0.5 + 0.5;

    albedo[DispatchRaysIndex().xy] = float4(BRDF, dist);
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
void MissShadow(inout lotus::ShadowPayload payload)
{
    lotus::MissShadow(payload);
}
