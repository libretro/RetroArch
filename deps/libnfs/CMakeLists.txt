cmake_minimum_required(VERSION 3.16)

project(libnfs
        LANGUAGES C
        VERSION 16.2.0)

set(SOVERSION 16.2.0 CACHE STRING "" FORCE)

include(cmake/GNUInstallDirs.cmake)

set(INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for binaries")
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_FULL_LIBDIR}" CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")
set(INSTALL_MAN_DIR "${CMAKE_INSTALL_PREFIX}/share/man" CACHE PATH "Installation directory for manual pages")
set(INSTALL_PKGCONFIG_DIR "${INSTALL_LIB_DIR}/pkgconfig" CACHE PATH "Installation directory for pkgconfig (.pc) files")
set(INSTALL_CMAKE_DIR "${INSTALL_LIB_DIR}/cmake/libnfs" CACHE PATH "Installation directory for cmake (.cmake) files")

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_TESTS "Build and run test programs" OFF)
option(ENABLE_DOCUMENTATION "Build Documentation" OFF)
option(ENABLE_UTILS "Build util programs" OFF)
option(ENABLE_EXAMPLES "Build example programs" OFF)
option(ENABLE_MULTITHREADING "Enable multithreading support" OFF)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(CMAKE_INSTALL_RPATH "${INSTALL_LIB_DIR}")
  set(CMAKE_SKIP_INSTALL_RPATH FALSE)
endif()

if(ENABLE_TESTS)
  set(ENABLE_UTILS ON CACHE BOOL "Building utils required by tests" FORCE)
endif()

if(ENABLE_MULTITHREADING)
  add_definitions(-DHAVE_MULTITHREADING)
endif()

include(cmake/Macros.cmake)

if(IOS)
    find_package(GSSAPI)

    if(GSSAPI_FOUND)
        add_definitions(-DHAVE_LIBKRB5)
    endif()
endif()

include(cmake/ConfigureChecks.cmake)

include_directories(${CMAKE_CURRENT_BINARY_DIR}
                    include
                    include/nfsc)

# list of system libs (sockets/etc) to be linked on this system
set(SYSTEM_LIBRARIES "")
# list of core (static) libraries built in current configuration (used by lib/CMakeLists.txt)
set(CORE_LIBRARIES "" CACHE INTERNAL "")

# make sure related functions are exported in final dll
if(WIN32 AND BUILD_SHARED_LIBS)
  add_definitions(-Dlibnfs_EXPORTS)
endif()

if(ENABLE_MULTITHREADING AND PTHREAD_LIBRARY)
    list(APPEND SYSTEM_LIBRARIES pthread)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  add_definitions("-D_U_=__attribute__((unused))")
  #
  # Currently RPC-with-TLS support is only available on Linux since it depends on kTLS support
  # on Linux.
  #
  # TODO: BSD also has kTLS support, but that will need separate validation.
  #
  find_package(GnuTLS "3.4.6")
  if(GNUTLS_FOUND)
    #
    # Make sure the two most important header files are present before we enable TLS support,
    # to avoid running into issues later during build. GnuTLS package found but gnutls/gnutls.h
    # not found is a serious issue while if linux/tls.h is not found it would likely mean that
    # user is using a kernel not supporting kTLS so we simply don't turn on TLS support.
    #
    check_include_file("gnutls/gnutls.h" HAVE_GNUTLS_H)
    if(NOT HAVE_GNUTLS_H EQUAL "1")
      message(FATAL_ERROR "GnuTLS found but gnutls/gnutls.h not found, GNUTLS_INCLUDE_DIR is ${GNUTLS_INCLUDE_DIR}")
    endif()

    check_include_file("linux/tls.h" HAVE_LINUX_TLS_H)
    if(NOT HAVE_LINUX_TLS_H EQUAL "1")
      message(STATUS "GnuTLS found but linux/tls.h not found, likely a kernel w/o kTLS support, can't enable TLS support")
    else()
      message(STATUS "Using ${GNUTLS_LIBRARIES}")
      add_definitions(-DHAVE_TLS)
      list(APPEND SYSTEM_LIBRARIES ${GNUTLS_LIBRARIES})
      add_subdirectory(tls)
    endif()
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL Windows OR CMAKE_SYSTEM_NAME STREQUAL WindowsStore)
  add_definitions("-D_U_=" -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
  list(APPEND SYSTEM_LIBRARIES ws2_32)
  add_subdirectory(win32)
elseif(CMAKE_SYSTEM_NAME STREQUAL Solaris)
  add_definitions("-D_U_=__attribute__((unused))")
  find_library(SOCKET_LIBRARY socket)
  find_library(NSL_LIBRARY nsl)
  list(APPEND SYSTEM_LIBRARIES ${SOCKET_LIBRARY} ${NSL_LIBRARY})
elseif(CMAKE_SYSTEM_NAME STREQUAL aros)
  add_definitions("-D_U_=__attribute__((unused))")
  add_definitions(-DAROS)
  add_subdirectory(aros)
else()
  add_definitions("-D_U_=__attribute__((unused))")
endif()

add_subdirectory(mount)
add_subdirectory(nfs)
add_subdirectory(nfs4)
add_subdirectory(nlm)
add_subdirectory(nsm)
add_subdirectory(portmap)
add_subdirectory(rquota)
add_subdirectory(lib)   # this has to be last (it links all static libs mentioned in CORE_LIBRARIES)

if(ENABLE_DOCUMENTATION)
  add_subdirectory(doc)
endif()

if(ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif()

if(ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(ENABLE_UTILS)
  add_subdirectory(utils)
endif()

# this will create build-tree-specific config file (so downstream client can use this specific build without having to install package)
export(EXPORT libnfs
       NAMESPACE libnfs::
       FILE "${CMAKE_CURRENT_BINARY_DIR}/libnfs-config.cmake")

# this will create relocatable config file in installation directory
install(EXPORT libnfs
        DESTINATION "${INSTALL_CMAKE_DIR}"
        NAMESPACE libnfs::
        FILE libnfs-config.cmake)

# handle version file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/libnfs-config-version.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY AnyNewerVersion)

install(FILES cmake/FindNFS.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/libnfs-config-version.cmake
        DESTINATION ${INSTALL_CMAKE_DIR})

# handle pc-config files
set(PKG_LIBLIST "")
foreach(LIB ${SYSTEM_LIBRARIES})
  if(IS_ABSOLUTE ${LIB} AND EXISTS ${LIB})
    list(APPEND PKG_LIBLIST "${LIB}")
  else()
    list(APPEND PKG_LIBLIST "-l${LIB}")
  endif()
endforeach()
configure_file(cmake/libnfs.pc.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/libnfs.pc @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libnfs.pc
        DESTINATION ${INSTALL_PKGCONFIG_DIR})

# handle headers installation
install(DIRECTORY include/nfsc
        DESTINATION ${INSTALL_INC_DIR})

install(FILES mount/libnfs-raw-mount.h
              nfs/libnfs-raw-nfs.h
              nfs4/libnfs-raw-nfs4.h
              nlm/libnfs-raw-nlm.h
              nsm/libnfs-raw-nsm.h
              portmap/libnfs-raw-portmap.h
              rquota/libnfs-raw-rquota.h
        DESTINATION ${INSTALL_INC_DIR}/nfsc)
