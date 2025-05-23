cmake_minimum_required(VERSION 3.10)

project(Singularity)

add_executable(Singularity main.cpp)

find_package(glm CONFIG REQUIRED)
target_link_libraries(Singularity PRIVATE glm::glm)

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(Singularity PRIVATE SDL3::SDL3)

find_package(Vulkan REQUIRED)
target_link_libraries(Singularity PRIVATE Vulkan::Vulkan)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-D_DEBUG)
endif()