# Copyright (c) 2017-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if(NOT MSVC)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
endif()

# List of all files externally generated outside of the loader that the loader
# needs to build with.
set(LOADER_EXTERNAL_GEN_FILES ${LOADER_GENERATED_OUTPUT})
set(LOADER_EXTERNAL_GEN_DEPENDS ${LOADER_GENERATED_DEPENDS})
run_xr_xml_generate(loader_source_generator.py xr_generated_loader.hpp)
run_xr_xml_generate(loader_source_generator.py xr_generated_loader.cpp)

if(DYNAMIC_LOADER)
    add_definitions(-DXRAPI_DLL_EXPORT)
    set(LIBRARY_TYPE SHARED)
else()
    # build static lib
    set(LIBRARY_TYPE STATIC)
endif()

add_library(
    openxr_loader
    ${LIBRARY_TYPE}
    android_utilities.cpp
    android_utilities.h
    api_layer_interface.cpp
    api_layer_interface.hpp
    loader_core.cpp
    loader_init_data.cpp
    loader_init_data.hpp
    loader_instance.cpp
    loader_instance.hpp
    loader_logger.cpp
    loader_logger.hpp
    loader_logger_recorders.cpp
    loader_logger_recorders.hpp
    loader_properties.cpp
    loader_properties.hpp
    manifest_file.cpp
    manifest_file.hpp
    runtime_interface.cpp
    runtime_interface.hpp
    "${PROJECT_SOURCE_DIR}/src/common/hex_and_handles.h"
    "${PROJECT_SOURCE_DIR}/src/common/object_info.cpp"
    "${PROJECT_SOURCE_DIR}/src/common/object_info.h"
    "${PROJECT_SOURCE_DIR}/src/common/platform_utils.hpp"
    ${GENERATED_OUTPUT}
    ${LOADER_EXTERNAL_GEN_FILES}
)
set_target_properties(openxr_loader PROPERTIES FOLDER ${LOADER_FOLDER})
add_sanitizers(openxr_loader)

target_include_directories(
    openxr_loader
    # for OpenXR headers
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src/common"
        # for generated dispatch table, common_config.h
        ..
        "${CMAKE_CURRENT_BINARY_DIR}/.."
        # for target-specific generated files
        .
        "${CMAKE_CURRENT_BINARY_DIR}"
    INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
add_dependencies(openxr_loader xr_global_generated_files)

target_link_libraries(
    openxr_loader
    PRIVATE ${CMAKE_DL_LIBS}
    PUBLIC Threads::Threads OpenXR::headers
)
target_compile_definitions(
    openxr_loader PRIVATE ${OPENXR_ALL_SUPPORTED_DEFINES}
)
openxr_add_filesystem_utils(openxr_loader)

set_target_properties(
    openxr_loader PROPERTIES DEBUG_POSTFIX "${OPENXR_DEBUG_POSTFIX}"
)

# TODO remove once we get rid of add_definitions()
if(Vulkan_FOUND)
    target_include_directories(openxr_loader PRIVATE ${Vulkan_INCLUDE_DIR})
endif()

# Get jsoncpp externally or internally
if(BUILD_WITH_SYSTEM_JSONCPP)
    target_link_libraries(openxr_loader PRIVATE JsonCpp::JsonCpp)
else()
    if(NOT BUILD_LOADER_WITH_EXCEPTION_HANDLING)
        target_compile_definitions(openxr_loader PRIVATE JSON_USE_EXCEPTION=0)
    endif()

    set(jsoncpp_sources
        "${PROJECT_SOURCE_DIR}/src/external/jsoncpp/src/lib_json/json_reader.cpp"
        "${PROJECT_SOURCE_DIR}/src/external/jsoncpp/src/lib_json/json_value.cpp"
        "${PROJECT_SOURCE_DIR}/src/external/jsoncpp/src/lib_json/json_writer.cpp"
    )

    set_source_files_properties(${jsoncpp_sources} PROPERTIES SKIP_LINTING ON)

    target_sources(openxr_loader PRIVATE ${jsoncpp_sources})
    target_include_directories(
        openxr_loader
        PRIVATE "${PROJECT_SOURCE_DIR}/src/external/jsoncpp/include"
    )
    if(SUPPORTS_Werrorunusedparameter)
        # Don't error on this - triggered by jsoncpp
        target_compile_options(openxr_loader PRIVATE -Wno-unused-parameter)
    endif()
endif()

if(LOADER_EXTERNAL_GEN_DEPENDS)
    # cmake-format: off
    set_source_files_properties(${LOADER_EXTERNAL_GEN_DEPENDS}
        PROPERTIES
            GENERATED TRUE
            SKIP_LINTING ON
    )
    # cmake-format: on
endif()

if(NOT BUILD_LOADER_WITH_EXCEPTION_HANDLING)
    target_compile_definitions(
        openxr_loader PRIVATE XRLOADER_DISABLE_EXCEPTION_HANDLING
    )

    #   TODO: uncomment this once jnipp starts supporting -fno-exceptions
    #    if(ANDROID AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang"))
    #        target_compile_options(openxr_loader
    #            PRIVATE
    #                -fno-exceptions
    #        )
    #    endif()

endif()

# Platform details

if(ANDROID)
    target_link_libraries(
        openxr_loader PRIVATE ${ANDROID_LOG_LIBRARY} ${ANDROID_LIBRARY}
    )
    # Force 16KB page sizes for Android 15+ compatibility.
    target_link_options(openxr_loader PRIVATE "-Wl,-z,max-page-size=16384")

elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(FALLBACK_CONFIG_DIRS
        "/etc/xdg"
        CACHE
            STRING
            "Search path to use when XDG_CONFIG_DIRS is unset or empty or the current process is SUID/SGID. Default is freedesktop compliant."
    )
    set(FALLBACK_DATA_DIRS
        "/usr/local/share:/usr/share"
        CACHE
            STRING
            "Search path to use when XDG_DATA_DIRS is unset or empty or the current process is SUID/SGID. Default is freedesktop compliant."
    )
    target_compile_definitions(
        openxr_loader
        PRIVATE
            FALLBACK_CONFIG_DIRS="${FALLBACK_CONFIG_DIRS}"
            FALLBACK_DATA_DIRS="${FALLBACK_DATA_DIRS}"
            SYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}"
    )
    if(NOT (CMAKE_INSTALL_FULL_SYSCONFDIR STREQUAL "/etc"))
        target_compile_definitions(openxr_loader PRIVATE EXTRASYSCONFDIR="/etc")
    endif()

    set_target_properties(
        openxr_loader PROPERTIES SOVERSION "${MAJOR}" VERSION
                                                      "${OPENXR_FULL_VERSION}"
    )

    add_custom_target(
        libopenxr_loader.so.${MAJOR}.${MINOR} ALL
        COMMAND
            "${CMAKE_COMMAND}" -E create_symlink
            libopenxr_loader.so.${OPENXR_FULL_VERSION}
            libopenxr_loader.so.${MAJOR}.${MINOR}
    )

elseif(WIN32)

    # Pass version fields as preprocessor for .rc resource version on Windows.
    target_compile_definitions(
        openxr_loader
        PRIVATE
            XR_CURRENT_API_MAJOR_VERSION=${MAJOR}
            XR_CURRENT_API_MINOR_VERSION=${MINOR}
            XR_CURRENT_API_PATCH_VERSION=${PATCH}
    )

    target_sources(
        openxr_loader PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/loader.rc"
    )

    if(MSVC)
        if(DYNAMIC_LOADER)
            target_sources(
                openxr_loader
                PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/openxr-loader.def"
            )
        endif()

        if(DYNAMIC_LOADER AND NOT (CMAKE_SYSTEM_NAME STREQUAL "WindowsStore"))
            # If building DLLs, force static CRT linkage
            set_target_properties(
                openxr_loader
                PROPERTIES MSVC_RUNTIME_LIBRARY
                           "MultiThreaded$<$<CONFIG:Debug>:Debug>"
            )
        else()
            # WindowsStore (UWP) apps must be compiled with dynamic CRT linkage (default)
            # Otherwise for static libs, link the CRT dynamically
            set_target_properties(
                openxr_loader
                PROPERTIES MSVC_RUNTIME_LIBRARY
                           "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
            )
        endif()
        target_compile_options(openxr_loader PRIVATE /wd6386)
    endif()

    target_link_libraries(openxr_loader PUBLIC advapi32)
endif()

# Need to copy DLL to client directories so clients can easily load it.
if(DYNAMIC_LOADER AND (CMAKE_GENERATOR MATCHES "^Visual Studio.*"))
    file(
        TO_NATIVE_PATH
        ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/openxr_loader$<$<CONFIG:Debug>:${OPENXR_DEBUG_POSTFIX}>.dll
        COPY_DLL_SRC_PATH
    )
    file(
        TO_NATIVE_PATH
        ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIGURATION>/openxr_loader$<$<CONFIG:Debug>:${OPENXR_DEBUG_POSTFIX}>.pdb
        COPY_PDB_SRC_PATH
    )
    file(
        TO_NATIVE_PATH
        ${CMAKE_CURRENT_BINARY_DIR}/../tests/hello_xr/$<CONFIGURATION>/
        COPY_DST_HELLO_XR_PATH
    )
    file(
        TO_NATIVE_PATH
        ${CMAKE_CURRENT_BINARY_DIR}/../tests/loader_test/$<CONFIGURATION>/
        COPY_DST_LOADER_TEST_PATH
    )
    file(
        TO_NATIVE_PATH
        ${CMAKE_CURRENT_BINARY_DIR}/../conformance/conformance_test/$<CONFIGURATION>/
        COPY_DST_CONFORMANCE_TEST_PATH
    )
    add_custom_command(
        TARGET openxr_loader
        POST_BUILD
        COMMAND xcopy /Y /I ${COPY_DLL_SRC_PATH} ${COPY_DST_HELLO_XR_PATH}
        COMMAND
            if exist ${COPY_PDB_SRC_PATH} xcopy /Y /I ${COPY_PDB_SRC_PATH}
            ${COPY_DST_HELLO_XR_PATH}
        COMMAND xcopy /Y /I ${COPY_DLL_SRC_PATH} ${COPY_DST_LOADER_TEST_PATH}
        COMMAND
            if exist ${COPY_PDB_SRC_PATH} xcopy /Y /I ${COPY_PDB_SRC_PATH}
            ${COPY_DST_LOADER_TEST_PATH}
        COMMAND
            xcopy /Y /I ${COPY_DLL_SRC_PATH} ${COPY_DST_CONFORMANCE_TEST_PATH}
        COMMAND
            if exist ${COPY_PDB_SRC_PATH} xcopy /Y /I ${COPY_PDB_SRC_PATH}
            ${COPY_DST_CONFORMANCE_TEST_PATH}
    )
endif()

# jnipp - android only
if(ANDROID)
    set(JNIPP_ROOT "${PROJECT_SOURCE_DIR}/src/external/jnipp")
    set(JNIWRAPPER_ROOT
        "${PROJECT_SOURCE_DIR}/src/external/android-jni-wrappers"
    )
    file(
        GLOB
        ANDROID_WRAP_SOURCES
        CONFIGURE_DEPENDS
        ${JNIWRAPPER_ROOT}/wrap/*.cpp
    )
    target_sources(
        openxr_loader
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/android_utilities.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/android_utilities.h
            ${ANDROID_WRAP_SOURCES}
            ${JNIPP_ROOT}/jnipp.cpp
    )
    target_include_directories(
        openxr_loader PRIVATE ${JNIPP_ROOT} ${JNIWRAPPER_ROOT}
    )
endif()

# Make pkg-config file
if(NOT MSVC)
    set(XR_API_VERSION "${MAJOR}.${MINOR}")
    set(EXTRA_LIBS ${CMAKE_THREAD_LIBS_INIT})
    configure_file(openxr.pc.in openxr.pc @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/openxr.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )
endif()

# Copy loader to conformance_cli binary folder if built as dynamic
if(DYNAMIC_LOADER AND BUILD_CONFORMANCE_CLI)
    add_custom_command(
        TARGET openxr_loader
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:openxr_loader>"
            "$<TARGET_PROPERTY:conformance_cli,BINARY_DIR>"
    )
endif()

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(NOT MSVC)
        # Do not do this for clang-cl
        target_compile_options(
            openxr_loader PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>"
                                  -ffunction-sections -fdata-sections
        )
    endif()
    target_compile_options(
        openxr_loader PRIVATE -Wextra -fno-strict-aliasing -fno-builtin-memcmp
    )

    # Add the linker flag, and make build depend on the version script/export map
    if(APPLE)
        set_target_properties(
            openxr_loader
            PROPERTIES
                LINK_FLAGS
                "-Wl,-exported_symbols_list,\"${CMAKE_CURRENT_SOURCE_DIR}/openxr-loader.expsym\""
        )
        target_sources(
            openxr_loader
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/openxr-loader.expsym"
        )
    else()
        set_target_properties(
            openxr_loader
            PROPERTIES
                LINK_FLAGS
                "-Wl,--version-script=\"${CMAKE_CURRENT_SOURCE_DIR}/openxr-loader.map\""
        )
        target_sources(
            openxr_loader
            PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/openxr-loader.map"
        )
    endif()
    # For GCC version 7.1 or greater, we need to disable the implicit fallthrough warning since
    # there's no consistent way to satisfy all compilers until they all accept the C++17 standard
    if(CMAKE_COMPILER_IS_GNUCC AND NOT (CMAKE_CXX_COMPILER_VERSION LESS 7.1))
        target_compile_options(openxr_loader PRIVATE -Wimplicit-fallthrough=0)
    endif()
endif()

include(CheckCCompilerFlag)

if(NOT ANDROID)
    check_c_compiler_flag(-Wmissing-prototypes HAVE_MISSING_PROTOTYPES)
    # https://gitlab.khronos.org/openxr/openxr/-/merge_requests/4020 highlighted that
    # teams often have stricter compiler settings for the openxr_loader
    if(HAVE_MISSING_PROTOTYPES)
        # https://gitlab.khronos.org/openxr/openxr/-/merge_requests/4120
        # Limit check to clang until this is supported more widely.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(
                openxr_loader PRIVATE -Werror=missing-prototypes
            )
        else()
            target_compile_options(
                openxr_loader
                PRIVATE $<IF:$<COMPILE_LANGUAGE:C>,-Werror=missing-prototypes,>
            )
        endif()
    endif()
endif()

check_c_compiler_flag(-Wundef HAVE_UNDEF)
check_c_compiler_flag(-Wtautological-type-limit-compare HAVE_TAUTOLOGICAL_TYPE_LIMIT)
check_c_compiler_flag(-Wmissing-braces HAVE_MISSING_BRACES)
check_c_compiler_flag(-Wunreachable-code HAVE_UNREACHABLE_CODE)
check_c_compiler_flag(-Wc++20-extensions HAVE_CPP20_EXTENSIONS)
# check_c_compiler_flag(-Wswitch-default HAVE_SWITCH_DEFAULT)
# check_c_compiler_flag(-Wswitch-enum HAVE_SWITCH_ENUM)

if(HAVE_UNDEF)
    target_compile_options(
            openxr_loader PRIVATE -Werror=undef)
endif()
if(HAVE_TAUTOLOGICAL_TYPE_LIMIT)
    target_compile_options(
            openxr_loader PRIVATE -Werror=tautological-type-limit-compare)
endif()
if(HAVE_MISSING_BRACES)
    target_compile_options(
            openxr_loader PRIVATE -Werror=missing-braces)
endif()
if(HAVE_UNREACHABLE_CODE)
    target_compile_options(
            openxr_loader PRIVATE -Werror=unreachable-code)
endif()
if(HAVE_CPP20_EXTENSIONS)
    target_compile_options(
            openxr_loader PRIVATE -Werror=c++20-extensions)
endif()
# json_value.cpp fails this right now
# if(HAVE_SWITCH_DEFAULT)
#     target_compile_options(
#             openxr_loader PRIVATE -Werror=switch-default)
# endif()
# json_reader.cpp fails this right now
# if(HAVE_SWITCH_ENUM)
#     target_compile_options(
#             openxr_loader PRIVATE -Werror=switch-enum)
# endif()

# We will never actually install here, but it's easier to set something than block all install code.
if(ANDROID AND NOT INSTALL_TO_ARCHITECTURE_PREFIXES)
    set(CMAKE_INSTALL_BINDIR bin)
    set(CMAKE_INSTALL_LIBDIR lib)
endif()

install(
    TARGETS openxr_loader headers
    EXPORT openxr_loader_export
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Loader
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Loader
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Loader
)

export(
    EXPORT openxr_loader_export
    FILE ${CMAKE_CURRENT_BINARY_DIR}/OpenXRTargets.cmake
    NAMESPACE OpenXR::
)

# Create alias so that it can be used the same whether vendored as source or found with CMake.
add_library(OpenXR::openxr_loader ALIAS openxr_loader)

if(WIN32 AND NOT INSTALL_TO_ARCHITECTURE_PREFIXES)
    set(TARGET_DESTINATION cmake)
else()
    set(TARGET_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/openxr/)
endif()

install(
    EXPORT openxr_loader_export
    FILE OpenXRTargets.cmake
    NAMESPACE OpenXR::
    DESTINATION ${TARGET_DESTINATION}
    COMPONENT CMakeConfigs
)

include(CMakePackageConfigHelpers)
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/)
configure_package_config_file(
    OpenXRConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/OpenXRConfig.cmake
    INSTALL_DESTINATION ${TARGET_DESTINATION}
    PATH_VARS INCLUDE_INSTALL_DIR
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/OpenXRConfigVersion.cmake
    VERSION "${OPENXR_FULL_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/OpenXRConfig.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/OpenXRConfigVersion.cmake
    DESTINATION ${TARGET_DESTINATION}
    COMPONENT CMakeConfigs
)

if(WIN32
   AND NOT MINGW
   AND DYNAMIC_LOADER
   AND INSTALL_SYMBOLS
)
    install(
        FILES $<TARGET_PDB_FILE:openxr_loader>
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        OPTIONAL
    )
endif()

# Make the "meta" cmake config/version file to redirect to the right arch/build
if(WIN32 AND INSTALL_TO_ARCHITECTURE_PREFIXES)
    set(TARGET_SUBDIR cmake/openxr)
    set(WARNING "This is a generated file - do not edit!")
    set(FN OpenXRConfig)
    configure_file(
        ../cmake/metaconfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/metaconfig.cmake @ONLY
    )
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/metaconfig.cmake
        DESTINATION .
        RENAME OpenXRConfig.cmake
        COMPONENT CMakeConfigs
    )

    set(FN OpenXRConfigVersion)
    configure_file(
        ../cmake/metaconfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/metaconfigversion.cmake @ONLY
    )
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/metaconfigversion.cmake
        DESTINATION .
        RENAME OpenXRConfigVersion.cmake
        COMPONENT CMakeConfigs
    )
elseif(ANDROID AND INSTALL_TO_ARCHITECTURE_PREFIXES)
    # Root directory CMake config and version files, in case you extract the .aar
    set(TARGET_SUBDIR cmake/openxr)
    set(WARNING "This is a generated file - do not edit!")
    set(FN OpenXRConfig)
    configure_file(
        ../cmake/metaconfig_android.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/metaconfig.cmake @ONLY
    )
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/metaconfig.cmake
        DESTINATION .
        RENAME OpenXRConfig.cmake
        COMPONENT CMakeConfigs
    )

    set(FN OpenXRConfigVersion)
    configure_file(
        ../cmake/metaconfig_android.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/metaconfigversion.cmake @ONLY
    )
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/metaconfigversion.cmake
        DESTINATION .
        RENAME OpenXRConfigVersion.cmake
        COMPONENT CMakeConfigs
    )

    # json data files for a "prefab" AAR
    configure_file(abi.json "${CMAKE_CURRENT_BINARY_DIR}/abi.json" @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/abi.json"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT Prefab
    )

    configure_file(prefab.json "${CMAKE_CURRENT_BINARY_DIR}/prefab.json" @ONLY)
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/prefab.json"
        DESTINATION ${PREFAB_INSTALL_DIR}
        COMPONENT Prefab
    )

    install(
        FILES module.json
        DESTINATION ${PREFAB_LOADER_MODULE_INSTALL_DIR}
        COMPONENT Prefab
    )

    # Manifest to merge with the consuming app
    configure_file(
        AndroidManifest.xml.in
        "${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml"
    )
    install(
        FILES "${CMAKE_CURRENT_BINARY_DIR}/AndroidManifest.xml"
        DESTINATION .
        COMPONENT Prefab
    )

    # Copy of library for older build systems. Gets de-duplicated in AAR so no size change.
    install(
        TARGETS openxr_loader
        RUNTIME DESTINATION "jni/${ANDROID_ABI}/" COMPONENT Loader
        LIBRARY DESTINATION "jni/${ANDROID_ABI}/" COMPONENT Loader
        ARCHIVE DESTINATION "jni/${ANDROID_ABI}/" COMPONENT Loader
    )

    # These get used directly by build-aar.sh
    configure_file(
        openxr_loader_for_android.pom
        "${CMAKE_CURRENT_BINARY_DIR}/openxr_loader_for_android-${OPENXR_FULL_VERSION}${OPENXR_ANDROID_VERSION_SUFFIX}.pom"
    )

    configure_file(
        additional_manifest.mf.in
        "${CMAKE_CURRENT_BINARY_DIR}/additional_manifest.mf"
    )
endif()
