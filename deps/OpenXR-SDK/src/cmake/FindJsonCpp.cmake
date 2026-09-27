# - Find jsoncpp - Overarching find module
# This is a over-arching find module to find older jsoncpp versions and those sadly built
# without JSONCPP_WITH_CMAKE_PACKAGE=ON, as well as those built with the cmake config file.
# It also wraps the different versions of the module.
#
# On CMake 3.0 and newer:
#  JsonCpp::JsonCpp - Imported target (possibly an interface/alias) to use:
#  if anything is populated, this is. If both shared and static are found, then
#  this will be the static version on DLL platforms and shared on non-DLL platforms.
#  JsonCpp::JsonCppShared - Imported target (possibly an interface/alias) for a
#  shared library version.
#  JsonCpp::JsonCppStatic - Imported target (possibly an interface/alias) for a
#  static library version.
#
# On all CMake versions: (Note that on CMake 2.8.10 and earlier, you may need to use JSONCPP_INCLUDE_DIRS)
#  JSONCPP_LIBRARY - wraps JsonCpp::JsonCpp or equiv.
#  JSONCPP_LIBRARY_IS_SHARED - if we know for sure JSONCPP_LIBRARY is shared, this is true-ish. We try to "un-set" it if we don't know one way or another.
#  JSONCPP_LIBRARY_SHARED - wraps JsonCpp::JsonCppShared or equiv.
#  JSONCPP_LIBRARY_STATIC - wraps JsonCpp::JsonCppStatic or equiv.
#  JSONCPP_INCLUDE_DIRS - Include directories - should (generally?) not needed if you require CMake 2.8.11+ since it handles target include directories.
#
#  JSONCPP_FOUND - True if JsonCpp was found.
#
# Original Author:
# 2016 Rylie Pavlik <rylie@ryliepavlik.com>
# Incorporates work from the module contributed to VRPN under the same license:
# 2011 Philippe Crassous (ENSAM ParisTech / Institut Image) p.crassous _at_ free.fr
#
# Copyright 2011, Philippe Crassous
# Copyright 2016, Sensics, Inc.
#
# SPDX-License-Identifier: BSL-1.0
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

set(__jsoncpp_have_namespaced_targets OFF)
set(__jsoncpp_have_interface_support OFF)
if(NOT ("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" LESS 3.0))
	set(__jsoncpp_have_namespaced_targets ON)
	set(__jsoncpp_have_interface_support ON)
elseif(("${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}" EQUAL 2.8) AND "${CMAKE_PATCH_VERSION}" GREATER 10)
	set(__jsoncpp_have_interface_support ON)
endif()

# sets __jsoncpp_have_jsoncpplib based on whether or not we have a real imported jsoncpp_lib target.
macro(_jsoncpp_check_for_real_jsoncpplib)
	set(__jsoncpp_have_jsoncpplib FALSE)
	if(TARGET jsoncpp_lib)
		get_property(__jsoncpp_lib_type TARGET jsoncpp_lib PROPERTY TYPE)
		# We make interface libraries. If an actual config module made it, it would be an imported library.
		if(NOT __jsoncpp_lib_type STREQUAL "INTERFACE_LIBRARY")
			set(__jsoncpp_have_jsoncpplib TRUE)
		endif()
	endif()
	#message(STATUS "__jsoncpp_have_jsoncpplib ${__jsoncpp_have_jsoncpplib}")
endmacro()

include(FindPackageHandleStandardArgs)
# Ensure that if this is TRUE later, it's because we set it.
set(JSONCPP_FOUND FALSE)
set(__jsoncpp_have_jsoncpplib FALSE)

# See if we find a CMake config file - there is no harm in calling this more than once,
# and we need to call it at least once every CMake invocation to create the original
# imported targets, since those don't stick around like cache variables.
find_package(jsoncpp QUIET NO_MODULE)

if(jsoncpp_FOUND)
	# Build a string to help us figure out when to invalidate our cache variables.
	# start with where we found jsoncpp
	set(__jsoncpp_info_string "[${jsoncpp_DIR}]")

	# part of the string to indicate if we found a real jsoncpp_lib (and what kind)
	_jsoncpp_check_for_real_jsoncpplib()

	macro(_jsoncpp_apply_map_config target)
		if(MSVC)
			# Can't do this - different runtimes, incompatible ABI, etc.
			set(_jsoncpp_debug_fallback)
		else()
			set(_jsoncpp_debug_fallback DEBUG)
			#osvr_stash_map_config(DEBUG DEBUG RELWITHDEBINFO RELEASE MINSIZEREL NONE)
		endif()
		# Appending, just in case using project or upstream fixes this.
		set_property(TARGET ${target} APPEND PROPERTY MAP_IMPORTED_CONFIG_RELEASE RELEASE RELWITHDEBINFO MINSIZEREL NONE ${_jsoncpp_debug_fallback})
		set_property(TARGET ${target} APPEND PROPERTY MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELWITHDEBINFO RELEASE MINSIZEREL NONE ${_jsoncpp_debug_fallback})
		set_property(TARGET ${target} APPEND PROPERTY MAP_IMPORTED_CONFIG_MINSIZEREL MINSIZEREL RELEASE RELWITHDEBINFO NONE ${_jsoncpp_debug_fallback})
		set_property(TARGET ${target} APPEND PROPERTY MAP_IMPORTED_CONFIG_NONE NONE RELEASE RELWITHDEBINFO MINSIZEREL ${_jsoncpp_debug_fallback})
		if(NOT MSVC)
			set_property(TARGET ${target} APPEND PROPERTY MAP_IMPORTED_CONFIG_DEBUG DEBUG RELWITHDEBINFO RELEASE MINSIZEREL NONE)
		endif()
	endmacro()
	if(__jsoncpp_have_jsoncpplib)
		list(APPEND __jsoncpp_info_string "[${__jsoncpp_lib_type}]")
		_jsoncpp_apply_map_config(jsoncpp_lib)
	else()
		list(APPEND __jsoncpp_info_string "[]")
	endif()
	# part of the string to indicate if we found jsoncpp_lib_static
	if(TARGET jsoncpp_lib_static)
		list(APPEND __jsoncpp_info_string "[T]")
		_jsoncpp_apply_map_config(jsoncpp_lib_static)
	else()
		list(APPEND __jsoncpp_info_string "[]")
	endif()
endif()


# If we found something, and it's not the exact same as what we've found before...
# NOTE: The contents of this "if" block update only (internal) cache variables!
# (since this will only get run the first CMake pass that finds jsoncpp or that finds a different/updated install)
if(jsoncpp_FOUND AND NOT __jsoncpp_info_string STREQUAL "${JSONCPP_CACHED_JSONCPP_DIR_DETAILS}")
	#message("Updating jsoncpp cache variables! ${__jsoncpp_info_string}")
	set(JSONCPP_CACHED_JSONCPP_DIR_DETAILS "${__jsoncpp_info_string}" CACHE INTERNAL "" FORCE)
	unset(JSONCPP_IMPORTED_LIBRARY_SHARED)
	unset(JSONCPP_IMPORTED_LIBRARY_STATIC)
	unset(JSONCPP_IMPORTED_LIBRARY)
	unset(JSONCPP_IMPORTED_INCLUDE_DIRS)
	unset(JSONCPP_IMPORTED_LIBRARY_IS_SHARED)

	# if(__jsoncpp_have_jsoncpplib) is equivalent to if(TARGET jsoncpp_lib) except it excludes our
	# "invented" jsoncpp_lib interface targets, made for convenience purposes after this block.

	if(__jsoncpp_have_jsoncpplib AND TARGET jsoncpp_lib_static)

		# A veritable cache of riches - we have both shared and static!
		set(JSONCPP_IMPORTED_LIBRARY_SHARED jsoncpp_lib CACHE INTERNAL "" FORCE)
		set(JSONCPP_IMPORTED_LIBRARY_STATIC jsoncpp_lib_static CACHE INTERNAL "" FORCE)
		if(WIN32 OR CYGWIN OR MINGW)
			# DLL platforms: static library should be default
			set(JSONCPP_IMPORTED_LIBRARY ${JSONCPP_IMPORTED_LIBRARY_STATIC} CACHE INTERNAL "" FORCE)
			set(JSONCPP_IMPORTED_LIBRARY_IS_SHARED FALSE CACHE INTERNAL "" FORCE)
		else()
			# Other platforms - might require PIC to be linked into shared libraries, so safest to prefer shared.
			set(JSONCPP_IMPORTED_LIBRARY ${JSONCPP_IMPORTED_LIBRARY_SHARED} CACHE INTERNAL "" FORCE)
			set(JSONCPP_IMPORTED_LIBRARY_IS_SHARED TRUE CACHE INTERNAL "" FORCE)
		endif()

	elseif(TARGET jsoncpp_lib_static)
		# Well, only one variant, but we know for sure that it's static.
		set(JSONCPP_IMPORTED_LIBRARY_STATIC jsoncpp_lib_static CACHE INTERNAL "" FORCE)
		set(JSONCPP_IMPORTED_LIBRARY jsoncpp_lib_static CACHE INTERNAL "" FORCE)
		set(JSONCPP_IMPORTED_LIBRARY_IS_SHARED FALSE CACHE INTERNAL "" FORCE)

	elseif(__jsoncpp_have_jsoncpplib AND __jsoncpp_lib_type STREQUAL "STATIC_LIBRARY")
		# We were able to figure out the mystery library is static!
		set(JSONCPP_IMPORTED_LIBRARY_STATIC jsoncpp_lib CACHE INTERNAL "" FORCE)
		set(JSONCPP_IMPORTED_LIBRARY jsoncpp_lib CACHE INTERNAL "" FORCE)
		set(JSONCPP_IMPORTED_LIBRARY_IS_SHARED FALSE CACHE INTERNAL "" FORCE)

	elseif(__jsoncpp_have_jsoncpplib AND __jsoncpp_lib_type STREQUAL "SHARED_LIBRARY")
		# We were able to figure out the mystery library is shared!
		set(JSONCPP_IMPORTED_LIBRARY_SHARED jsoncpp_lib CACHE INTERNAL "" FORCE)
		set(JSONCPP_IMPORTED_LIBRARY jsoncpp_lib CACHE INTERNAL "" FORCE)
		set(JSONCPP_IMPORTED_LIBRARY_IS_SHARED TRUE CACHE INTERNAL "" FORCE)

	elseif(__jsoncpp_have_jsoncpplib)
		# One variant, and we have no idea if this is just an old version or if
		# this is shared based on the target name alone. Hmm.
		set(JSONCPP_IMPORTED_LIBRARY jsoncpp_lib CACHE INTERNAL "" FORCE)
	endif()

	# Now, we need include directories. Can't just limit this to old CMakes, since
	# new CMakes might be used to build projects designed to support older ones.
	if(__jsoncpp_have_jsoncpplib)
		get_property(__jsoncpp_interface_include_dirs TARGET jsoncpp_lib PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
		if(__jsoncpp_interface_include_dirs)
			set(JSONCPP_IMPORTED_INCLUDE_DIRS "${__jsoncpp_interface_include_dirs}" CACHE INTERNAL "" FORCE)
		endif()
	endif()
	if(TARGET jsoncpp_lib_static AND NOT JSONCPP_IMPORTED_INCLUDE_DIRS)
		get_property(__jsoncpp_interface_include_dirs TARGET jsoncpp_lib_static PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
		if(__jsoncpp_interface_include_dirs)
			set(JSONCPP_IMPORTED_INCLUDE_DIRS "${__jsoncpp_interface_include_dirs}" CACHE INTERNAL "" FORCE)
		endif()
	endif()
endif()

# As a convenience...
if(TARGET jsoncpp_lib_static AND NOT TARGET jsoncpp_lib)
	add_library(jsoncpp_lib INTERFACE)
	target_link_libraries(jsoncpp_lib INTERFACE jsoncpp_lib_static)
endif()

if(JSONCPP_IMPORTED_LIBRARY)
	if(NOT JSONCPP_IMPORTED_INCLUDE_DIRS)
		# OK, so we couldn't get it from the target... maybe we can figure it out from jsoncpp_DIR.

		# take off the jsoncpp component
		get_filename_component(__jsoncpp_import_root "${jsoncpp_DIR}/.." ABSOLUTE)
		set(__jsoncpp_hints "${__jsoncpp_import_root}")
		# take off the cmake component
		get_filename_component(__jsoncpp_import_root "${__jsoncpp_import_root}/.." ABSOLUTE)
		list(APPEND __jsoncpp_hints "${__jsoncpp_import_root}")
		# take off the lib component
		get_filename_component(__jsoncpp_import_root "${__jsoncpp_import_root}/.." ABSOLUTE)
		list(APPEND __jsoncpp_hints "${__jsoncpp_import_root}")
		# take off one more component in case of multiarch lib
		get_filename_component(__jsoncpp_import_root "${__jsoncpp_import_root}/.." ABSOLUTE)
		list(APPEND __jsoncpp_hints "${__jsoncpp_import_root}")

		# Now, search.
		find_path(JsonCpp_INCLUDE_DIR
			NAMES
			json/json.h
			PATH_SUFFIXES include jsoncpp include/jsoncpp
			HINTS ${__jsoncpp_hints})
		if(JsonCpp_INCLUDE_DIR)
			mark_as_advanced(JsonCpp_INCLUDE_DIR)
			# Note - this does not set it in the cache, in case we find it better at some point in the future!
			set(JSONCPP_IMPORTED_INCLUDE_DIRS ${JsonCpp_INCLUDE_DIR})
		endif()
	endif()

	find_package_handle_standard_args(JsonCpp
		DEFAULT_MSG
		jsoncpp_DIR
		JSONCPP_IMPORTED_LIBRARY
		JSONCPP_IMPORTED_INCLUDE_DIRS)
endif()

if(JSONCPP_FOUND)
	# Create any missing namespaced targets from the config module.
	if(__jsoncpp_have_namespaced_targets)
		if(JSONCPP_IMPORTED_LIBRARY AND NOT TARGET JsonCpp::JsonCpp)
			add_library(JsonCpp::JsonCpp INTERFACE IMPORTED)
			set_target_properties(JsonCpp::JsonCpp PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_IMPORTED_INCLUDE_DIRS}"
				INTERFACE_LINK_LIBRARIES "${JSONCPP_IMPORTED_LIBRARY}")
		endif()

		if(JSONCPP_IMPORTED_LIBRARY_SHARED AND NOT TARGET JsonCpp::JsonCppShared)
			add_library(JsonCpp::JsonCppShared INTERFACE IMPORTED)
			set_target_properties(JsonCpp::JsonCppShared PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_IMPORTED_INCLUDE_DIRS}"
				INTERFACE_LINK_LIBRARIES "${JSONCPP_IMPORTED_LIBRARY_SHARED}")
		endif()

		if(JSONCPP_IMPORTED_LIBRARY_STATIC AND NOT TARGET JsonCpp::JsonCppStatic)
			add_library(JsonCpp::JsonCppStatic INTERFACE IMPORTED)
			set_target_properties(JsonCpp::JsonCppStatic PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_IMPORTED_INCLUDE_DIRS}"
				INTERFACE_LINK_LIBRARIES "${JSONCPP_IMPORTED_LIBRARY_STATIC}")
		endif()

		# Hide the stuff we didn't, and no longer, need.
		if(NOT JsonCpp_LIBRARY)
			unset(JsonCpp_LIBRARY CACHE)
		endif()
		if(NOT JsonCpp_INCLUDE_DIR)
			unset(JsonCpp_INCLUDE_DIR CACHE)
		endif()
	endif()

	set(JSONCPP_LIBRARY ${JSONCPP_IMPORTED_LIBRARY})
	set(JSONCPP_INCLUDE_DIRS ${JSONCPP_IMPORTED_INCLUDE_DIRS})
	if(DEFINED JSONCPP_IMPORTED_LIBRARY_IS_SHARED)
		set(JSONCPP_LIBRARY_IS_SHARED ${JSONCPP_IMPORTED_LIBRARY_IS_SHARED})
	else()
		unset(JSONCPP_LIBRARY_IS_SHARED)
	endif()

	if(JSONCPP_IMPORTED_LIBRARY_SHARED)
		set(JSONCPP_LIBRARY_SHARED ${JSONCPP_IMPORTED_LIBRARY_SHARED})
	endif()

	if(JSONCPP_IMPORTED_LIBRARY_STATIC)
		set(JSONCPP_LIBRARY_STATIC ${JSONCPP_IMPORTED_LIBRARY_STATIC})
	endif()
endif()

# Still nothing after looking for the config file: must go "old-school"
if(NOT JSONCPP_FOUND)
	# Invoke pkgconfig for hints
	find_package(PkgConfig QUIET)
	set(_JSONCPP_INCLUDE_HINTS)
	set(_JSONCPP_LIB_HINTS)
	if(PKG_CONFIG_FOUND)
		pkg_search_module(_JSONCPP_PC QUIET jsoncpp)
		if(_JSONCPP_PC_INCLUDE_DIRS)
			set(_JSONCPP_INCLUDE_HINTS ${_JSONCPP_PC_INCLUDE_DIRS})
		endif()
		if(_JSONCPP_PC_LIBRARY_DIRS)
			set(_JSONCPP_LIB_HINTS ${_JSONCPP_PC_LIBRARY_DIRS})
		endif()
		if(_JSONCPP_PC_LIBRARIES)
			set(_JSONCPP_LIB_NAMES ${_JSONCPP_PC_LIBRARIES})
		endif()
	endif()

	if(NOT _JSONCPP_LIB_NAMES)
		# OK, if pkg-config wasn't able to give us a library name suggestion, then we may
		# have to resort to some intense old logic.
		set(_JSONCPP_LIB_NAMES jsoncpp)
		set(_JSONCPP_PATHSUFFIXES)

		if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
			list(APPEND _JSONCPP_PATHSUFFIXES
				linux-gcc) # bit of a generalization but close...
		endif()
		if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
			list(APPEND
				_JSONCPP_LIB_NAMES
				json_linux-gcc-${CMAKE_CXX_COMPILER_VERSION}_libmt
				json_linux-gcc_libmt)
			list(APPEND _JSONCPP_PATHSUFFIXES
				linux-gcc-${CMAKE_CXX_COMPILER_VERSION})

		elseif(MSVC)
			if(MSVC_VERSION EQUAL 1200)
				list(APPEND _JSONCPP_LIB_NAMES json_vc6_libmt)
				list(APPEND _JSONCPP_PATHSUFFIXES msvc6)
			elseif(MSVC_VERSION EQUAL 1300)
				list(APPEND _JSONCPP_LIB_NAMES json_vc7_libmt)
				list(APPEND _JSONCPP_PATHSUFFIXES msvc7)
			elseif(MSVC_VERSION EQUAL 1310)
				list(APPEND _JSONCPP_LIB_NAMES json_vc71_libmt)
				list(APPEND _JSONCPP_PATHSUFFIXES msvc71)
			elseif(MSVC_VERSION EQUAL 1400)
				list(APPEND _JSONCPP_LIB_NAMES json_vc8_libmt)
				list(APPEND _JSONCPP_PATHSUFFIXES msvc80)
			elseif(MSVC_VERSION EQUAL 1500)
				list(APPEND _JSONCPP_LIB_NAMES json_vc9_libmt)
				list(APPEND _JSONCPP_PATHSUFFIXES msvc90)
			elseif(MSVC_VERSION EQUAL 1600)
				list(APPEND _JSONCPP_LIB_NAMES json_vc10_libmt)
				list(APPEND _JSONCPP_PATHSUFFIXES msvc10 msvc100)
			endif()

		elseif(MINGW)
			list(APPEND _JSONCPP_LIB_NAMES
				json_mingw_libmt)
			list(APPEND _JSONCPP_PATHSUFFIXES mingw)

		else()
			list(APPEND _JSONCPP_LIB_NAMES
				json_suncc_libmt
				json_vacpp_libmt)
		endif()
	endif() # end of old logic

	# Actually go looking.
	find_path(JsonCpp_INCLUDE_DIR
		NAMES
		json/json.h
		PATH_SUFFIXES jsoncpp
		HINTS ${_JSONCPP_INCLUDE_HINTS})
	find_library(JsonCpp_LIBRARY
		NAMES
		${_JSONCPP_LIB_NAMES}
		PATHS libs
		PATH_SUFFIXES ${_JSONCPP_PATHSUFFIXES}
		HINTS ${_JSONCPP_LIB_HINTS})

	find_package_handle_standard_args(JsonCpp
		DEFAULT_MSG
		JsonCpp_INCLUDE_DIR
		JsonCpp_LIBRARY)

	if(JSONCPP_FOUND)
		# We already know that the target doesn't exist, let's make it.
		# TODO don't know why we get errors like:
		# error: 'JsonCpp::JsonCpp-NOTFOUND', needed by 'bin/osvr_json_to_c', missing and no known rule to make it
		# when we do the imported target commented out below. So, instead, we make an interface
		# target with an alias. Hmm.

		#add_library(JsonCpp::JsonCpp UNKNOWN IMPORTED)
		#set_target_properties(JsonCpp::JsonCpp PROPERTIES
		#	IMPORTED_LOCATION "${JsonCpp_LIBRARY}"
		#	INTERFACE_INCLUDE_DIRECTORIES "${JsonCpp_INCLUDE_DIR}"
		#	IMPORTED_LINK_INTERFACE_LANGUAGES "CXX")

		set(JSONCPP_LIBRARY "${JsonCpp_LIBRARY}")
		set(JSONCPP_INCLUDE_DIRS "${JsonCpp_INCLUDE_DIR}")
		unset(JSONCPP_LIBRARY_IS_SHARED)

		if(__jsoncpp_have_interface_support AND NOT TARGET jsoncpp_interface)
			add_library(jsoncpp_interface INTERFACE)
			set_target_properties(jsoncpp_interface PROPERTIES
				INTERFACE_LINK_LIBRARIES "${JsonCpp_LIBRARY}"
				INTERFACE_INCLUDE_DIRECTORIES "${JsonCpp_INCLUDE_DIR}")
		endif()
		if(__jsoncpp_have_namespaced_targets)
			if(NOT TARGET JsonCpp::JsonCpp)
				add_library(JsonCpp::JsonCpp ALIAS jsoncpp_interface)
			endif()
		endif()
	endif()
endif()

if(JSONCPP_FOUND)
	mark_as_advanced(jsoncpp_DIR JsonCpp_INCLUDE_DIR JsonCpp_LIBRARY)
endif()
