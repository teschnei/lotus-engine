struct QueryInput
{
    float3 origin;
    float min;
    float3 direction;
    float max;
    int flags;
};

struct QueryOutput
{
    float intersection_dist;
};

[vk_binding(0)] RaytracingAccelerationStructure topLevelAS;
[vk_binding(1)] ConstantBuffer<QueryInput[512]> query_input;
[vk_binding(2)] RWStructuredBuffer<QueryOutput> query_output;

struct RayPayload
{
    float intersection_dist;
};

[shader("raygeneration")]
void Query()
{
    RayDesc ray;
    ray.Origin = query_input[DispatchRaysIndex().x].origin;
    ray.TMin = query_input[DispatchRaysIndex().x].min;
    ray.Direction = query_input[DispatchRaysIndex().x].direction;
    ray.TMax = query_input[DispatchRaysIndex().x].max;
    int flags = query_input[DispatchRaysIndex().x].flags;
    RayPayload payload;

    TraceRay(topLevelAS, 0, flags, 0, 0, 0, ray, payload);

    query_output[DispatchRaysIndex().x].intersection_dist = payload.intersection_dist;
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.intersection_dist = RayTCurrent();
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload)
{
    payload.intersection_dist = RayTCurrent();
}
