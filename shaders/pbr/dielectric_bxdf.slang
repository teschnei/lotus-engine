implementing pbr;

__include bsdf;
__include sampling;
__include mathutil;

namespace pbr {

public struct DielectricBxDF : IBxDF
{
    public Optional<BSDFSample> Sample_f(float3 wo, float uc, float2 u, BSDFSampleFlags flags)
    {
        if (eta == 1 || microfacet.Smooth()) {
            float R = FrDielectric(wo.z, eta);
            float T = 1 - R;
            float pr = R, pt = T;
            if ((flags & BSDFSampleFlags.Reflection) == 0)
                pr = 0;
            if ((flags & BSDFSampleFlags.Transmission) == 0)
                pt = 0;
            if (pr == 0 && pt == 0)
                return none;

            if (uc < pr / (pr + pt)) {
                float3 wi = float3(-wo.x, -wo.y, wo.z);
                float3 fr = float3(R / abs(wi.z));
                return BSDFSample(fr, (pr + pt) / pr, wi, BSDFFlags.Specular | BSDFFlags.Reflection);
            } else {
                float etap;
                float3 wi = refract(-wo, float3(0, 0, 1), 1/eta);
                if (all(wi == float3(0)))
                    return none;

                float3 ft = float3(T / abs(wi.z));

                return BSDFSample(ft, (pr + pt) / pt, wi, BSDFFlags.Specular | BSDFFlags.Transmission);
            }
        } else {
            // Sample rough dielectric BSDF
            float3 wm = microfacet.Sample_wm(wo, u);
            float R = FrDielectric(dot(wo, wm), eta);
            float T = 1 - R;
            float pr = R, pt = T;
            if ((flags & BSDFSampleFlags.Reflection) == 0)
                pr = 0;
            if ((flags & BSDFSampleFlags.Transmission) == 0)
                pt = 0;
            if (pr == 0 && pt == 0)
                return none;

            float pdf_inv;
            if (uc < pr / (pr + pt)) {
                float3 wi = reflect(-wo, wm);
                if (!SameHemisphere(wo, wi))
                    return none;
                pdf_inv = (4 * abs(dot(wo, wm))) / microfacet.PDF(wo, wm) * (pr + pt) / pr;

                float3 f = float3(microfacet.D(wm) * microfacet.G(wo, wi) * R /
                                (4 * wi.z * wo.z));
                return BSDFSample(f, pdf_inv, wi, BSDFFlags.Glossy | BSDFFlags.Reflection);

            } else {
                //TODO: deal with inverted eta
                float etap = eta;
                float3 wi = refract(-wo, wm, 1/eta);
                if (SameHemisphere(wo, wi) || wi.z == 0 || all(wi == float3(0)))
                    return none;
                float denom = (dot(wi, wm) + dot(wo, wm) / etap);
                denom *= denom;
                float dwm_dwi = abs(dot(wi, wm)) / denom;
                pdf_inv = (pr + pt) / (pt * microfacet.PDF(wo, wm) * dwm_dwi);

                float3 ft = float3(T * microfacet.D(wm) * microfacet.G(wo, wi) *
                                   abs(dot(wi, wm) * dot(wo, wm) /
                                   (wi.z * wo.z * denom)));

                return BSDFSample(ft, pdf_inv, wi, BSDFFlags.Glossy | BSDFFlags.Transmission);
            }
        }
    }

    public float3 f(float3 wo, float3 wi)
    {
        if (eta == 1 || microfacet.Smooth())
            return float3(0.f);
        float cosTheta_o = (wo.z);
        float cosTheta_i = (wi.z);
        bool reflect = cosTheta_i * cosTheta_o > 0;
        float etap = 1;
        if (!reflect)
            etap = cosTheta_o > 0 ? eta : (1 / eta);
        float3 wm = wi * etap + wo;
        if (cosTheta_i == 0 || cosTheta_o == 0 || dot(wm, wm) == 0)
            return {};
        wm = normalize(wm);
        wm = dot(wm, float3(0, 0, 1)) < 0 ? -wm : wm;

        if (dot(wm, wi) * cosTheta_i < 0 || dot(wm, wo) * cosTheta_o < 0)
            return {};

        float F = FrDielectric(dot(wo, wm), eta);
        if (reflect) {
            return float3(microfacet.D(wm) * microfacet.G(wo, wi) * F / abs(4 * cosTheta_i * cosTheta_o));

        } else {
            float denom = dot(wi, wm) + dot(wo, wm) / etap;
            denom *= denom * cosTheta_i * cosTheta_o;
            float ft = microfacet.D(wm) * (1 - F) * microfacet.G(wo, wi) *
                    abs(dot(wi, wm) * dot(wo, wm) / denom);

            return float3(ft);
        }
    }

    [mutating]
    void Regularize()
    {
        microfacet.Regularize();
    }

    public SerializedBxDF Serialize()
    {
        SerializedBxDF serialized_bxdf;
        serialized_bxdf.type.x = (uint)BxDFType.Dielectric;
        serialized_bxdf.data1.x = eta;
        serialized_bxdf.data1.yz = microfacet.alpha;
        return serialized_bxdf;
    }

    public __init(float _eta, float2 _alpha)
    {
        eta = _eta;
        microfacet = TrowbridgeReitzDistribution(_alpha);
    }

    float eta; // aka IOR
    TrowbridgeReitzDistribution microfacet;
}
} // namespace pbr
