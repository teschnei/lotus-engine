project( ffxi )

add_executable( ffxi
    main.cpp
    config.cpp
    config.h
    ffxi.h
    ffxi.cpp
    particle_tester.cpp
    particle_tester.h
    scheduler_resources.cpp
    scheduler_resources.h
    system_dat.h
    system_dat.cpp
    vana_time.h
     )
target_include_directories(ffxi PRIVATE "." )
target_include_directories(ffxi PRIVATE "../external/stb" )

target_link_libraries( ffxi engine ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} $<$<PLATFORM_ID:Linux>:-ldl> )

if (MSVC)
    #these files never change, so they can be copied at generation time rather than build time
    file( COPY ${SDL2_DLL} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

file( COPY ../textures DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_subdirectory(dat)
add_subdirectory(entity)
add_subdirectory(shaders)
