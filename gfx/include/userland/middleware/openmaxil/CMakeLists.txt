# =============================================================================
# Copyright (c) 2012 Broadcom Europe Limited.
# All rights reserved.
#
# FILE DESCRIPTION
# CMake build file for OpenMAX IL.
# =============================================================================

cmake_minimum_required (VERSION 2.8)

if (VIDEOCORE_SIMULATION)

   # VC-simulation build.

   add_library (openmaxil SHARED
 #               core/ilcore.c
                core/ril_top.c
                core/ilmalloc.c
                core/ilvlls.c
                core/ilfifo.c
                core/ilutil.c)

   add_subdirectory (components)

else ()

   # Host build.
   #
   # Provides a host OpenMAX IL core as well as access to OpenMAX IL core and
   # components running on Videocore with the help of the ILCS library.
   #
   # Ideally, we would want the native 64-bit OMX_TICKS datatype but on
   # Videocore, structures are word-aligned, even if they have 64-bit members.
   #
   #    remove_definitions(-DOMX_SKIP64BIT)

   add_definitions (-DVLL_PATH_PREFIX="${VMCS_PLUGIN_DIR}/")

   add_library (openmaxil SHARED
                ../../host_applications/framework/common/ilcore.c
#                core/ilcore.c
#                core/ril_top.c
#                core/ilmalloc.c
#                core/ilvlls.c
#                core/ilfifo.c
                )

   target_link_libraries (openmaxil vcilcs vcos dl)

endif ()

install (TARGETS openmaxil DESTINATION lib)
