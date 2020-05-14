project( engine )

file( GLOB_RECURSE SOURCE *.cpp *.hpp *.c )
file( GLOB_RECURSE HEADERS *.h )

add_library( engine
    ""
)

target_include_directories(engine PUBLIC "." )
target_include_directories(engine PUBLIC "../external/vma" )
target_include_directories(engine PUBLIC ${Vulkan_INCLUDE_DIR} )
target_include_directories(engine PUBLIC ${SDL2_INCLUDE_DIR} )
target_include_directories(engine PUBLIC ${GLM_INCLUDE_DIR} )

target_link_libraries( engine ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} $<$<PLATFORM_ID:Linux>:-ldl>)

add_subdirectory(engine)