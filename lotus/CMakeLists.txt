
target_sources(lotus-engine
    PRIVATE
    async_queue.h
    audio.cpp
    audio.h
    config.cpp
    config.h
    core.cpp
    core.h
    game.cpp
    game.h
    idgenerator.h
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

add_subdirectory(entity)
add_subdirectory(renderer)
add_subdirectory(ui)
