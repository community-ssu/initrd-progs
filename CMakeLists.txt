# Generic info
cmake_minimum_required(VERSION 2.6)
project(fb_text2screen C)
set(fb_text2screen_VERSION 0.1-git)

# Dependencies
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules")
find_package(Popt REQUIRED)

# Checks
set(CMAKE_REQUIRED_FLAGS "-std=c99 -Wall -Wextra -pedantic")
include(CheckIncludeFiles)

# Executables
add_definitions(${CMAKE_REQUIRED_FLAGS})
add_definitions(-D_FORTIFY_SOURCE=2)
add_executable(fb_text2screen fb_text2screen.c)
target_link_libraries(fb_text2screen ${Popt_LIBRARY})

# Tests
enable_testing()

# Installation
install(TARGETS fb_text2screen RUNTIME DESTINATION bin)

# Packaging
set(CPACK_PACKAGE_VERSION ${fb_text2screen_VERSION})
set(CPACK_PACKAGE_VENDOR "Marat Radchenko")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README")
set(CPACK_SOURCE_IGNORE_FILES .git .gitignore build)
include(CPack)
