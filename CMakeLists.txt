# Generic info
cmake_minimum_required(VERSION 2.6)
project(opendsme C)
set(opendsme_VERSION 0.1)

# Checks
set(CMAKE_REQUIRED_FLAGS "-std=c99 -Wall -Wextra -pedantic")
include(CheckIncludeFiles)
check_include_files(mtd/mtd-user.h HAVE_MTD_USER_H)
if(!${HAVE_MTD_USER_H})
	message(FATAL_ERROR "<mtd/mtd-user.h> can't be included")
endif()

# Executables
add_definitions(${CMAKE_REQUIRED_FLAGS})
add_definitions(-D_POSIX_C_SOURCE=2 -D_FORTIFY_SOURCE=2)
add_executable(open-wlan-cal open-wlan-cal.c)

# Tests
enable_testing()

# Installation
install(TARGETS open-wlan-cal RUNTIME DESTINATION sbin)

# Packaging
set(CPACK_PACKAGE_VERSION ${opendsme_VERSION})
set(CPACK_PACKAGE_VENDOR "Marat Radchenko")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README")
set(CPACK_SOURCE_IGNORE_FILES .git .gitignore build)
include(CPack)
