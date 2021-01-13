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
    d3m_gbuffer_vert.vert
    deferred.vert
    deferred_raster.frag
    deferred_raytrace.frag
    deferred_raytrace_hybrid.frag
    ffxiactor_gbuffer_vert.vert
    ffxiactor_shadow_vert.vert
    gbuffer_frag.frag
    miss.rmiss
    mmb_closest_hit.rchit
    mmb_color_hit.rahit
    mmb_gbuffer_vert.vert
    mmb_shadow_vert.vert
    particle_blend.frag
    particle_closest_hit.rchit
    particle_color_hit.rahit
    particle_intersection.rint
    particle_shadow_color_hit.rahit
    particle_shadow_vert.vert
    post_process.comp
    quad.vert
    query_closest_hit.rchit
    query_miss.rmiss
    raygen_hybrid.rgen
    raygen.rgen
    rayquery.rgen
    shadow_frag.frag
    shadow_miss.rmiss
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

