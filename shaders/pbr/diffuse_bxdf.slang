implementing pbr;

__include bsdf;
__include sampling;

namespace pbr {

public struct DiffuseBxDF : IBxDF
{
    Optional<BSDFSample> Sample_f(float3 wo, float uc, float2 u, BSDFSampleFlags)
    {
        Sample sample = pbr::SampleCosineHemisphere(u);
        return BSDFSample(R / M_PI, sample.pdf_inv, sample.wi, BSDFFlags.Diffuse | BSDFFlags.Reflection);
    }

    float3 f(float3 wo, float3 wi)
    {
        // Technically this should be here, but sometimes normals that are different than the face normal
        // will show artifacts at very low angles with this
        /*
        if (!SameHemisphere(wo, wi))
            return float3(0);
        */
        return R / M_PI;
    }

    void Regularize() {}

    public SerializedBxDF Serialize()
    {
        SerializedBxDF serialized_bxdf;
        serialized_bxdf.type.x = (uint)BxDFType.Diffuse;
        serialized_bxdf.data1.xyz = R;
        return serialized_bxdf;
    }

    public __init(float3 _R) { R = _R; }

    float3 R;
}
} // namespace pbr
