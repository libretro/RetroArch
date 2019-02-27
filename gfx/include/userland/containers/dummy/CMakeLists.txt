# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(writer_dummy ${LIBRARY_TYPE} dummy_writer.c)

target_link_libraries(writer_dummy containers)

install(TARGETS writer_dummy DESTINATION ${VMCS_PLUGIN_DIR})
