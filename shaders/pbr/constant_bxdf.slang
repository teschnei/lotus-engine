implementing pbr;

__include bsdf;

namespace pbr {

public struct ConstantBxDF : IBxDF
{
    Optional<BSDFSample> Sample_f(float3 wo, float uc, float2 u, BSDFSampleFlags)
    {
        return BSDFSample(const_f, pdf_inv, float3(0), BSDFFlags.None);
    }

    float3 f(float3 wo, float3 wi)
    {
        return const_f;
    }

    void Regularize() {}

    public SerializedBxDF Serialize()
    {
        SerializedBxDF serialized_bxdf;
        serialized_bxdf.type.x = (uint)BxDFType.Constant;
        serialized_bxdf.data1.xyz = const_f;
        serialized_bxdf.data1.w = pdf_inv;
        return serialized_bxdf;
    }

    public __init(float3 _f, float _pdf_inv)
    {
        const_f = _f;
        pdf_inv = _pdf_inv;
    }

    float3 const_f;
    float pdf_inv;
}
} // namespace pbr
