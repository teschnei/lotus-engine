implementing pbr;

static const float M_PI = 3.14159265f;

namespace pbr
{
float2 SampleUniformDiskPolar(float2 u) {
    float r = sqrt(u.x);
    float theta = 2 * M_PI * u.y;
    return float2(r * cos(theta), r * sin(theta));
}

public struct Sample
{
    public float3 wi;
    public float pdf_inv;
}

public Sample SampleCosineHemisphere(float2 u)
{
    float2 p = SampleUniformDiskPolar(u);
    float z_sqr = 1.0 - p.x - p.y;
    float z = z_sqr > 0 ? sqrt(z_sqr) : 1;

    return Sample(float3(p, z), M_PI / z);
}
// Randomly sampling in direction of +Z
public Sample SampleCone(float2 u, float radius, float length_sq)
{
    float cos_a_max = sqrt(1 - radius * radius / length_sq);
    float cos_a = 1.f - u.x + (u.x * cos_a_max);
    float sin_a = sqrt(1.f - cos_a * cos_a);
    float phi = 2 * M_PI * u.y;
    float3 direction = float3(cos(phi) * sin_a, sin(phi) * sin_a, cos_a);
    float pdf_inv = (2 * M_PI * (1.f - cos_a_max));
    return Sample(normalize(direction), pdf_inv);
}
} // namespace pbr
