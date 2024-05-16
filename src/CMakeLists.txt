cmake_minimum_required(VERSION 3.19)
set(executable_name vulk_tut)
file(GLOB SRC_FILES *.cpp)

add_executable(${executable_name})

target_sources(${executable_name} PRIVATE ${SRC_FILES})

target_link_libraries(${executable_name} PRIVATE Vulkan::Vulkan)
target_link_libraries(${executable_name} PRIVATE glm::glm)
target_link_libraries(${executable_name} PRIVATE glfw)

target_compile_options(${executable_name} PRIVATE "-Wall" "-O3")