target_sources(engine
    PRIVATE
    gpu.cpp
    gpu.h
    renderer.cpp
    renderer.h
    swapchain.cpp
    swapchain.h
    window.cpp
    window.h
    vulkan_inc.h
    )

#add_subdirectory(hybrid)
add_subdirectory(raster)
add_subdirectory(raytrace)
add_subdirectory(entity_initializers)
