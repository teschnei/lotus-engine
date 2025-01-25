implementing pbr;

__include common;
__include diffuse_bxdf;

namespace pbr
{

[Flags]
public enum BSDFFlags
{
    None = 0,
    Diffuse,
    Specular,
    Glossy,
    Reflection,
    Transmission
};

[Flags]
public enum BSDFSampleFlags
{
    Reflection,
    Transmission
}

public struct BSDFSample
{
    public float3 f;
    public float pdf_inv;
    public float3 wi;
    public BSDFFlags flags;
};

enum BxDFType
{
    Constant,
    Conductor,
    Diffuse,
    Dielectric,
    Layered
};

public struct SerializedBxDF
{
    uint3 type = uint3(0);
    float4 data1 = float4(0);
    float2 data2 = float2(0);
    float4 data3 = float4(0);
    float2 data4 = float2(0);
}

interface IBxDF
{
    Optional<BSDFSample> Sample_f(float3 wo, float uc, float2 u, BSDFSampleFlags flags);
    float3 f(float3 wo, float3 wi);
    [mutating]
    void Regularize();
    SerializedBxDF Serialize();
};

public struct BSDF
{
    public Optional<BSDFSample> Sample_f(float3 woWorld, float uc, float2 u, BSDFSampleFlags flags = BSDFSampleFlags.Transmission | BSDFSampleFlags.Reflection)
    {
        float3 wo = frame.WorldToLocal(woWorld);
        if (let sample = bxdf.Sample_f(wo, uc, u, flags))
        {
            return BSDFSample(sample.f, sample.pdf_inv, frame.LocalToWorld(sample.wi), sample.flags);
        }
        return none;
    }

    public float3 f(float3 woWorld, float3 wiWorld)
    {
        float3 wo = frame.WorldToLocal(woWorld);
        float3 wi = frame.WorldToLocal(wiWorld);
        if (wo.z == 0) return float3(0);
        return bxdf.f(wo, wi);
    }

    [mutating]
    public void Regularize()
    {
        bxdf.Regularize();
    }

    static IBxDF Deserialize(SerializedBxDF serialized_bxdf)
    {
        if (serialized_bxdf.type.x == BxDFType.Layered)
        {

            if (serialized_bxdf.type.y == BxDFType.Dielectric && serialized_bxdf.type.z == BxDFType.Diffuse)
            {
                DielectricBxDF top = DielectricBxDF(serialized_bxdf.data1.x, serialized_bxdf.data1.yz);
                DiffuseBxDF bottom = DiffuseBxDF(serialized_bxdf.data3.xyz);
                return DielectricDiffuseBxDF(top, bottom);
            }
            return ConstantBxDF(0, 0);
        }
        else
        {
            return DeserializeHelper((BxDFType)serialized_bxdf.type.x, serialized_bxdf.data1, serialized_bxdf.data2);
        }
    }

    static IBxDF DeserializeHelper(BxDFType type, float4 data1, float2 data2)
    {
        switch(type)
        {
            case BxDFType.Constant:
                return ConstantBxDF(data1.xyz, data1.w);
            case BxDFType.Conductor:
                return ConductorBxDF(data1.x, data1.yzw, data2.xy);
            case BxDFType.Diffuse:
                return DiffuseBxDF(data1.xyz);
            case BxDFType.Dielectric:
                return DielectricBxDF(data1.x, data1.yz);
            default:
                return ConstantBxDF(0, 0);
        }
    }

    public __init(SerializedBxDF serialized_bxdf, float3 normal)
    {
        bxdf = Deserialize(serialized_bxdf);
        frame = Frame(normal);
    }
    IBxDF bxdf;
    Frame frame;
}

} // namespace pbr
