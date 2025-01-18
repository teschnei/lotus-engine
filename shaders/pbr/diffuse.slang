implementing pbr;

__include bxdf;
__include sampling;

namespace pbr {

public struct DiffuseBxDF : IBxDF {
    Optional<BSDFSample> Sample_f(float3 wo, float uc, float2 u) {
        Sample sample = pbr::SampleCosineHemisphere(u);

        return BSDFSample(R / M_PI, sample.pdf_inv, sample.wi);
    }

    public __init(float3 _R) { R = _R; }

    float3 R;
}
} // namespace pbr
