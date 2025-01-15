implementing pbr;

__include bxdf;

namespace pbr {

struct DiffuseBxDF : IBxDF {
    Optional<BSDFSample> Sample_f(float3 wo) {
        
    }
}
} // namespace pbr
