cmake_minimum_required(VERSION 3.19)
set(executable_name prototype)
file(GLOB SRC_FILES *.cpp)

add_executable(${executable_name})

target_sources(${executable_name} PRIVATE ${SRC_FILES})

#target_link_libraries(${executable_name} PRIVATE Vulkan::Vulkan)
target_link_libraries(${executable_name} PRIVATE glm::glm)
target_link_libraries(${executable_name} PRIVATE glfw)
target_link_libraries(${executable_name} PRIVATE volk)

set_target_properties(${executable_name} PROPERTIES CXX_STANDARD 17)
set_target_properties(${executable_name} PROPERTIES CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(${executable_name} PRIVATE "/W4")
    else()
        target_compile_options(${executable_name} PRIVATE "-Wall" "-Wextra")
    endif()
endif()