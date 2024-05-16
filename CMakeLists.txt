cmake_minimum_required(VERSION 3.19)

set(project_name VULKAN_TUTORIAL)

project(${VULKAN_TUTORIAL}
    LANGUAGES CXX
)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED true)

find_package(Vulkan REQUIRED)

set(DGLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DBUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glm)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

add_subdirectory(src)