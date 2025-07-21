# all .cpp files
set(
  sources
  main.cpp
  App.cpp
  GPUResources.cpp
  Pipelines.cpp
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${sources})
