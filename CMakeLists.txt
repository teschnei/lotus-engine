cmake_minimum_required( VERSION 3.2 FATAL_ERROR )
cmake_policy( VERSION 3.2 )

project( ffxi )


set(CMAKE_MODULE_PATH
	${CMAKE_MODULE_PATH}
	${CMAKE_MODULE_PATH}/cmake
)

###############################
## deps
###############################
include( "cmake/compiler.cmake" )
include( "cmake/FindGLM.cmake" )
include( "cmake/FindSDL2.cmake" )

find_package( Vulkan REQUIRED )

###############################
## includes
###############################
include_directories( "engine" )
include_directories( "ffxi" )

include_directories( "external" )
include_directories( "external/stb" )
include_directories( "external/vma" )

include_directories( ${Vulkan_INCLUDE_DIR} )
include_directories( ${SDL2_INCLUDE_DIR} )
include_directories( ${GLM_INCLUDE_DIR} )


###############################
## output
###############################
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin" )

add_subdirectory( "engine" )
add_subdirectory( "ffxi" )