
set (HEADERS
   vcos_common.h
   vcos_generic_blockpool.h
   vcos_generic_event_flags.h
   vcos_generic_named_sem.h
   vcos_generic_quickslow_mutex.h
   vcos_generic_reentrant_mtx.h
   vcos_generic_tls.h
   vcos_joinable_thread_from_plain.h
   vcos_latch_from_sem.h
   vcos_mem_from_malloc.h
   vcos_mutexes_are_reentrant.h
   vcos_thread_reaper.h
)

foreach (header ${HEADERS})
   configure_file ("${header}" "${VCOS_HEADERS_BUILD_DIR}/generic/${header}" COPYONLY)
endforeach ()

install (FILES ${HEADERS} DESTINATION include/interface/vcos/generic)
