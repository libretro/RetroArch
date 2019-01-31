SET( SOURCE_DIR . )

# We support building both static and shared libraries
if (NOT DEFINED LIBRARY_TYPE)
set(LIBRARY_TYPE SHARED)
endif (NOT DEFINED LIBRARY_TYPE)

# Make sure the compiler can find the necessary include files
include_directories (${SOURCE_DIR}/.. ${SOURCE_DIR}/../interface/vcos)

# Needed for the container loader
add_definitions(-DDL_PATH_PREFIX="${VMCS_PLUGIN_DIR}/")

SET( GCC_COMPILER_FLAGS -Wall -g -O2 -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wcast-qual -Wwrite-strings -Wundef )
SET( GCC_COMPILER_FLAGS ${GCC_COMPILER_FLAGS} -Wextra )#-Wno-missing-field-initializers )
SET( GCC_COMPILER_FLAGS ${GCC_COMPILER_FLAGS} -std=c99 -D_POSIX_C_SOURCE=200112L )
SET( GCC_COMPILER_FLAGS ${GCC_COMPILER_FLAGS} -Wno-missing-field-initializers )
SET( GCC_COMPILER_FLAGS ${GCC_COMPILER_FLAGS} -Wno-unused-value )

add_definitions( ${GCC_COMPILER_FLAGS} )

# Containers core library
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_io.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_io_helpers.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_codecs.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_utils.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_writer_utils.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_loader.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_filters.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_logging.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_uri.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_bits.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_list.c)
set(core_SRCS ${core_SRCS} ${SOURCE_DIR}/core/containers_index.c)

# Containers io library
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_file.c)
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_null.c)
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_net.c)
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_pktfile.c)
set(io_SRCS ${io_SRCS} ${SOURCE_DIR}/io/io_http.c)
add_definitions( -DENABLE_CONTAINER_IO_HTTP )

# Containers net library
if (DEFINED MSVC)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_common.c)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_win32.c)
elseif (DEFINED LINUX OR DEFINED UNIX)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_common.c)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_bsd.c)
else (DEFINED MSVC)
set(net_SRCS ${net_SRCS} ${SOURCE_DIR}/net/net_sockets_null.c)
endif (DEFINED MSVC)
set(extra_net_SRCS net_sockets_win32.c net_sockets_win32.h net_sockets_null.c)
add_custom_target(containers_net_extra ALL
    COMMAND touch ${extra_net_SRCS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/containers/net)

# Packetizers library
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/core/packetizers.c)
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/mpga/mpga_packetizer.c)
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/mpgv/mpgv_packetizer.c)
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/pcm/pcm_packetizer.c)
set(packetizers_SRCS ${packetizers_SRCS} ${SOURCE_DIR}/h264/avc1_packetizer.c)

add_library(containers ${LIBRARY_TYPE} ${core_SRCS} ${io_SRCS} ${net_SRCS} ${packetizers_SRCS})
target_link_libraries(containers vcos)
install(TARGETS containers DESTINATION lib)

set(container_readers)
set(container_writers)

# Container modules
add_subdirectory(mp4)
set(container_readers ${container_readers} reader_mp4)
set(container_writers ${container_writers} writer_mp4)
add_subdirectory(mpeg)
set(container_readers ${container_readers} reader_ps)
add_subdirectory(mpga)
set(container_readers ${container_readers} reader_mpga)
add_subdirectory(binary)
set(container_readers ${container_readers} reader_binary)
set(container_writers ${container_writers} writer_binary)
add_subdirectory(mkv)
set(container_readers ${container_readers} reader_mkv)
add_subdirectory(wav)
set(container_readers ${container_readers} reader_wav)
add_subdirectory(asf)
set(container_readers ${container_readers} reader_asf)
set(container_writers ${container_writers} writer_asf)
add_subdirectory(flash)
set(container_readers ${container_readers} reader_flv)
add_subdirectory(avi)
set(container_readers ${container_readers} reader_avi)
set(container_writers ${container_writers} writer_avi)
add_subdirectory(rtp)
set(container_readers ${container_readers} reader_rtp)
add_subdirectory(rtsp)
set(container_readers ${container_readers} reader_rtsp)
add_subdirectory(rcv)
set(container_readers ${container_readers} reader_rcv)
add_subdirectory(rv9)
set(container_readers ${container_readers} reader_rv9)
add_subdirectory(qsynth)
set(container_readers ${container_readers} reader_qsynth)
add_subdirectory(simple)
set(container_readers ${container_readers} reader_simple)
set(container_writers ${container_writers} writer_simple)
add_subdirectory(raw)
set(container_readers ${container_readers} reader_raw_video)
set(container_writers ${container_writers} writer_raw_video)
add_subdirectory(dummy)
set(container_writers ${container_writers} writer_dummy)

add_subdirectory(metadata/id3)
set(container_readers ${container_readers} reader_metadata_id3)

if (${LIBRARY_TYPE} STREQUAL STATIC)
target_link_libraries(containers ${container_readers} ${container_writers})
endif (${LIBRARY_TYPE} STREQUAL STATIC)

# Test apps
add_subdirectory(test)
