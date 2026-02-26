#.rst:
# FindSMB2
# -------
# Finds the libsmb2 library
#
# This will will define the following variables::
#
# SMB2_FOUND - system has libsmb2
# SMB2_INCLUDE_DIRS - the libsmb2 include directory
# SMB2_LIBRARIES - the libsmb2 libraries
# SMB2_DEFINITIONS - the libsmb2 compile definitions

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_SMB2 libsmb2 QUIET)
endif()

find_path(SMB2_INCLUDE_DIR smb2/libsmb2.h
                           PATHS ${PC_SMB2_INCLUDEDIR})

set(SMB2_VERSION ${PC_SMB2_VERSION})

include(FindPackageHandleStandardArgs)

find_library(SMB2_LIBRARY NAMES smb2
                          PATHS ${PC_SMB2_LIBDIR})

find_package_handle_standard_args(SMB2
                                  REQUIRED_VARS SMB2_LIBRARY SMB2_INCLUDE_DIR
                                  VERSION_VAR SMB2_VERSION)

if(SMB2_FOUND)
  set(SMB2_LIBRARIES ${SMB2_LIBRARY})
  set(SMB2_INCLUDE_DIRS ${SMB2_INCLUDE_DIR})
  set(SMB2_DEFINITIONS -DHAVE_LIBSMB2=1)
endif()

mark_as_advanced(SMB2_INCLUDE_DIR SMB2_LIBRARY)
