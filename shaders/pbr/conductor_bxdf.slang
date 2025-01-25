implementing pbr;

__include bsdf;
__include mathutil;

namespace pbr {

public struct ConductorBxDF : IBxDF
{
    Optional<BSDFSample> Sample_f(float3 wo, float uc, float2 u, BSDFSampleFlags)
    {
        if(microfacet.Smooth())
        {
            float3 wi = float3(-wo.x, -wo.y, wo.z);
            float3 f = FrComplex(abs(wi.z), eta, k) / abs(wi.z);
            return BSDFSample(f, 1, wi, BSDFFlags.Specular | BSDFFlags.Reflection);
        }
        if (wo.z == 0)
            return none;
        float3 wm = microfacet.Sample_wm(wo, u);
        float3 wi = reflect(-wo, wm);
        float attenuation = 1;
        if (!SameHemisphere(wo, wi))
            attenuation = 0.4;

        float pdf = microfacet.PDF(wo, wm) / (4 * abs(dot(wo, wm)));

        float cosTheta_o = abs(wo.z);
        float cosTheta_i = abs(wi.z);
        if (cosTheta_o == 0 || cosTheta_i == 0)
            return none;

        float3 F = FrComplex(abs(dot(wo, wm)), eta, k);
        float3 f = microfacet.D(wm) * F * microfacet.G(wo, wi) / (4 * cosTheta_i * cosTheta_o);
        return BSDFSample(f, pdf, wi, BSDFFlags.Glossy | BSDFFlags.Reflection);
    }

    float3 f(float3 wo, float3 wi)
    {
        if (!SameHemisphere(wo, wi))
            return float3(0);
        if (microfacet.Smooth())
            return float3(0);

        float cosTheta_o = abs(wo.z);
        float cosTheta_i = abs(wi.z);
        if (cosTheta_o == 0 || cosTheta_i == 0)
            return float3(0);
        float3 wm = wi + wo;
        if (dot(wm, wm) == 0)
            return float3(0);
        wm = normalize(wm);

        // Evaluate Fresnel factor _F_ for conductor BRDF
        float3 F = FrComplex(abs(dot(wo, wm)), eta, k);

        return microfacet.D(wm) * F * microfacet.G(wo, wi) / (4 * cosTheta_i * cosTheta_o);
    }

    [mutating]
    void Regularize()
    {
        microfacet.Regularize();
    }

    public SerializedBxDF Serialize()
    {
        SerializedBxDF serialized_bxdf;
        serialized_bxdf.type.x = (uint)BxDFType.Conductor;
        serialized_bxdf.data1.x = eta;
        serialized_bxdf.data1.yzw = k;
        serialized_bxdf.data2.xy = microfacet.alpha;
        return serialized_bxdf;
    }

    public __init(float _eta, float3 _k, float2 _alpha)
    {
        eta = _eta;
        k = _k;
        microfacet = TrowbridgeReitzDistribution(_alpha);
    }

    float eta; // aka IOR
    float3 k; // texture colour (absorption colours)
    TrowbridgeReitzDistribution microfacet;
}

} // namespace pbr
