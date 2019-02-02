# Run this file only once per cmake run. This is so that
# users projects can override those global settings before
# parsing subdirectories (which can also include this file
# directly so that they can be built as standalone projects).
if (DEFINED GLOBAL_SETTINGS_SET)
   return ()
endif ()
set (GLOBAL_SETTINGS_SET "TRUE")

if (NOT DEFINED VIDEOCORE_ROOT)
   message (FATAL_ERROR
      " **************************************************\n"
      " * Variable VIDEOCORE_ROOT is not defined. please *\n"
      " * define it before including this cmake file.    *\n"
      " * this variable has to be set to the *absolute*  *\n"
      " * path to the top of the videocore source tree   *\n"
      " **************************************************\n"
   )
endif ()

if (NOT IS_ABSOLUTE "${VIDEOCORE_ROOT}")
   message (FATAL_ERROR
      " **************************************************\n"
      " * Variable VIDEOCORE_ROOT doesn't contain        *\n"
      " * absolute path                                  *\n"
      " **************************************************\n"
   )
endif ()

set (VIDEOCORE_RTOSES threadx none win32 pthreads nucleus)
foreach (possible_rtos ${VIDEOCORE_RTOSES})
   set (VIDEOCORE_RTOSES_STR "${possible_rtos}, ${VIDEOCORE_RTOSES_STR}")
   set (VIDEOCORE_RTOSES_RE "${VIDEOCORE_RTOSES_RE}|(${possible_rtos})")
endforeach ()

if (NOT DEFINED RTOS)
   # Guess which OS we are on to maintain backwards compatibility
   if (WIN32)
      set (RTOS win32)
   else ()
      set (RTOS pthreads)
   endif ()
endif ()

if (NOT ${RTOS} MATCHES ${VIDEOCORE_RTOSES_RE})
   message (FATAL_ERROR
      " **************************************************\n"
      " * RTOS incorrectly defined. Please define it     *\n"
      " * correctly and rerun cmake (possibly after      *\n"
      " * removing files from this run).                 *\n"
      " * Possible options are: ${VIDEOCORE_RTOSES_STR}\n"
      " **************************************************\n"
   )
endif ()

if (NOT DEFINED VIDEOCORE_BUILD_DIR)
   set (VIDEOCORE_BUILD_DIR "${VIDEOCORE_ROOT}/build")
endif ()
set (VIDEOCORE_ARCHIVE_BUILD_DIR "${VIDEOCORE_BUILD_DIR}/lib")
set (VIDEOCORE_HEADERS_BUILD_DIR "${VIDEOCORE_BUILD_DIR}/inc")
set (VIDEOCORE_RUNTIME_BUILD_DIR "${VIDEOCORE_BUILD_DIR}/bin")
if (VIDEOCORE_ALL_RUNTIMES_AND_LIBRARIES_IN_SAME_DIRECTORY)
   # this is useful on windows to avoid problems with dlls not being found at
   # runtime...
   set (VIDEOCORE_LIBRARY_BUILD_DIR "${VIDEOCORE_BUILD_DIR}/bin")
   set (VIDEOCORE_TESTAPP_BUILD_DIR "${VIDEOCORE_BUILD_DIR}/bin")
else ()
   set (VIDEOCORE_LIBRARY_BUILD_DIR "${VIDEOCORE_BUILD_DIR}/lib")
   set (VIDEOCORE_TESTAPP_BUILD_DIR "${VIDEOCORE_BUILD_DIR}/test")
endif ()

set (VCOS_HEADERS_BUILD_DIR "${VIDEOCORE_HEADERS_BUILD_DIR}/interface/vcos")

set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${VIDEOCORE_ARCHIVE_BUILD_DIR}")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${VIDEOCORE_LIBRARY_BUILD_DIR}")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${VIDEOCORE_RUNTIME_BUILD_DIR}")

include_directories ("${VIDEOCORE_HEADERS_BUILD_DIR}")

function (add_testapp_subdirectory name)
   set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${VIDEOCORE_TESTAPP_BUILD_DIR}")
   add_subdirectory (${name})
endfunction ()
