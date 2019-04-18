# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_mp4 ${LIBRARY_TYPE} mp4_reader.c)

target_link_libraries(reader_mp4 containers)

install(TARGETS reader_mp4 DESTINATION ${VMCS_PLUGIN_DIR})

add_library(writer_mp4 ${LIBRARY_TYPE} mp4_writer.c)

target_link_libraries(writer_mp4 containers)

install(TARGETS writer_mp4 DESTINATION ${VMCS_PLUGIN_DIR})
