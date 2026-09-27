# Copyright 2020, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0

set(_FILESYSTEM_UTILS_DIR "${PROJECT_SOURCE_DIR}/src/common")

if(MSVC AND MSVC_VERSION GREATER 1890)
    set(HAVE_FILESYSTEM_WITHOUT_LIB
        ON
        CACHE INTERNAL "" FORCE
    )
    if(MSVC_VERSION GREATER 1910)
        # Visual Studio 2017 Update 3 added new filesystem impl,
        # which only works in C++17 mode.
        set(HAVE_FILESYSTEM_NEEDS_17
            ON
            CACHE INTERNAL "" FORCE
        )
    endif()
else()
    include(CheckCXXSourceCompiles)

    ###
    # Test Sources
    ###

    # This is just example code that is known to not compile if std::filesystem isn't working right.
    # It depends on having the proper includes and `using namespace` so it can use the `is_regular_file`
    # function unqualified.
    # It is at the end of every test file below.
    set(_stdfs_test_source
        "int main() {
        (void)is_regular_file(\"/\");
        return 0;
    }
    "
    )

    # This is preprocessor code included in all test compiles, which pulls in the conditions
    # originally found in filesystem_utils.cpp.
    #
    # It defines:
    #   USE_FINAL_FS = 1         if it thinks we have the full std::filesystem in <filesystem> as in C++17
    #   USE_EXPERIMENTAL_FS = 1  if it thinks we don't have the full c++17 filesystem, but should have
    #                            std::experimental::filesystem and <experimental/filesystem>
    #
    # Ideally the first condition (__cplusplus >= 201703L) would handle most cases,
    # however you're not supposed to report that unless you're fully conformant with all
    # of c++17, so you might have a c++17 build flag and the final filesystem library but
    # a lower __cplusplus value if some other part of the standard is missing.
    set(_stdfs_conditions "#include <stdfs_conditions.h>
    ")

    # This should only compile if our common detection code decides on the
    # **final** (non-experimental) filesystem library.
    set(_stdfs_source
        "${_stdfs_conditions}
    #if defined(USE_FINAL_FS) && USE_FINAL_FS
    #include <filesystem>
    using namespace std::filesystem;
    #endif
    ${_stdfs_test_source}
    "
    )

    # This should only compile if our common detection code decides on the
    # **experimental** filesystem library.
    set(_stdfs_experimental_source
        "${_stdfs_conditions}
    #if defined(USE_EXPERIMENTAL_FS) && USE_EXPERIMENTAL_FS
    #include <experimental/filesystem>
    using namespace std::experimental::filesystem;
    #endif
    ${_stdfs_test_source}
    "
    )

    # This should compile if the common detection code decided that either
    # the experimental or final filesystem library is available.
    # We use this when trying to detect what library to link, if any:
    # earlier checks are the ones that care about how we include the headers.
    set(_stdfs_needlib_source
        "${_stdfs_conditions}
    #if defined(USE_FINAL_FS) && USE_FINAL_FS
    #include <filesystem>
    using namespace std::filesystem;
    #endif
    #if defined(USE_EXPERIMENTAL_FS) && USE_EXPERIMENTAL_FS
    #include <experimental/filesystem>
    using namespace std::experimental::filesystem;
    #endif
    ${_stdfs_test_source}
    "
    )

    ###
    # Identifying header/namespace and standards flags
    ###

    # First, just look for the include.
    # We're checking if it compiles, not if the include exists,
    # because the source code uses the same conditionals to decide.
    # (Static libraries are just object files, they don't get linked)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(CMAKE_REQUIRED_INCLUDES "${_FILESYSTEM_UTILS_DIR}")
    unset(CMAKE_REQUIRED_LIBRARIES)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0)
        # GCC 11+ defaults to C++17 mode and acts badly in these tests if we tell it to use a different version.
        set(HAVE_FILESYSTEM_IN_STD_17 ON)
        set(HAVE_FILESYSTEM_IN_STD OFF)
    else()
        set(CMAKE_REQUIRED_FLAGS "-DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=TRUE")
        check_cxx_source_compiles("${_stdfs_source}" HAVE_FILESYSTEM_IN_STD)
        check_cxx_source_compiles("${_stdfs_experimental_source}" HAVE_FILESYSTEM_IN_STDEXPERIMENTAL)

        # See if the "final" version builds if we try to specify C++17 explicitly
        set(CMAKE_REQUIRED_FLAGS "-DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=TRUE")
        check_cxx_source_compiles("${_stdfs_source}" HAVE_FILESYSTEM_IN_STD_17)
        unset(CMAKE_REQUIRED_FLAGS)
    endif()

    # If we found the final version of filesystem when specifying C++17 explicitly,
    # but found it no other way, then we record that we must use C++17 flags.
    if(HAVE_FILESYSTEM_IN_STD_17 AND NOT HAVE_FILESYSTEM_IN_STD)
        set(HAVE_FILESYSTEM_NEEDS_17
            ON
            CACHE INTERNAL ""
        )
        set(CMAKE_REQUIRED_FLAGS "-DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=TRUE")
    else()
        set(HAVE_FILESYSTEM_NEEDS_17
            OFF
            CACHE INTERNAL ""
        )
    endif()

    ###
    # Identifying library to link
    ###

    # Now, see if we need to link against libstdc++fs, and what it's called
    # If we needed C++17 standard flags to find it, they've already been set above.
    set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

    # Try with no lib specified
    check_cxx_source_compiles("${_stdfs_needlib_source}" HAVE_FILESYSTEM_WITHOUT_LIB)

    # Try with stdc++fs
    set(CMAKE_REQUIRED_LIBRARIES stdc++fs)
    check_cxx_source_compiles("${_stdfs_needlib_source}" HAVE_FILESYSTEM_NEEDING_LIBSTDCXXFS)

    # Try with c++fs (from clang's libc++)
    set(CMAKE_REQUIRED_LIBRARIES c++fs)
    check_cxx_source_compiles("${_stdfs_needlib_source}" HAVE_FILESYSTEM_NEEDING_LIBCXXFS)

    # Clean up these variables before the next user.
    unset(CMAKE_REQUIRED_LIBRARIES)
    unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
    unset(CMAKE_REQUIRED_INCLUDES)
endif()

# Use the observations of the code above to add the filesystem_utils.cpp
# file to a target, along with any required compiler settings and libraries.
# Also handles our BUILD_WITH_STD_FILESYSTEM option.
function(openxr_add_filesystem_utils TARGET_NAME)
    target_sources(${TARGET_NAME} PRIVATE ${_FILESYSTEM_UTILS_DIR}/filesystem_utils.cpp)
    target_include_directories(${TARGET_NAME} PRIVATE ${_FILESYSTEM_UTILS_DIR})
    if(NOT BUILD_WITH_STD_FILESYSTEM)
        target_compile_definitions(${TARGET_NAME} PRIVATE DISABLE_STD_FILESYSTEM)
    else()
        if(HAVE_FILESYSTEM_NEEDS_17)
            set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 17)
            set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD_REQUIRED TRUE)
        endif()
        if(NOT HAVE_FILESYSTEM_WITHOUT_LIB)
            if(HAVE_FILESYSTEM_NEEDING_LIBSTDCXXFS)
                target_link_libraries(${TARGET_NAME} PRIVATE stdc++fs)
            elseif(HAVE_FILESYSTEM_NEEDING_LIBCXXFS)
                target_link_libraries(${TARGET_NAME} PRIVATE c++fs)
            endif()
        endif()
    endif()
endfunction()
