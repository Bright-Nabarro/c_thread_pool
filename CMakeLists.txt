cmake_minimum_required(VERSION 3.10)

project(c_thread_pool C)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG_MODE ON)
option(ENABLE_TEST ON)

if (DEBUG_MODE)
	set(CMAKE_BUILD_TYPE Debug)
	add_compile_definitions(THD_POOL_DEBUG)
else()
	set(CMAKE_BUILD_TYPE Release)
	add_compile_definitions(NDEBUG)
endif()

add_compile_options(
	-Wall
	-Wextra
	-Wuninitialized
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_subdirectory(src)

if (ENABLE_TEST)
	message(STATUS "triggered enable test")
	enable_testing()
	add_subdirectory(test/unit_test)
endif()

add_custom_target(move_compile_commands ALL
	COMMAND ${CMAKE_COMMAND} -E copy_if_different			#平台无关的复制命令
			"${CMAKE_BINARY_DIR}/compile_commands.json"
			"${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json"
	DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json"
)
