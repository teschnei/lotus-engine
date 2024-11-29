target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	audio.cppm
	config.cppm
	engine.cppm
	game.cppm
	input.cppm
	light_manager.cppm
	lotus.cppm
	scene.cppm
	PRIVATE
	audio.cpp
	engine.cpp
	input.cpp
	light_manager.cpp
	scene.cpp
)

add_subdirectory(entity)
add_subdirectory(renderer)
add_subdirectory(ui)
add_subdirectory(util)
