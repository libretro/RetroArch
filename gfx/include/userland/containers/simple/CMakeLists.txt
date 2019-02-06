# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_simple ${LIBRARY_TYPE} simple_reader.c)

target_link_libraries(reader_simple containers)

install(TARGETS reader_simple DESTINATION ${VMCS_PLUGIN_DIR})

add_library(writer_simple ${LIBRARY_TYPE} simple_writer.c)

target_link_libraries(writer_simple containers)

install(TARGETS writer_simple DESTINATION ${VMCS_PLUGIN_DIR})
