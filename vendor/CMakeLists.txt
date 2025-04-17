set(SDL_TEST_LIBRARY OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(SDL)

set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF)
set(SDLSHADERCROSS_VENDORED ON)
add_subdirectory(SDL_Shadercross)

set(GLM_BUILD_TESTS OFF)
add_subdirectory(glm)

add_subdirectory(fastgltf)

add_library(vendor INTERFACE)
target_link_libraries(
	vendor INTERFACE 
	SDL3::SDL3-static
	SDL3_shadercross::SDL3_shadercross-static
	glm::glm
	fastgltf::fastgltf
)
