project( engine )

file( GLOB_RECURSE SOURCE *.cpp *.hpp *.c )
file( GLOB_RECURSE HEADERS *.h )

add_library( engine
  "${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/../external"
	${SOURCE}
	${HEADERS}
)

message( STATUS "VULKAN SHIT ${Vulkan_INCLUDE_DIR} ${Vulkan_LIBRARIES}" )

target_link_libraries( engine ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} )