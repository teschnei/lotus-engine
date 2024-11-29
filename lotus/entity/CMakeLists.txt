target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	entity.cppm
	PRIVATE
	entity.cpp
)

add_subdirectory(component)
