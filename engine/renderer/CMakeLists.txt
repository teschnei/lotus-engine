target_sources(engine
    PRIVATE
    acceleration_structure.cpp
    acceleration_structure.h
    animation.cpp
    animation.h
    material.cpp
    material.h
    memory.cpp
    memory.h
    mesh.cpp
    mesh.h
    model.cpp
    model.h
    raytrace_query.cpp
    raytrace_query.h
    sdl_inc.h
    skeleton.cpp
    skeleton.h
    texture.cpp
    texture.h
    )

add_subdirectory(vulkan)
