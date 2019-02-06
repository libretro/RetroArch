# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_asf ${LIBRARY_TYPE} asf_reader.c)

target_link_libraries(reader_asf containers)

install(TARGETS reader_asf DESTINATION ${VMCS_PLUGIN_DIR})

add_library(writer_asf ${LIBRARY_TYPE} asf_writer.c)

target_link_libraries(writer_asf containers)

install(TARGETS writer_asf DESTINATION ${VMCS_PLUGIN_DIR})
