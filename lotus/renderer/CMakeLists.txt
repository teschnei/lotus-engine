target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	acceleration_structure.cppm
	animation.cppm
	material.cppm
	memory.cppm
	mesh.cppm
	model.cppm
	raytrace_query.cppm
	skeleton.cppm
	texture.cppm
	PRIVATE
	acceleration_structure.cpp
	animation.cpp
	material.cpp
	mesh.cpp
	model.cpp
	raytrace_query.cpp
	skeleton.cpp
	texture.cpp
)

add_subdirectory(vulkan)
