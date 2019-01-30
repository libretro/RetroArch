# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

set(rtsp_SRCS ${rtsp_SRCS} rtsp_reader.c)
add_library(reader_rtsp ${LIBRARY_TYPE} ${rtsp_SRCS})

target_link_libraries(reader_rtsp containers)

install(TARGETS reader_rtsp DESTINATION ${VMCS_PLUGIN_DIR})

