implementing pbr;

__include bsdf;
__include sampling;
__include mathutil;
__include random;

__include dielectric_bxdf;
__include diffuse_bxdf;

namespace pbr {

// Simple LayeredBxDF
// Assumes the Bottom layer is opaque (and probably diffuse),
// and that the incoming light is always hitting Top first
public struct LayeredBxDF<TopBxDF:IBxDF, BottomBxDF:IBxDF> : IBxDF
{
    Optional<BSDFSample> Sample_f(float3 wo, float uc, float2 u, BSDFSampleFlags flags)
    {
        if (let topbsdf = topbxdf.Sample_f(wo, uc, u, BSDFSampleFlags.Transmission | BSDFSampleFlags.Reflection))
        {
            if (topbsdf.flags & BSDFFlags.Transmission)
            {
                //TODO: should use topbsdf.wi? or wo?
                if (let bottombsdf = bottombxdf.Sample_f(wo, uc, u, flags))
                {
                    // Should interact with topbsdf?
                    return BSDFSample(bottombsdf.f, bottombsdf.pdf_inv, bottombsdf.wi, bottombsdf.flags | (topbsdf.flags & (BSDFFlags.Glossy | BSDFFlags.Specular | BSDFFlags.Diffuse)));
                }
                else
                {
                    return none;
                }
            }
            else
            {
                return topbsdf;
            }
        }
        return none;
    }

    float3 f(float3 wo, float3 wi)
    {
        //TODO: figure out actual formula
        return max(topbxdf.f(wo, wi), bottombxdf.f(wo, wi));
    }

    [mutating]
    void Regularize()
    {
        topbxdf.Regularize();
        bottombxdf.Regularize();
    }

    public SerializedBxDF Serialize()
    {
        var top_serialized = topbxdf.Serialize();
        var bottom_serialized = bottombxdf.Serialize();
        SerializedBxDF serialized_bxdf;
        serialized_bxdf.type.x = (uint)BxDFType.Layered;
        serialized_bxdf.type.y = top_serialized.type.x;
        serialized_bxdf.type.z = bottom_serialized.type.x;
        serialized_bxdf.data1 = top_serialized.data1;
        serialized_bxdf.data2 = top_serialized.data2;
        serialized_bxdf.data3 = bottom_serialized.data1;
        serialized_bxdf.data4 = bottom_serialized.data2;
        return serialized_bxdf;
    }

    public __init(TopBxDF _topbxdf, BottomBxDF _bottombxdf)
    {
        topbxdf = _topbxdf;
        bottombxdf = _bottombxdf;
    }

    TopBxDF topbxdf;
    BottomBxDF bottombxdf;
}

public typedef LayeredBxDF<DielectricBxDF, DiffuseBxDF> DielectricDiffuseBxDF;

} // namespace pbr
