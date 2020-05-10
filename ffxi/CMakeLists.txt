project( ffxi )

file( GLOB_RECURSE SOURCE *.cpp *.hpp *.c )
file( GLOB_RECURSE HEADERS *.h )

add_executable( ffxi
	${SOURCE}
	${HEADERS}
)

message( STATUS "VULKAN SHIT ${Vulkan_INCLUDE_DIR} ${Vulkan_LIBRARIES}" )

target_link_libraries( ffxi engine ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} -ldl )

if (MSVC)
  add_custom_command( TARGET ffxi POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${SDL2_DLL} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Copying SDL2.dll over to runtime directory..."
  )
endif()