cmake_minimum_required(VERSION 3.24)
project(sdl_gltf)

add_executable(${CMAKE_PROJECT_NAME})
# if debug, compile & link with asan
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
	target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

add_subdirectory(vendor)
add_subdirectory(src)
add_subdirectory(include)

target_link_libraries(${PROJECT_NAME} PRIVATE vendor)
