set(SDL_TEST_LIBRARY OFF)
set(BUILD_SHARED_LIBS ON)
add_subdirectory(SDL)


set(GLM_BUILD_TESTS OFF)
add_subdirectory(glm)

add_subdirectory(fastgltf)

add_library(vendor INTERFACE)
target_link_libraries(
	vendor INTERFACE 
	SDL3::SDL3
	glm::glm
	fastgltf::fastgltf
)
