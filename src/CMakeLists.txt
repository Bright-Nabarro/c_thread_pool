
file(GLOB src "*.c")

add_library(ctp SHARED ${src})
target_include_directories(ctp PUBLIC 
	"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
	"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>"
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

