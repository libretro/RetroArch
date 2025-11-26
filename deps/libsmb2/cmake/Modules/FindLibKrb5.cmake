# - Try to find Krb5 headers and libraries
#
# Usage of this module as follows:
#
#     find_package(LibKrb5)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  LibKrb5_ROOT_DIR         Set this variable to the root installation of
#                            libKrb5 if the module has problems finding the
#                            proper installation path.
#
# Variables defined by this module:
#
#  LibKrb5_FOUND                   System has Krb5 libraries and headers
#  LibKrb5_LIBRARY                 The Krb5 library
#  LibKrb5_INCLUDE_DIR             The location of Krb5 headers

find_path(LibKrb5_ROOT_DIR
    NAMES include/krb5.h
)

find_library(LibKrb5_LIBRARY
    NAMES krb5
    HINTS ${LibKrb5_ROOT_DIR}/lib
)

find_path(LibKrb5_INCLUDE_DIR
    NAMES krb5.h
    HINTS ${LibKrb5_ROOT_DIR}/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibKrb5 DEFAULT_MSG
    LibKrb5_LIBRARY
    LibKrb5_INCLUDE_DIR
)

mark_as_advanced(
    LibKrb5_ROOT_DIR
    LibKrb5_LIBRARY
    LibKrb5_INCLUDE_DIR
)