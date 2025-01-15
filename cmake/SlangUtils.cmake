# Modified from:
#   https://github.com/eliemichel/SlangWebGPU
#
# MIT License
# Copyright (c) 2024 Elie Michel
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

#############################################
# Information

# This provides utility CMake functions to compile slang shaders to SPIRV
# Until slangc supports depfiles in modules, modules in use must be specified manually

# Example:
#   add_slang_shader(
#     hello_world
#     SOURCE shaders/hello-world.slang
#   )
function(add_slang_shader TargetName)
	set(options)
	set(oneValueArgs SOURCE ENTRY)
	set(multiValueArgs SLANG_INCLUDE_DIRECTORIES MODULES)
	cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if (NOT SLANGC)
		message(FATAL_ERROR "Could not find Slang executable. Provide the SLANGC cache variable or use FindSlang.cmake.")
	endif()

	# The input slang file
	set(SLANG_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/${arg_SOURCE}")

	# The generated SPIRV file
	cmake_path(GET arg_SOURCE STEM LAST_ONLY stem)
	set(SPIRV_SHADER_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
	set(SPIRV_SHADER "${SPIRV_SHADER_DIR}/${stem}.spv")

	# Dependency file
	set(DEPFILE "${CMAKE_CURRENT_BINARY_DIR}/${TargetName}.depfile")
	set(DEPFILE_OPT)
	if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.21.0")
		list(APPEND DEPFILE_OPT "DEPFILE" "${DEPFILE}")
	else()
		message(AUTHOR_WARNING "Using a version of CMake older than 3.21 does not allow keeping track of Slang files imported in each others when building the compilation dependency graph. You may need to manually trigger shader transpilation.")
	endif()

	# Extra include directories
	set(INCLUDE_DIRECTORY_OPTS)
	foreach (dir ${arg_SLANG_INCLUDE_DIRECTORIES})
		list(APPEND INCLUDE_DIRECTORY_OPTS "-I${dir}")
	endforeach()

	foreach (module ${arg_MODULES})
		set(MODULE_DIR)
		get_property(MODULE_DIR TARGET ${module} PROPERTY MODULE_DIR )
		list(APPEND INCLUDE_DIRECTORY_OPTS "-I${MODULE_DIR}")
	endforeach()

	# Target that represents the generated SPIRV file in the dependency graph.
	add_custom_target(${TargetName}
		DEPENDS
		    ${SPIRV_SHADER}
		    ${arg_MODULES}
	)

	# Command that give the recipe to transpile slang into SPIRV
	# i.e., internal behavior of the target ${TargetName} defined above
	add_custom_command(
		COMMENT
		    "Transpiling shader '${SLANG_SHADER}' into '${SPIRV_SHADER}'"
		OUTPUT
		    ${SPIRV_SHADER}
		COMMAND
		${CMAKE_COMMAND} -E make_directory ${SPIRV_SHADER_DIR}
		COMMAND
			${SLANGC}
			${SLANG_SHADER}
			-fvk-use-entrypoint-name
			-fvk-use-scalar-layout
			-target spirv
			-o ${SPIRV_SHADER}
			-depfile ${DEPFILE}
			${INCLUDE_DIRECTORY_OPTS}
		MAIN_DEPENDENCY
			${SLANG_SHADER}
		DEPENDS
			${SLANGC}
		${DEPFILE_OPT}
	)
endfunction(add_slang_shader)

#############################################
# Information

# This provides utility CMake functions to compile slang shaders to a slang module
# The intended use is to supply the main source file as SOURCE, and a list of files
# containing implementing files
# Currently, it seems that slangc doesn't support depfile for modules, so the files
# have to be specified manually

# Example:
#   add_slang_module(
#     hello_world
#     SOURCE shaders/hello-world.slang
#     IMPLEMENTING hello_world
#   )
function(add_slang_module TargetName)
    set(options)
    set(oneValueArgs SOURCE)
    set(multiValueArgs IMPLEMENTING SLANG_INCLUDE_DIRECTORIES)
	cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

	if (NOT SLANGC)
		message(FATAL_ERROR "Could not find Slang executable. Provide the SLANGC cache variable or use FindSlang.cmake.")
	endif()

	# The input slang file
	set(SLANG_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/${arg_SOURCE}")

	# The generated SPIRV file
	cmake_path(GET arg_SOURCE PARENT_PATH parent)
	cmake_path(GET arg_SOURCE STEM LAST_ONLY stem)
	set(MODULE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${parent}")
	set(MODULE_OUT "${MODULE_DIR}/${stem}.slang-module")

	# Dependency file
	set(DEPFILE "${CMAKE_CURRENT_BINARY_DIR}/${TargetName}.depfile")
	set(DEPFILE_OPT)
	if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.21.0")
		list(APPEND DEPFILE_OPT "DEPFILE" "${DEPFILE}")
	else()
		message(AUTHOR_WARNING "Using a version of CMake older than 3.21 does not allow keeping track of Slang files imported in each others when building the compilation dependency graph. You may need to manually trigger shader transpilation.")
	endif()

	# Extra include directories
	set(INCLUDE_DIRECTORY_OPTS)
	foreach (dir ${arg_SLANG_INCLUDE_DIRECTORIES})
		list(APPEND INCLUDE_DIRECTORY_OPTS "-I${dir}")
	endforeach()

	# Target that represents the generated SPIRV file in the dependency graph.
	add_custom_target(${TargetName}
		DEPENDS
		${MODULE_OUT}
	)

	# Command that give the recipe to transpile slang into SPIRV
	# i.e., internal behavior of the target ${TargetName} defined above
	add_custom_command(
		COMMENT
		"Compiling module '${SLANG_SHADER}' into '${MODULE_OUT}'"
		OUTPUT
		    ${MODULE_OUT}
		COMMAND
		    ${CMAKE_COMMAND} -E make_directory ${MODULE_DIR}
		COMMAND
			${SLANGC}
			${SLANG_SHADER}
			-o ${MODULE_OUT}
			-depfile ${DEPFILE}
			${INCLUDE_DIRECTORY_OPTS}
		MAIN_DEPENDENCY
			${SLANG_SHADER}
		DEPENDS
			${SLANGC}
			${arg_IMPLEMENTING}
		${DEPFILE_OPT}
	)

	set_property(TARGET ${TargetName}
	    PROPERTY
	    MODULE_DIR ${MODULE_DIR}
	)

endfunction(add_slang_module)
