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
    public float3 BRDF;
    public float3 diffuse;
    public float3 normal;
    public int depth;
    public uint seed;
    public float weight;
    public float3 origin;
    public float3 direction;
    public float distance;
    public float3 particle;
    public float3 prev_pos;
};

public void MissGI(inout RayPayload payload)
{
    payload.BRDF = float3(1.0) / M_PI;
    payload.diffuse = lights[0].light.landscape.ambient_color.rgb * lights[0].light.landscape.brightness / 2;
    payload.depth = 10;
}

public Mesh GetMeshInfo()
{
    return meshes[InstanceID() + GeometryIndex()];
}

public struct ShadowPayload
{
    float4 light;
    float4 shadow;
}

public float3 TraceLight(LightInfo light, float3 origin, float3 normal, float2 u)
{
    float3 diff = light.pos - origin;
    float3 dir = normalize(diff);
    float length = length(diff);
    float radius = light.radius;
    float3 diffuse = float3(0);

    if (length > radius)
    {
        float att = 1;
        float intensity = light.intensity;
        int use_pdf = 1;
        if (intensity > 0)
        {
            att = max(1, radius * radius) / (length * length);
        }
        else
        {
            intensity = 1;
            use_pdf = 0;
        }
        ShadowPayload shadow;
        shadow.shadow = float4(0.0);
        shadow.light = float4(light.colour * intensity, 1.0);
        float3 trace_dir = dir;
        float pdf = 1;

        if (radius > 0)
        {
            float3 tangent, bitangent;
                pbr::createCoordinateSystem(dir, tangent, bitangent);

            float temp_pdf;
            trace_dir = pbr::SampleCone(u, temp_pdf, tangent, bitangent, dir, radius, dot(diff, diff));
            if (use_pdf > 0)
                pdf = temp_pdf;
        }

        att *= dot(trace_dir, normal);
        if (att > 0.001)
        {
            RayDesc ray;
            ray.Origin = origin;
            ray.TMin = 0.0;
            ray.Direction = trace_dir;
            ray.TMax = length;
            TraceRay(topLevelAS, RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, 0x01 | 0x02 | 0x10 , 1, 0, 2, ray, shadow);
            diffuse = att * shadow.shadow.rgb / pdf;
        }
    }
    return diffuse;
}

public void MissShadow(inout ShadowPayload payload)
{
    payload.shadow = payload.light;
    payload.shadow.a = 1.0;
}
}
