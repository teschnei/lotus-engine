add_slang_module(lotus-slang-module
    SOURCE lotus.slang
    IMPLEMENTING
    lotus/billboard.slang
    lotus/common.slang
    SLANG_INCLUDE_DIRECTORIES
    .
)
add_slang_module(pbr-slang-module
    SOURCE pbr.slang
    IMPLEMENTING
    pbr/bsdf.slang
    pbr/common.slang
    pbr/complex.slang
    pbr/conductor_bxdf.slang
    pbr/dielectric_bxdf.slang
    pbr/diffuse_bxdf.slang
    pbr/layered_bxdf.slang
    pbr/lights.slang
    pbr/mathutil.slang
    pbr/random.slang
    pbr/sampling.slang
)
add_slang_module(lotus-rt-slang-module
    SOURCE lotus/raytrace.slang
    SLANG_INCLUDE_DIRECTORIES
    .
)

add_slang_shader(animation_skin SOURCE animation_skin.slang)
add_slang_shader(post_process SOURCE post_process.slang)
add_slang_shader(rayquery SOURCE rayquery.slang)
add_slang_shader(ui SOURCE ui.slang)
add_slang_shader(deferred_raytrace SOURCE deferred_raytrace.slang MODULES lotus-slang-module)
add_slang_shader(deferred_hybrid SOURCE deferred_hybrid.slang MODULES lotus-slang-module)
add_slang_shader(raytrace_pure SOURCE raytrace_pure.slang MODULES lotus-slang-module lotus-rt-slang-module)
add_slang_shader(raytrace_hybrid SOURCE raytrace_hybrid.slang MODULES lotus-slang-module lotus-rt-slang-module)

add_dependencies(lotus-slang-module pbr-slang-module)
add_dependencies(lotus-rt-slang-module lotus-slang-module pbr-slang-module)
add_dependencies(lotus-engine lotus-slang-module animation_skin post_process rayquery ui deferred_raytrace deferred_hybrid raytrace_pure raytrace_hybrid)
