# Container module needs to go in as a plugins so different prefix
# and install path
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Make sure the compiler can find the necessary include files
include_directories (../..)

add_library(reader_wav ${LIBRARY_TYPE} wav_reader.c)

target_link_libraries(reader_wav containers)

install(TARGETS reader_wav DESTINATION ${VMCS_PLUGIN_DIR})

