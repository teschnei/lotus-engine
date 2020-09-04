project( engine )

add_library( engine
    background_work.h
    background_work.cpp
    config.cpp
    config.h
    core.cpp
    core.h
    game.h
    input.cpp
    input.h
    light_manager.cpp
    light_manager.h
    random.h
    scene.cpp
    scene.h
    types.h
    worker_pool.cpp
    worker_pool.h
    worker_thread.cpp
    worker_thread.h
    worker_thread_concurrent.cpp
    worker_thread_concurrent.h
    work_item.cpp
    work_item.h
 )

target_include_directories(engine PUBLIC "../" )
target_include_directories(engine PUBLIC "../external/vma" )

target_link_libraries(engine $<$<PLATFORM_ID:Linux>:-ldl>)
target_link_libraries(engine Vulkan::Vulkan SDL::SDL2 GLM::GLM)

set_property(TARGET engine PROPERTY CXX_STANDARD 20)
set_property(TARGET engine PROPERTY CXX_STANDARD_REQUIRED ON)

add_subdirectory(entity)
add_subdirectory(renderer)
add_subdirectory(task)
