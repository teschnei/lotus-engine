struct VertexWeight
{
    float3 pos;
    float3 norm;
    float weight;
    uint bone_index;
    uint mirror_axis;
    float2 uv;
};

struct Bone
{
    float4 rot;
    float3 trans;
    float3 scale;
};

struct Vertex
{
    float3 pos;
    float3 norm;
    float2 uv;
};

[vk_binding(0)] StructuredBuffer<VertexWeight> weights;
[vk_binding(1)] StructuredBuffer<Bone> bones;
[vk_binding(2)] RWStructuredBuffer<Vertex> vertices;

float3 mirror_vec(float3 pos, uint mirror_axis)
{
    float3 out_pos = pos;
    if (mirror_axis == 1)
        out_pos.x = -out_pos.x;
    if (mirror_axis == 2)
        out_pos.y = -out_pos.y;
    if (mirror_axis == 3)
        out_pos.z = -out_pos.z;
    return out_pos;
}

float3 rotate_trans(float4 quat_rot, float3 pos)
{
    float3 uv = cross(quat_rot.xyz, pos);
    float3 uuv = cross(quat_rot.xyz, uv);
    return pos + 2.0 * ((uv * quat_rot.w) + uuv);
}

[shader("compute")]
[numthreads(1,1,1)]
void Skin(uint3 threadId : SV_DispatchThreadID) {
    VertexWeight weight1 = weights[threadId.x*2];
    VertexWeight weight2 = weights[threadId.x*2+1];

    Bone bone1 = bones[weight1.bone_index];

    float3 pos = rotate_trans(bone1.rot, mirror_vec(weight1.pos * bone1.scale, weight1.mirror_axis)) + (bone1.trans * weight1.weight);
    float3 norm = rotate_trans(bone1.rot, mirror_vec(weight1.norm * bone1.scale, weight1.mirror_axis));

    if (weight2.weight > 0)
    {
        Bone bone2 = bones[weight2.bone_index];

        pos += rotate_trans(bone2.rot, mirror_vec(weight2.pos * bone2.scale, weight2.mirror_axis)) + (bone2.trans * weight2.weight);
        norm += rotate_trans(bone2.rot, mirror_vec(weight2.norm * bone2.scale, weight2.mirror_axis));
    }

    norm = normalize(norm);
    vertices[threadId.x].pos = pos;
    vertices[threadId.x].norm = norm;
    vertices[threadId.x].uv = weight1.uv;
}

