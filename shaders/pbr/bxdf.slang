implementing pbr;

__include common;

namespace pbr
{

public struct BSDFSample
{
    public float3 f;
    public float pdf_inv;
    public float3 wi;
};

interface IBxDF
{
    Optional<BSDFSample> Sample_f(float3 wo, float uc, float2 u);
};

public struct BSDF<BxDF> where BxDF: IBxDF
{
    public Optional<BSDFSample> Sample_f(float3 woWorld, float uc, float2 u)
    {
        float3 wo = frame.WorldToLocal(woWorld);
        if (let bs = bxdf.Sample_f(wo, uc, u))
        {
            //TODO: check for zeroes, etc
            // optional value cant be mutable?
            BSDFSample copy;
            copy.f = bs.f;
            copy.pdf_inv = bs.pdf_inv;
            copy.wi = frame.LocalToWorld(bs.wi);
            return copy;
        }
        return none;
    }

    public __init(BxDF _bxdf, float3 normal) { bxdf = _bxdf; frame = Frame(normal); }

    BxDF bxdf;
    Frame frame;
};


} // namespace pbr
