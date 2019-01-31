# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_qsynth ${LIBRARY_TYPE} qsynth_reader.c)

target_link_libraries(reader_qsynth containers)

install(TARGETS reader_qsynth DESTINATION ${VMCS_PLUGIN_DIR})

