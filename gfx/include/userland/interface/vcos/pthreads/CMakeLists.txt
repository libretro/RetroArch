# MSVC5 does not fully support C99, enabling declaration-after-statement
# warnings allows a common MSVC5 build error to be detected in Linux builds.
if (CMAKE_COMPILER_IS_GNUCC)
   set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wdeclaration-after-statement")
   add_definitions (-D_GNU_SOURCE)
endif ()

set (HEADERS
   vcos_platform.h
   vcos_platform_types.h
)

foreach (header ${HEADERS})
   configure_file ("${header}" "${VCOS_HEADERS_BUILD_DIR}/${header}" COPYONLY)
endforeach ()

add_subdirectory (../generic generic)

set (SOURCES
   vcos_pthreads.c
   vcos_dlfcn.c
   ../glibc/vcos_backtrace.c
   ../generic/vcos_generic_event_flags.c
   ../generic/vcos_mem_from_malloc.c
   ../generic/vcos_generic_named_sem.c
   ../generic/vcos_generic_safe_string.c
   ../generic/vcos_generic_reentrant_mtx.c
   ../generic/vcos_abort.c
   ../generic/vcos_cmd.c
   ../generic/vcos_init.c
   ../generic/vcos_msgqueue.c
   ../generic/vcos_logcat.c
   ../generic/vcos_generic_blockpool.c
)

if (VCOS_PTHREADS_BUILD_SHARED)
   add_library (vcos SHARED ${SOURCES})
   target_link_libraries (vcos pthread dl rt)
else ()
   add_library (vcos ${SOURCES})
   target_link_libraries (vcos pthread rt)
endif ()

#install(FILES ${HEADERS} DESTINATION include)
install(TARGETS vcos DESTINATION lib)
