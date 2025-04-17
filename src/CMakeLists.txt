# all .cpp files
set(
  sources
  main.cpp
  SDL_Context.cpp
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${sources})
