target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	element.cppm
	events.cppm
	ui.cppm
	PRIVATE
	element.cpp
	ui.cpp
)
