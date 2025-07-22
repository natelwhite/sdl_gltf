cmake_minimum_required(VERSION 3.24)
project(sdl_gltf)

add_executable(${CMAKE_PROJECT_NAME})

add_subdirectory(vendor)
add_subdirectory(src)
add_subdirectory(include)

target_link_libraries(${PROJECT_NAME} PRIVATE vendor)
