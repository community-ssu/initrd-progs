# Generic info
cmake_minimum_required(VERSION 2.6)
project(initrd-progs C)
set(initrd-progs_VERSION 1.0)

# Compiler flags
set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
    -D_GNU_SOURCE -D_FORTIFY_SOURCE=2)
set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
    -pedantic -std=c99 -fvisibility=hidden -fmessage-length=0
    -fdiagnostics-show-option -ffast-math -O -Wfatal-errors
    -Wall -Wextra -Wformat=2 -Winit-self -Wunknown-pragmas
    -Wmissing-include-dirs -Wswitch-default -Wswitch-enum -Wunused -ftree-vrp
    -Wfloat-equal
    -Wundef -Wshadow -Wno-overlength-strings
    -Wbad-function-cast -Wcast-qual -Wcast-align -Wno-variadic-macros
    -Wwrite-strings -Waggregate-return
    -Wmissing-noreturn -Wmissing-format-attribute -Wnormalized=nfkc -Wpacked
    -Wredundant-decls -Wnested-externs -Winline
    -Winvalid-pch -Wvolatile-register-var -Wdisabled-optimization
    -fstrict-aliasing -Wimport -Wdisabled-optimization)
add_definitions(${CMAKE_REQUIRED_DEFINITIONS}
    -Wold-style-definition -Wmissing-prototypes
    -Wmissing-declarations -Wstrict-prototypes)

# Dependencies
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
find_package(Popt REQUIRED)

# Checks
include(CheckIncludeFile)
include(CheckSymbolExists)

check_include_file("linux/omapfb.h" HAVE_LINUX_OMAPFB_H "-Dsize_t=__u32")
if(NOT HAVE_LINUX_OMAPFB_H)
  set(LINUX_SRC_DIR "/usr/src/linux" CACHE PATH "Path to Linux kernel sources")
  if(NOT IS_DIRECTORY "${LINUX_SRC_DIR}")
	    message(FATAL_ERROR "No Linux sources found at ${LINUX_SRC_DIR}. Set LINUX_SRC_DIR variable to directory with Linux sources")
  endif()

  set(CMAKE_REQUIRED_INCLUDES "${LINUX_SRC_DIR}/include")
  unset(HAVE_LINUX_OMAPFB_H CACHE)
  check_include_file("linux/omapfb.h" HAVE_LINUX_OMAPFB_H "-Dsize_t=__u32")
  if(NOT HAVE_LINUX_OMAPFB_H)
      check_include_file("asm-arm/arch-omap/omapfb.h" HAVE_ARCH_OMAP_OMAPFB_H)
      if(NOT HAVE_ARCH_OMAP_OMAPFB_H)
          message(FATAL_ERROR "omapfb.h not found")
      endif()
  endif()
  include_directories(AFTER SYSTEM ${CMAKE_REQUIRED_INCLUDES})
endif()

check_include_file(mtd/mtd-user.h HAVE_MTD_USER_H "-D__user=")
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
configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "${PROJECT_BINARY_DIR}/config.h")
include_directories("${PROJECT_BINARY_DIR}")

# Executables
add_executable(text2screen text2screen.c)
target_link_libraries(text2screen ${Popt_LIBRARY})

add_executable(cal-tool cal-tool.c)
target_link_libraries(cal-tool ${Popt_LIBRARY} cal)

add_executable(key_pressed key_pressed.c)

# Installation
install(TARGETS text2screen cal-tool key_pressed RUNTIME DESTINATION bin)
