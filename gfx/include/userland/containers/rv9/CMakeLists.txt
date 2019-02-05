# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_rv9 ${LIBRARY_TYPE} rv9_reader.c)

target_link_libraries(reader_rv9 containers)

install(TARGETS reader_rv9 DESTINATION ${VMCS_PLUGIN_DIR})
