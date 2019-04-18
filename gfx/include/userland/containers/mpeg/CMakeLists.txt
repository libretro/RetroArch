# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_ps ${LIBRARY_TYPE} ps_reader.c)

target_link_libraries(reader_ps containers)

install(TARGETS reader_ps DESTINATION ${VMCS_PLUGIN_DIR})
