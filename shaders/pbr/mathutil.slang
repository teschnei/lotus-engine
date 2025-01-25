implementing pbr;

__include complex;
__include sampling;

namespace pbr {
bool SameHemisphere(float3 w, float3 wp) {
    return w.z * wp.z > 0;
}

float CosPhi(float3 w)
{
    float cos2theta = w.z * w.z;
    float sintheta = sqrt(1 - cos2theta);
    if (sintheta == 0)
        return 1;
    return clamp(w.x / sintheta, -1, 1);
}

float SinPhi(float3 w)
{
    float cos2theta = w.z * w.z;
    float sintheta = sqrt(1 - cos2theta);
    if (sintheta == 0)
        return 0;
    return clamp(w.y / sintheta, -1, 1);
}
float Tan2Theta(float3 w)
{
    float cos2theta = w.z * w.z;
    return max(0, 1 - cos2theta) / cos2theta;
}

float3 FrComplex(float cosTheta_i, float eta, float3 k)
{
    return float3(FrComplex(cosTheta_i, Complex<float>(eta, k.x)),
                  FrComplex(cosTheta_i, Complex<float>(eta, k.y)),
                  FrComplex(cosTheta_i, Complex<float>(eta, k.z)));
}

float FrComplex(float cosTheta_i, Complex<float> eta)
{
    cosTheta_i = clamp(cosTheta_i, 0, 1);
    float sin2Theta_i = 1 - (cosTheta_i * cosTheta_i);
    Complex<float> sin2Theta_t = sin2Theta_i / (eta * eta);
    Complex<float> cosTheta_t = sqrt(1 - sin2Theta_t);

    Complex<float> r_parl = (eta * cosTheta_i - cosTheta_t) / (eta * cosTheta_i + cosTheta_t);
    Complex<float> r_perp = (cosTheta_i - eta * cosTheta_t) / (cosTheta_i + eta * cosTheta_t);
    return (normalize(r_parl) + normalize(r_perp)) / 2;
}

float FrDielectric(float cosTheta_i, float eta)
{
    cosTheta_i = clamp(cosTheta_i, -1, 1);
    if (cosTheta_i < 0) {
        eta = 1 / eta;
        cosTheta_i = -cosTheta_i;
    }

    float sin2Theta_i = 1 - (cosTheta_i * cosTheta_i);
    float sin2Theta_t = sin2Theta_i / (eta * eta);
    if (sin2Theta_t >= 1)
        return 1.f;
    float cosTheta_t = sqrt(1 - sin2Theta_t);

    float r_parl = (eta * cosTheta_i - cosTheta_t) / (eta * cosTheta_i + cosTheta_t);
    float r_perp = (cosTheta_i - eta * cosTheta_t) / (cosTheta_i + eta * cosTheta_t);
    return ((r_parl * r_parl) + (r_perp * r_perp)) / 2;
}

struct TrowbridgeReitzDistribution
{
    bool Smooth() { return all(alpha < 1e-3f); }

    float3 Sample_wm(float3 w, float2 u)
    {
        float3 wh = normalize(float3(alpha.x * w.x, alpha.y * w.y, w.z));
        if (wh.z < 0)
            wh = -wh;

        float3 T1;
        if (wh.z < 1)
            T1 = normalize(cross(float3(0,0,1), wh));
        else
            T1 = float3(1,0,0);
        float3 T2 = cross(wh, T1);

        float2 p = SampleUniformDiskPolar(u);

        float h = sqrt(1 - (p.x * p.x));
        p.y = lerp(h, p.y, (1 + wh.z) / 2);

        float pz = sqrt(max(0, 1 - ((p.x * p.x) + (p.y * p.y))));
        float3 nh = p.x * T1 + p.y * T2 + pz * wh;
        return normalize(float3(alpha.x * nh.x, alpha.y * nh.y, max(1e-6, nh.z)));
    }

    float PDF(float3 w, float3 wm)
    {
        return G1(w) / abs(w.z) * D(wm) * abs(dot(w, wm));
    }

    float G1(float3 w)
    {
        float lambda = Lambda(w);
        return (1.f / (1.f + lambda));
    }

    float G(float3 wo, float3 wi)
    {
        return (1.f / (1.f + Lambda(wo) + Lambda(wi)));
    }

    float Lambda(float3 w)
    {
        float tan2theta = Tan2Theta(w);
        if (isinf(tan2theta)) return 1;
        float alpha_x = CosPhi(w) * alpha.x;
        float alpha_y = SinPhi(w) * alpha.y;
        float alpha2 = (alpha_x * alpha_x) + (alpha_y * alpha_y);
        return (sqrt(1 + alpha2 * tan2theta) - 1) / 2;
    }

    float D(float3 wm)
    {
        float tan2theta = Tan2Theta(wm);
        if (isinf(tan2theta)) return 0;
        float cos2theta = wm.z * wm.z;
        float cos4theta = cos2theta * cos2theta;
        float alpha_x = CosPhi(wm) / alpha.x;
        float alpha_y = SinPhi(wm) / alpha.y;
        float e1 = (tan2theta * ((alpha_x * alpha_x) + (alpha_y * alpha_y))) + 1;
        return 1.f / (M_PI * alpha.x * alpha.y * cos4theta * (e1 * e1));
    }

    [mutating]
    void Regularize()
    {
        if (alpha.x < 0.3)
        {
            alpha.x = clamp(2 * alpha.x, 0.1, 0.3);
        }
        if (alpha.y < 0.3)
        {
            alpha.y = clamp(2 * alpha.y, 0.1, 0.3);
        }
        /*
        if (any(alpha < 0.3))
        {
            alpha = clamp(2 * alpha, 0.1, 0.3);
        }*/
    }

    __init(float2 _alpha) { alpha = _alpha; }

    float2 alpha; // material roughness (in x and y direction)
}
}
