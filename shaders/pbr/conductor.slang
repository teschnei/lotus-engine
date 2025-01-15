implementing pbr;

__include bxdf;

namespace pbr {

struct ConductorBxDF : IBxDF {
    Optional<BSDFSample> Sample_f(float3 wo) {
        
    }
}
} // namespace pbr
