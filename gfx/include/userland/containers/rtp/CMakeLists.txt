# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

set(rtp_SRCS ${rtp_SRCS} rtp_reader.c)
set(rtp_SRCS ${rtp_SRCS} rtp_h264.c)
set(rtp_SRCS ${rtp_SRCS} rtp_mpeg4.c)
set(rtp_SRCS ${rtp_SRCS} rtp_base64.c)
add_library(reader_rtp ${LIBRARY_TYPE} ${rtp_SRCS})

target_link_libraries(reader_rtp containers)

install(TARGETS reader_rtp DESTINATION ${VMCS_PLUGIN_DIR})
