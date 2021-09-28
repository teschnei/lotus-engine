project( engine )

add_library( engine
    async_queue.h
    audio.cpp
    audio.h
    config.cpp
    config.h
    core.cpp
    core.h
    game.cpp
    game.h
    input.cpp
    input.h
    light_manager.cpp
    light_manager.h
    random.h
    scene.cpp
    scene.h
    shared_linked_list.h
    task.h
    types.h
    worker_pool.cpp
    worker_pool.h
    worker_task.h
    )

target_include_directories(engine PUBLIC "../" )
target_include_directories(engine PUBLIC "../external/vma" )
target_include_directories(engine PUBLIC "../external/soloud/include" )

target_compile_options(engine PUBLIC $<$<PLATFORM_ID:Linux>:-fcoroutines>)

target_link_libraries(engine $<$<PLATFORM_ID:Linux>:-ldl>)
target_link_libraries(engine Vulkan::Vulkan SDL::SDL2 GLM::GLM soloud)

add_compile_definitions(GLM_FORCE_LEFT_HANDED)

add_subdirectory(entity)
add_subdirectory(renderer)
add_subdirectory(ui)
