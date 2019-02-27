# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_raw_video ${LIBRARY_TYPE} raw_video_reader.c)

target_link_libraries(reader_raw_video containers)

install(TARGETS reader_raw_video DESTINATION ${VMCS_PLUGIN_DIR})

add_library(writer_raw_video ${LIBRARY_TYPE} raw_video_writer.c)

target_link_libraries(writer_raw_video containers)

install(TARGETS writer_raw_video DESTINATION ${VMCS_PLUGIN_DIR})
