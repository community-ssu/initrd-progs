# Dependencies
find_package(Popt REQUIRED)
find_package(ZLIB REQUIRED)

# Checks
set(CMAKE_REQUIRED_FLAGS "-std=c99 -Wall -Wextra -pedantic")
include(CheckIncludeFiles)
check_include_files(mtd/mtd-user.h HAVE_MTD_USER_H)
if(!${HAVE_MTD_USER_H})
	message(FATAL_ERROR "<mtd/mtd-user.h> can't be included")
endif()

# config.h
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/config.h")

# Executables
include_directories(
	"${PROJECT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_BINARY_DIR}")
add_definitions(${CMAKE_REQUIRED_FLAGS} -D_FORTIFY_SOURCE=2)

add_library(opencal SHARED libopencal.c)
target_link_libraries(opencal ${ZLIB_LIBRARY})
add_executable(open-wlan-cal open-wlan-cal.c)
target_link_libraries(open-wlan-cal ${Popt_LIBRARY} opencal)
add_executable(open-cal-tool open-cal-tool.c)
target_link_libraries(open-cal-tool ${Popt_LIBRARY} opencal)

# Installation
install(TARGETS
	open-cal-tool open-wlan-cal
	RUNTIME DESTINATION sbin)
