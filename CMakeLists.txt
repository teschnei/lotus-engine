cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

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

# Vulkan Module (temporary)
add_library( VulkanHppModule )
target_sources( VulkanHppModule PUBLIC
  FILE_SET CXX_MODULES
  BASE_DIRS ${Vulkan_INCLUDE_DIR}
  FILES ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
)
target_compile_definitions(VulkanHppModule
	PUBLIC
	VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
	PRIVATE
	VULKAN_HPP_NO_CONSTRUCTORS
	VULKAN_HPP_NO_SETTERS
)
target_compile_features( VulkanHppModule PUBLIC cxx_std_20 )
target_link_libraries( VulkanHppModule PUBLIC Vulkan::Vulkan )

add_library(lotus-engine)

target_include_directories(lotus-engine PUBLIC "." )
target_include_directories(lotus-engine PUBLIC "external/soloud/include" )

target_compile_features(lotus-engine PUBLIC cxx_std_20)

target_link_libraries(lotus-engine PUBLIC VulkanHppModule SDL::SDL2 GLMModule soloud GPUOpen::VulkanMemoryAllocator $<$<PLATFORM_ID:Linux>:-ldl>)

target_compile_definitions(GLMModule PUBLIC
	GLM_FORCE_LEFT_HANDED
	PRIVATE
	GLM_ENABLE_EXPERIMENTAL
	GLM_GTC_INLINE_NAMESPACE
	GLM_EXT_INLINE_NAMESPACE
	GLM_GTX_INLINE_NAMESPACE
)

add_subdirectory(lotus)
add_subdirectory(external/soloud/contrib)
