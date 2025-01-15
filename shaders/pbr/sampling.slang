implementing pbr;

static const float M_PI = 3.14159265f;

namespace pbr
{
float2 SampleUniformDiskPolar(float2 u) {
    float r = sqrt(u.x);
    float theta = 2 * M_PI * u.y;
    return float2(r * cos(theta), r * sin(theta));
}

public float3 SampleCosineHemisphere(float2 u)
{
    float2 p = SampleUniformDiskPolar(u);
    float z_sqr = 1.0 - p.x - p.y;
    float z = z_sqr > 0 ? sqrt(z_sqr) : 1;

    return float3(p, z);
}
// Randomly sampling in direction of +Z
public float3 SampleCone(float2 u, inout float pdf, float3 x, float3 y, float3 z, float radius, float length_sq)
{
    float cos_a_max = sqrt(1 - radius * radius / length_sq);
    float cos_a = 1.f - u.x + (u.x * cos_a_max);
    float sin_a = sqrt(1.f - cos_a * cos_a);
    float phi = 2 * M_PI * u.y;
    float3 direction = x * cos(phi) * sin_a + y * sin(phi) * sin_a + z * cos_a;
    pdf = 1.f / (2 * M_PI * (1.f - cos_a_max));
    return normalize(direction);
}

// Return the tangent and binormal from the incoming normal
public void createCoordinateSystem(in float3 N, out float3 Nt, out float3 Nb)
{
  if(abs(N.x) > abs(N.y))
    Nt = float3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
  else
    Nt = float3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
  Nb = cross(N, Nt);
}
} // namespace pbr
