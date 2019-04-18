# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_metadata_id3 ${LIBRARY_TYPE} id3_metadata_reader.c)

target_link_libraries(reader_metadata_id3 containers)

install(TARGETS reader_metadata_id3 DESTINATION ${VMCS_PLUGIN_DIR})
