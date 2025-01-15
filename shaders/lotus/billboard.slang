implementing lotus;

namespace lotus
{
public float4x4 LookAt(float3 eye, float3 center, float3 up)
{
    float3 f = float3(normalize(center - eye));
    float3 s = float3(normalize(cross(f, up)));
    float3 u = float3(cross(s, f));

    float4x4 result = float4x4(1);
    result[0][0] = s.x;
    result[1][0] = s.y;
    result[2][0] = s.z;
    result[0][1] = u.x;
    result[1][1] = u.y;
    result[2][1] = u.z;
    result[0][2] =-f.x;
    result[1][2] =-f.y;
    result[2][2] =-f.z;
    result[3][0] =-dot(s, eye);
    result[3][1] =-dot(u, eye);
    result[3][2] = dot(f, eye);

    return result;
}
}
