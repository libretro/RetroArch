cmake_minimum_required (VERSION 2.8)

get_filename_component (VIDEOCORE_ROOT "../.." ABSOLUTE)
include (${VIDEOCORE_ROOT}/makefiles/cmake/global_settings.cmake)

set (HEADERS
   vcos_assert.h
   vcos_atomic_flags.h
   vcos_blockpool.h
   vcos_cmd.h
   vcos_dlfcn.h
   vcos_event_flags.h
   vcos_event.h
   vcos.h
   vcos_init.h
   vcos_inttypes.h
   vcos_isr.h
   vcos_legacy_isr.h
   vcos_logging.h
   vcos_logging_control.h
   vcos_lowlevel_thread.h
   vcos_mem.h
   vcos_mempool.h
   vcos_msgqueue.h
   vcos_mutex.h
   vcos_named_semaphore.h
   vcos_once.h
   vcos_queue.h
   vcos_quickslow_mutex.h
   vcos_reentrant_mutex.h
   vcos_semaphore.h
   vcos_stdint.h
   vcos_string.h
   vcos_thread_attr.h
   vcos_thread.h
   vcos_timer.h
   vcos_tls.h
   vcos_types.h
)

foreach (header ${HEADERS})
   configure_file ("${header}" "${VCOS_HEADERS_BUILD_DIR}/${header}" COPYONLY)
endforeach ()

if (CMAKE_COMPILER_IS_GNUCC)
   add_definitions (-ggdb -Werror -Wall)
endif ()

if (CMAKE_COMPILER_2005)
   add_definitions (/WX /W4 /wd4127 /D_CRT_SECURE_NO_DEPRECATE)
endif ()

include_directories (${VIDEOCORE_ROOT} ${VCOS_HEADERS_BUILD_DIR})

add_subdirectory (${RTOS})

set(VCOS_EXCLUDE_TESTS TRUE)
if (NOT DEFINED VCOS_EXCLUDE_TESTS)
add_testapp_subdirectory (test)
endif (NOT DEFINED VCOS_EXCLUDE_TESTS)

if (WIN32)
   build_command (RELEASE_BUILD_CMD CONFIGURATION Release)
   build_command (DEBUG_BUILD_CMD CONFIGURATION Debug)
   configure_file (build_all.bat.in build_all.bat @ONLY)
endif ()

#install (FILES ${HEADERS} DESTINATION include/interface/vcos)
