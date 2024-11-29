target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	async_compute.cppm
	global_descriptors.cppm
	post_process_pipeline.cppm
	raster_pipeline.cppm
	raytrace_pipeline.cppm
	PRIVATE
	async_compute.cpp
	global_descriptors.cpp
	post_process_pipeline.cpp
	raster_pipeline.cpp
	raytrace_pipeline.cpp
)
