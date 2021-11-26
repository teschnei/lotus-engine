target_sources(ffxi
    PRIVATE
    actor.cpp
    actor.h
    actor_data.h
    landscape_entity.cpp
    landscape_entity.h
    particle.cpp
    particle.h
    third_person_ffxi_camera.cpp
    third_person_ffxi_camera.h
    third_person_ffxiv_camera.cpp
    third_person_ffxiv_camera.h
    )

add_subdirectory(component)
add_subdirectory(loader)
