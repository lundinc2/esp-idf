# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
add_compile_definitions(configUSE_POSIX_ERRNO=1)
add_compile_definitions(configUSE_APPLICATION_TASK_TAG=1)

project(hello-world)
