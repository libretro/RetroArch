
SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "0")
SET(CPACK_PACKAGE_VERSION_PATCH "pre-1")

INCLUDE(CPack)

# Where shall we install?
if (ANDROID)
  SET(VMCS_INSTALL_PREFIX "/vendor/brcm/islands" CACHE PATH "Prefix prepended to install directories" FORCE)
elseif(NOT DEFINED VMCS_INSTALL_PREFIX)
  SET(VMCS_INSTALL_PREFIX "/opt/vc" CACHE PATH "Prefix prepended to install directories" FORCE)
endif()

SET(CMAKE_INSTALL_PREFIX "${VMCS_INSTALL_PREFIX}" CACHE INTERNAL "Prefix
    prepended to install directories" FORCE)
if(NOT DEFINED VMCS_PLUGIN_DIR)
  SET(VMCS_PLUGIN_DIR ${CMAKE_INSTALL_PREFIX}/${CMAKE_SHARED_LIBRARY_PREFIX}/plugins)
endif()

# What kind of system are we?
if (${UNIX})
   set (VMCS_TARGET linux)
elseif (${SYMBIAN})
   set (VMCS_TARGET symbian)
elseif (${WIN32})
   set (VMCS_TARGET win32)
else()
   message(FATAL_ERROR,"Unknown system type")
endif()
set (ARM64 OFF CACHE BOOL "Whether target is ARM64")

# construct the vmcs config header file
add_definitions(-DHAVE_VMCS_CONFIG)
configure_file (
    "${vmcs_root}/host_applications/vmcs/vmcs_config.h.in"
    "${PROJECT_BINARY_DIR}/vmcs_config.h"
    )

# install an ld.so.conf file to pick up our shared libraries
#configure_file (${vmcs_root}/makefiles/cmake/srcs/vmcs.conf.in
#                ${PROJECT_BINARY_DIR}/vmcs.conf)
#if(NOT DEFINED ANDROID)
#   install(FILES ${PROJECT_BINARY_DIR}/vmcs.conf DESTINATION /etc/ld.so.conf.d)
#endif()

# also put it in /opt/vc for access by install script
#install(FILES  ${PROJECT_BINARY_DIR}/vmcs.conf
#        DESTINATION ${VMCS_INSTALL_PREFIX}/share/install)
# provide headers the libraries need in /opt/vc too
#install(DIRECTORY interface/khronos/include
#        DESTINATION ${VMCS_INSTALL_PREFIX})
#install(DIRECTORY interface/vmcs_host/khronos/IL
#        DESTINATION ${VMCS_INSTALL_PREFIX}/include)
# provide an install script
#install(PROGRAMS ${vmcs_root}/makefiles/cmake/scripts/install_vmcs
#        DESTINATION ${VMCS_INSTALL_PREFIX}/sbin
#        PERMISSIONS OWNER_WRITE WORLD_READ)

# provide hello_pi demos
install(DIRECTORY host_applications/linux/apps/hello_pi
        DESTINATION ${VMCS_INSTALL_PREFIX}/src)

# provide header files
#install(DIRECTORY host_applications/linux/libs/bcm_host/include
#        DESTINATION ${VMCS_INSTALL_PREFIX}/)

install(DIRECTORY ${vmcs_root}/interface/vcos      DESTINATION ${VMCS_INSTALL_PREFIX}/include/interface FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${vmcs_root}/interface/vchiq_arm DESTINATION ${VMCS_INSTALL_PREFIX}/include/interface FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${vmcs_root}/interface/vchi      DESTINATION ${VMCS_INSTALL_PREFIX}/include/interface FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${vmcs_root}/interface/vctypes   DESTINATION ${VMCS_INSTALL_PREFIX}/include/interface FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${vmcs_root}/vcinclude           DESTINATION ${VMCS_INSTALL_PREFIX}/include           FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${vmcs_root}/interface/peer      DESTINATION ${VMCS_INSTALL_PREFIX}/include/interface FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${vmcs_root}/interface/vmcs_host DESTINATION ${VMCS_INSTALL_PREFIX}/include/interface FILES_MATCHING PATTERN "*.h" PATTERN "${vmcs_root}/interface/vmcs_host/khronos" EXCLUDE)

install(DIRECTORY ${vmcs_root}/interface/khronos/include       DESTINATION ${VMCS_INSTALL_PREFIX}     FILES_MATCHING PATTERN "*.h")
install(DIRECTORY ${vmcs_root}/interface/vmcs_host/khronos/IL  DESTINATION ${VMCS_INSTALL_PREFIX}/include     FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${vmcs_root}/host_applications/linux/libs/bcm_host/include     DESTINATION ${VMCS_INSTALL_PREFIX}       FILES_MATCHING PATTERN "*.h")
