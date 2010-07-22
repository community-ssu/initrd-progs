# Executables

add_library(opencal SHARED crc32.c libopencal.c)
set_target_properties(opencal PROPERTIES
    SOVERSION 0
    VERSION 0)

add_executable(open-wlan-cal open-wlan-cal.c)
target_link_libraries(open-wlan-cal ${Popt_LIBRARY} opencal)

add_executable(open-cal-tool open-cal-tool.c)
target_link_libraries(open-cal-tool ${Popt_LIBRARY} opencal)

# Installation
install(TARGETS
    opencal
    LIBRARY DESTINATION lib)
install(TARGETS
    open-cal-tool open-wlan-cal
    RUNTIME DESTINATION sbin)
