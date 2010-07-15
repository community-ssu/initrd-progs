# Generic info
cmake_minimum_required(VERSION 2.6)
project(fb_text2screen C)
set(fb_text2screen_VERSION 0.2-git)

# Compiler flags
set(CMAKE_REQUIRED_DEFINITIONS -std=c99 -pedantic -Wall -Wextra)
set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} -D_FORTIFY_SOURCE=2)
add_definitions(${CMAKE_REQUIRED_DEFINITIONS})

# Dependencies
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
find_package(Popt REQUIRED)

# Checks
include(CheckIncludeFile)

check_include_file("linux/omapfb.h" HAVE_LINUX_OMAPFB_H)
if(NOT HAVE_LINUX_OMAPFB_H)
  set(LINUX_SRC_DIR "/usr/src/linux" CACHE PATH "Path to Linux kernel sources")
  if(NOT IS_DIRECTORY "${LINUX_SRC_DIR}")
	    message(FATAL_ERROR "No Linux sources found at ${LINUX_SRC_DIR}. Set LINUX_SRC_DIR variable to directory with Linux sources")
  endif()
  
  set(CMAKE_REQUIRED_INCLUDES "${LINUX_SRC_DIR}/include")
  check_include_file("asm-arm/arch-omap/omapfb.h" HAVE_ARCH_OMAP_OMAPFB_H)
  if(NOT HAVE_ARCH_OMAP_OMAPFB_H)
      message(FATAL_ERROR "omapfb.h not found")
  endif()
  include_directories(AFTER SYSTEM ${CMAKE_REQUIRED_INCLUDES})
endif()

# config.h
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

# Executables
add_executable(fb_text2screen fb_text2screen.c)
target_link_libraries(fb_text2screen ${Popt_LIBRARY})

# Installation
install(TARGETS fb_text2screen RUNTIME DESTINATION bin)
