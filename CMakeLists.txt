cmake_minimum_required(VERSION 3.10)

project(c_thread_pool C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG_MODE ON)
option(ENABLE_TEST ON)

if (DEBUG_MODE)
	set(CMAKE_BUILD_TYPE Debug)
	add_compile_definitions(C_THD_POOL_DEBUG)
else()
	set(CMAKE_BUILD_TYPE Release)
	add_compile_definitions(NDEBUG)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_subdirectory(src)

if(PROJECT_IS_TOP_LEVEL OR FORCE_GENERATE_COMPILE_COMMANDS)
	message(STATUS "add move_compile_commands target")
    add_custom_target(move_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_BINARY_DIR}/compile_commands.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json"
        DEPENDS "${CMAKE_BINARY_DIR}/compile_commands.json"
    )
endif()


if((PROJECT_IS_TOP_LEVEL OR FORCE_GENERATE_COMPILE_COMMANDS) AND ENABLE_TEST)
	enable_testing()
	add_subdirectory(test/unit_test)
endif()
