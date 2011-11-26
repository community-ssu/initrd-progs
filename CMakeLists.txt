# Generic info
cmake_minimum_required(VERSION 2.8)
project(opendsme C)

set(opendsme_VERSION 0.3-git)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Compiler flags
set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
    -D_GNU_SOURCE -D_FORTIFY_SOURCE=2)
set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
    -pedantic -std=c99 -fvisibility=hidden -fmessage-length=0
    -fdiagnostics-show-option -ffast-math -O -Wabi -Wfatal-errors
    -Wall -Wextra -Werror -Wformat=2 -Winit-self -Wunknown-pragmas
    -Wmissing-include-dirs -Wswitch-default -Wswitch-enum -Wunused -ftree-vrp
    -fstrict-overflow -Wstrict-overflow=5 -Wfloat-equal -Wsync-nand
    -Wundef -Wshadow -Wno-overlength-strings
    -Wbad-function-cast -Wcast-qual -Wcast-align
    -Wwrite-strings -Wconversion -Wlogical-op -Waggregate-return
    -Wmissing-noreturn -Wmissing-format-attribute -Wnormalized=nfkc -Wpacked
    -Wpadded -Wno-error=padded -Wredundant-decls -Wnested-externs -Winline
    -Winvalid-pch -Wvolatile-register-var -Wdisabled-optimization
    -fstrict-aliasing -Wimport -Wdisabled-optimization -Wpacked-bitfield-compat)
add_definitions(${CMAKE_REQUIRED_DEFINITIONS}
    -Wold-style-definition -Wmissing-prototypes
    -Wmissing-declarations -Wstrict-prototypes)

# Dependencies
find_package(Popt REQUIRED)

include(CheckIncludeFile)
include(CheckSymbolExists)

check_include_file(mtd/mtd-user.h HAVE_MTD_USER_H)
if(NOT HAVE_MTD_USER_H)
    message(FATAL_ERROR "Your system doesn't have mtd/mtd-user.h")
endif()

check_include_file(stdbool.h HAVE_STDBOOL_H)
if(NOT HAVE_STDBOOL_H)
    message(FATAL_ERROR "Your system doesn't have stdbool.h")
endif()

check_symbol_exists(asprintf stdio.h HAVE_ASPRINTF)
if(NOT HAVE_ASPRINTF)
    message(FATAL_ERROR "Your system doesn't have asprintf(3) function")
endif()

check_symbol_exists(pread unistd.h HAVE_PREAD)
if(NOT HAVE_PREAD)
    message(FATAL_ERROR "Your system doesn't have pread(2) function")
endif()

# config.h
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h")

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(include)
add_subdirectory(src)
