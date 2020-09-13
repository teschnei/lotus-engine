find_program(GLSL_VALIDATOR_EXE glslangValidator)

function(compile_spirv INPUT OUTPUT)
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND "${GLSL_VALIDATOR_EXE}" --target-env vulkan1.2 -V "${INPUT}" -o "${OUTPUT}"
        MAIN_DEPENDENCY ${INPUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endfunction()

set(SHADERS 
    animation_skin.comp
    blend.frag
    closesthit.rchit
    color_hit.rahit
    deferred_raster.frag
    deferred_raytrace.frag
    deferred_raytrace_hybrid.frag
    deferred.vert
    gbuffer_frag.frag
    gbuffer_vert.vert
    landscape_closest_hit.rchit
    landscape_color_hit.rahit
    landscape_gbuffer_vert.vert
    landscape_shadow_vert.vert
    miss.rmiss
    particle_blend.frag
    particle_closest_hit.rchit
    particle_color_hit.rahit
    particle_gbuffer_vert.vert
    particle_intersection.rint
    particle_shadow_color_hit.rahit
    particle_shadow_vert.vert
    quad.vert
    query_closest_hit.rchit
    query_miss.rmiss
    raygen_hybrid.rgen
    raygen.rgen
    rayquery.rgen
    shadow_frag.frag
    shadow_miss.rmiss
    shadow_vert.vert
    ui_frag.frag
    ui_vert.vert
)

add_custom_target(shaders-dir ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)

add_custom_target(shaders ALL
    )

foreach(SHADER ${SHADERS})
    get_filename_component(OUTPUT_FILE ${SHADER} NAME_WLE)
    set(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders/${OUTPUT_FILE}.spv)
    compile_spirv(${SHADER} ${OUTPUT})
    add_custom_target(${OUTPUT_FILE} DEPENDS shaders-dir ${OUTPUT})
    add_dependencies(shaders ${OUTPUT_FILE})
endforeach()

