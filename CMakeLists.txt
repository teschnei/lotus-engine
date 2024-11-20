cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
cmake_policy(VERSION 3.13)

project(lotus-engine)

set(CMAKE_MODULE_PATH
	${CMAKE_MODULE_PATH}
	${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

###############################
## deps
###############################

find_package(Vulkan REQUIRED)
find_package(GLM REQUIRED)
find_package(SDL2 REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

add_library(lotus-engine)

target_include_directories(lotus-engine PUBLIC "." )
target_include_directories(lotus-engine PUBLIC "external/soloud/include" )

target_compile_features(lotus-engine PUBLIC cxx_std_20)
target_compile_definitions(lotus-engine PUBLIC GLM_ENABLE_EXPERIMENTAL)

target_link_libraries(lotus-engine $<$<PLATFORM_ID:Linux>:-ldl>)
target_link_libraries(lotus-engine Vulkan::Vulkan SDL::SDL2 GLM::GLM soloud GPUOpen::VulkanMemoryAllocator)

add_compile_definitions(GLM_FORCE_LEFT_HANDED)

add_subdirectory(lotus)
add_subdirectory(external/soloud/contrib)
