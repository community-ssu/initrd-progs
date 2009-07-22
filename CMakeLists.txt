# Generic info
cmake_minimum_required(VERSION 2.6)
project(opendsme C)

set(opendsme_VERSION 0.2-git)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

add_subdirectory(include)
add_subdirectory(src)
