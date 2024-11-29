target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	buffer_resource.cppm
	descriptor_resource.cppm
	gpu.cppm
	renderer.cppm
	renderer_settings.cppm
	swapchain.cppm
	ui_renderer.cppm
	window.cppm
	PRIVATE
	renderer.cpp
	swapchain.cpp
	ui_renderer.cpp
	vk_dispatch.cpp
	window.cpp
)

add_subdirectory(hybrid)
add_subdirectory(raster)
add_subdirectory(raytrace)
add_subdirectory(common)
