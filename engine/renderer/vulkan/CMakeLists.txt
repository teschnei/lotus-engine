target_sources(engine
    PRIVATE
    gpu.cpp
    gpu.h
    global_resources.cpp
    global_resources.h
    renderer.cpp
    renderer.h
    swapchain.cpp
    swapchain.h
    ui_renderer.cpp
    ui_renderer.h
    vulkan_inc.h
    window.cpp
    window.h
    )

add_subdirectory(hybrid)
add_subdirectory(raster)
add_subdirectory(raytrace)
add_subdirectory(entity_initializers)
