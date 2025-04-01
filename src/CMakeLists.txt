set(src "thd_pool.c")
add_library(thd_pool OBJECT ${src})
target_include_directories(thd_pool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
