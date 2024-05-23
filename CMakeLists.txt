cmake_minimum_required(VERSION 3.19)

set(project_name VULKAN_TUTORIAL)

project(${VULKAN_TUTORIAL}
    LANGUAGES CXX
)

#find_package(Vulkan REQUIRED)

set(DGLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DBUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glm)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

if (WIN32)
   set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
    set(UNIX_WSI_TYPE "")
    execute_process(COMMAND detect_unix_wsi.sh OUTPUT_VARIABLE UNIX_WSI_TYPE)
    if(UNIX_WSI_TYPE STREQUAL "WAYLAND")
        set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
    else()
        set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
    endif()
elseif(APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
endif()
add_subdirectory(third_party/volk)

add_subdirectory(src)