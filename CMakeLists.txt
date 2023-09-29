cmake_minimum_required( VERSION 3.13 FATAL_ERROR )
cmake_policy( VERSION 3.13 )

project( ffxi )


set(CMAKE_MODULE_PATH
	${CMAKE_MODULE_PATH}
	${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

###############################
## deps
###############################
include( "cmake/compiler.cmake" )

find_package( Vulkan REQUIRED )
find_package( GLM REQUIRED )
find_package( SDL2 REQUIRED )
find_package( VulkanMemoryAllocator CONFIG REQUIRED )

###############################
## output
###############################
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" )

add_subdirectory( "engine" )
add_subdirectory( "ffxi" )
add_subdirectory( "external/soloud/contrib" )
