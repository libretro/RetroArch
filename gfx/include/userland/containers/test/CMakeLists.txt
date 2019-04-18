# Generate test application
add_executable(containers_test test.c)
target_link_libraries(containers_test -Wl,--no-whole-archive containers)
install(TARGETS containers_test DESTINATION bin)

# Generate test application
add_executable(containers_check_frame_int check_frame_int.c)
target_link_libraries(containers_check_frame_int -Wl,--no-whole-archive containers)
install(TARGETS containers_check_frame_int DESTINATION bin)

# Generate autotest application
#add_executable(containers_autotest autotest.cpp crc_32.c)
#target_link_libraries(containers_autotest -Wl,--no-whole-archive containers})
#install(TARGETS containers_autotest DESTINATION bin)

# Helper code to provide non-blocking console input
if (WIN32)
set( NB_IO_SOURCE nb_io_win32.c )
elseif (UNIX)
set( NB_IO_SOURCE nb_io_unix.c )
endif (WIN32)
set(extra_test_SRCS nb_io_win32.c autotest.cpp crc_32.c)
add_custom_target(containers_test_extra
    COMMAND touch ${extra_test_SRCS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/containers/test)
add_dependencies(containers_test containers_test_extra)

# Generate net test applications
add_executable(containers_stream_client stream_client.c ${NB_IO_SOURCE})
target_link_libraries(containers_stream_client containers)
install(TARGETS containers_stream_client DESTINATION bin)

add_executable(containers_stream_server stream_server.c)
target_link_libraries(containers_stream_server containers)
install(TARGETS containers_stream_server DESTINATION bin)

add_executable(containers_datagram_sender datagram_sender.c)
target_link_libraries(containers_datagram_sender containers)
install(TARGETS containers_datagram_sender DESTINATION bin)

add_executable(containers_datagram_receiver datagram_receiver.c)
target_link_libraries(containers_datagram_receiver containers)
install(TARGETS containers_datagram_receiver DESTINATION bin)

add_executable(containers_rtp_decoder rtp_decoder.c ${NB_IO_SOURCE})
target_link_libraries(containers_rtp_decoder containers)
install(TARGETS containers_rtp_decoder DESTINATION bin)

# Generate URI test application
add_executable(containers_test_uri test_uri.c)
target_link_libraries(containers_test_uri containers)
install(TARGETS containers_test_uri DESTINATION bin)

# Generate URI pipe application
add_executable(containers_uri_pipe uri_pipe.c ${NB_IO_SOURCE})
target_link_libraries(containers_uri_pipe containers)
install(TARGETS containers_uri_pipe DESTINATION bin)

# Generate bit stream test application
add_executable(containers_test_bits test_bits.c)
target_link_libraries(containers_test_bits containers)
install(TARGETS containers_test_bits DESTINATION bin)

# Generate packet file dump application
add_executable(containers_dump_pktfile dump_pktfile.c)
install(TARGETS containers_dump_pktfile DESTINATION bin)
