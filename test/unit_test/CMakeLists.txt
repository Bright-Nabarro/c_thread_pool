set(targets
	queue_st
	queue_mt
	thd_pool
)

foreach(target ${targets})
	message(STATUS "add ${target} test")
	add_executable(${target} "${target}_test.c")
	target_link_libraries(${target} PUBLIC ctp)
	set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
		"${CMAKE_SOURCE_DIR}/test/bin")
	add_test(${target}_test "${CMAKE_SOURCE_DIR}/test/bin/${target}")
endforeach()


