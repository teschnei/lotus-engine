implementing pbr;

struct Frame
{
    float3 WorldToLocal(float3 wWorld)
    {
        return float3(dot(wWorld, Nt), dot(wWorld, Nb), dot(wWorld, N));
    }

    float3 LocalToWorld(float3 wLocal)
    {
        return wLocal.x * Nt + wLocal.y * Nb + wLocal.z * N;
    }

    __init(float3 _N)
    {
        N = _N;
        if(abs(N.x) > abs(N.y))
            Nt = float3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
        else
            Nt = float3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
        Nb = cross(N, Nt);
    }

    float3 N;
    float3 Nt;
    float3 Nb;
};

