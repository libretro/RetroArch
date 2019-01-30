cmake_minimum_required(VERSION 2.8)

get_filename_component (VIDEOCORE_ROOT ../../../.. ABSOLUTE)
include (${VIDEOCORE_ROOT}/makefiles/cmake/global_settings.cmake)

if (NOT WIN32)
   add_definitions(-Wall -Werror)
endif ()

include_directories (
   ${VIDEOCORE_HEADERS_BUILD_DIR}
   ${VIDEOCORE_ROOT}
   ${VIDEOCORE_ROOT}/host_applications/linux/libs/sm
)

add_executable(vcsmem smem.c)
target_link_libraries(vcsmem vcos vcsm vchostif)

install(TARGETS vcsmem RUNTIME DESTINATION bin)

