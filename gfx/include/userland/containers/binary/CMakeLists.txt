# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_binary ${LIBRARY_TYPE} binary_reader.c)

target_link_libraries(reader_binary containers)

install(TARGETS reader_binary DESTINATION ${VMCS_PLUGIN_DIR})

add_library(writer_binary ${LIBRARY_TYPE} binary_writer.c)

target_link_libraries(writer_binary containers)

install(TARGETS writer_binary DESTINATION ${VMCS_PLUGIN_DIR})

