# Generic info
cmake_minimum_required(VERSION 2.6)
project(opendsme C)
set(opendsme_VERSION 0.1-git)

# Dependencies
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")
find_package(Popt REQUIRED)

# Checks
set(CMAKE_REQUIRED_FLAGS "-std=c99 -Wall -Wextra -pedantic")
include(CheckIncludeFiles)
check_include_files(mtd/mtd-user.h HAVE_MTD_USER_H)
if(!${HAVE_MTD_USER_H})
	message(FATAL_ERROR "<mtd/mtd-user.h> can't be included")
endif()

# config.h
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")

# Executables
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
add_definitions(${CMAKE_REQUIRED_FLAGS})
add_definitions(-D_FORTIFY_SOURCE=2)
add_executable(open-wlan-cal open-wlan-cal.c)
target_link_libraries(open-wlan-cal ${Popt_LIBRARY})

# Tests
enable_testing()

# Installation
install(TARGETS open-wlan-cal RUNTIME DESTINATION sbin)
