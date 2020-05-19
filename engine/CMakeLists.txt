project( engine )

file( GLOB_RECURSE SOURCE *.cpp *.hpp *.c )
file( GLOB_RECURSE HEADERS *.h )

add_library( engine
    ""
)

target_include_directories(engine PUBLIC "." )
target_include_directories(engine PUBLIC "../external/vma" )

target_link_libraries(engine $<$<PLATFORM_ID:Linux>:-ldl>)
target_link_libraries(engine Vulkan::Vulkan SDL::SDL2 GLM::GLM)

add_subdirectory(engine)