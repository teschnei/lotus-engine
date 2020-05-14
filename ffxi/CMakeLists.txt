project( ffxi )

add_executable( ffxi
    main.cpp
    config.cpp
    config.h
    particle_tester.cpp
    particle_tester.h
)
target_include_directories(ffxi PRIVATE "." )
target_include_directories(ffxi PRIVATE "../external/stb" )

target_link_libraries( ffxi engine ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} $<$<PLATFORM_ID:Linux>:-ldl> )

if (MSVC)
  add_custom_command( TARGET ffxi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SDL2_DLL} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/../textures ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/textures
  )
endif()

add_subdirectory(dat)
add_subdirectory(entity)
add_subdirectory(shaders)
add_subdirectory(task)