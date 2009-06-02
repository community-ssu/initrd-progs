# Generic info
cmake_minimum_required(VERSION 2.6)
project(opendsme C)
set(opendsme_VERSION 0.1)

# Checks
include(CheckIncludeFile)

# Executables
add_definitions("-std=c99 -Wall -Wextra -pedantic")
add_definitions(-D_POSIX_C_SOURCE=2 -D_FORTIFY_SOURCE=2)
add_executable(open-wlan-cal open-wlan-cal.c)

# Tests
enable_testing()

# Installation
install(TARGETS open-wlan-cal RUNTIME DESTINATION sbin)

# Packaging
set(CPACK_PACKAGE_VERSION ${opendsme_VERSION})
set(CPACK_PACKAGE_VENDOR "Marat Radchenko")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README")
set(CPACK_SOURCE_IGNORE_FILES .git .gitignore build)
include(CPack)
