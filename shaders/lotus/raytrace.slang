module raytrace;

import pbr;
import lotus;

[vk_binding(0,0)] public RaytracingAccelerationStructure topLevelAS;

[vk_binding(6,1)] public StructuredBuffer<lotus::Lights> lights;

[vk_binding(0,2)] public StructuredBuffer<lotus::Mesh> meshes;
[vk_binding(1,2)] public Sampler2D textures[];

namespace lotus
{
public struct RayPayload
{
    public pbr::SerializedBxDF bxdf = pbr::SerializedBxDF();
    public float3 normal = float3(0);
    public float3 face_normal = float3(0);
    public int depth = 0;
    public float3 intersection = float3(0);
    public float distance = 0;
    public float3 particle = float3(0);
    public float3 prev_pos = float3(0);
    public uint id = 0;
    public float3 albedo = float3(0);
};

public void MissGI(inout RayPayload payload)
{
    payload.bxdf = pbr::ConstantBxDF(lights[0].light.landscape.ambient_color.rgb * lights[0].light.landscape.brightness / M_PI, 1).Serialize();
    payload.distance = RayTCurrent();
}

public int GetMeshID()
{
    //TODO: technically not exactly correct, but kind of works for now
    return ((Material*)GetMeshInfo().material)->texture_index + GeometryIndex();
}

public Mesh GetMeshInfo()
{
    return meshes[InstanceID() + GeometryIndex()];
}

public struct LightPayload
{
    float3 light_potential = float3(0.0);
    float3 light_actual = float3(0.0);
}

public struct IncomingLight
{
    public float3 light;
    public float3 wi;
}

public IncomingLight TraceLight(LightInfo light, float3 origin, float3 normal, float2 u)
{
    float attenuation = 1.0;
    Optional<pbr::LightSample> sample;
    if(light.intensity == 0.0)
    {
        let pbr_light = pbr::DistantAreaLight(light.colour, light.pos, light.radius);
        let sampler = pbr::LightSampler<pbr::DistantAreaLight>(pbr_light);
        sample = sampler.SampleLi(origin, u);
    }
    if (let s = sample)
    {
        float3 diffuse = float3(0);
        attenuation *= abs(dot(s.wi, normal));
        if (attenuation > 0.001)
        {
            var light_ray = LightPayload();
            light_ray.light_potential = float3(s.L * attenuation);
            RayDesc ray;
            ray.Origin = origin;
            ray.TMin = 0.0;
            ray.Direction = s.wi;
            ray.TMax = s.len;
            TraceRay(topLevelAS, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0x01 | 0x02 | 0x10 , 1, 0, 2, ray, light_ray);
            diffuse = light_ray.light_actual;
            if (light.intensity > 0.0)
            {
                diffuse *= s.pdf_inv;
            }
        }
        return {diffuse, s.wi};
    }
    return {float3(0), float3(0)};
}

public void MissShadow(inout LightPayload payload)
{
    payload.light_actual = payload.light_potential;
}
}
