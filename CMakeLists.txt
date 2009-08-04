# Generic info
cmake_minimum_required(VERSION 2.6)
project(fb_text2screen C)
set(fb_text2screen_VERSION 0.1.1)

# Dependencies
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")
find_package(Popt REQUIRED)
set(LINUX_SRC_DIR "/usr/src/linux" CACHE FILEPATH "Path to Linux kernel sources")
if(NOT IS_DIRECTORY "${LINUX_SRC_DIR}")
	message(FATAL_ERROR "No Linux sources found at ${LINUX_SRC_DIR}. Set LINUX_SRC_DIR variable to directory with Linux sources")
endif()
include_directories(AFTER SYSTEM "${LINUX_SRC_DIR}/include")

# Checks
set(CMAKE_REQUIRED_FLAGS "-std=c99 -Wall -Wextra -pedantic")
include(CheckIncludeFiles)

# config.h
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")

# Executables
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
add_definitions(${CMAKE_REQUIRED_FLAGS})
add_definitions(-D_FORTIFY_SOURCE=2)
add_executable(fb_text2screen fb_text2screen.c)
target_link_libraries(fb_text2screen ${Popt_LIBRARY})

# Tests
enable_testing()

# Installation
install(TARGETS fb_text2screen RUNTIME DESTINATION bin)
