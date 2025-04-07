target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	engine.cppm
	instance.cppm
	source.cppm
	PRIVATE
	engine.cpp
)
