implementing pbr;

namespace pbr {

struct BSDFSample {
    float3 f;
    float pdf_inv;
    float3 wi;
};

interface IBxDF {
    Optional<BSDFSample> Sample_f(float3 wo);
};

} // namespace pbr
