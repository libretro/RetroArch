# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(retroarch)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)
if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()


set(RARCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../)
set(DEPS_DIR ${RARCH_DIR}/deps)
set(LIBRETRO_COMM_DIR ${RARCH_DIR}/libretro-common)

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)


# 选项配置
set(HAVE_NEON 1)
set(HAVE_LOGGER 0)
set(HAVE_VULKAN 1)
set(HAVE_CHEEVOS 1)
set(HAVE_FILE_LOGGER 1)
set(HAVE_GFX_WIDGETS 1)
set(HAVE_SAF 1)
set(HAVE_BUILTINSMBCLIENT 1)

# 基础定义
add_definitions(
    -D_DEBUG
    -DRARCH_MOBILE
    -DHAVE_GRIFFIN
    -DHAVE_STB_VORBIS
    -DHAVE_LANGEXTRA
    -DOHOS # TODO
    -DHAVE_DYNAMIC
    -DHAVE_OPENGL_CORE
    -DHAVE_OPENGL
    -DHAVE_OVERLAY
    -DHAVE_OPENGLES
    -DGLSL_DEBUG
    -DHAVE_DYLIB
    -DHAVE_EGL
    -DHAVE_GLSL
    -DHAVE_MENU
    -DHAVE_CONFIGFILE
    -DHAVE_PATCH
    -DHAVE_DSP_FILTER
    -DHAVE_VIDEO_FILTER
    -DHAVE_SCREENSHOTS
    -DHAVE_REWIND
    -DHAVE_CHEATS
    -DHAVE_BSV_MOVIE
    -DHAVE_ZLIB
    -DHAVE_NO_BUILTINZLIB
    -DHAVE_ZSTD
    -DZSTD_DISABLE_ASM
    -DHAVE_CHEEVOS_RVZ
    -DHAVE_RPNG
    -DHAVE_RJPEG
    -DHAVE_RBMP
    -DHAVE_RTGA
    -DINLINE=inline
    -DHAVE_THREADS
    -D__LIBRETRO__
    -DHAVE_RSOUND
    -DHAVE_NETWORKGAMEPAD
    -DHAVE_NETWORKING
    -DHAVE_NETWORK_CMD
    -DHAVE_COMMAND
    -DHAVE_CLOUDSYNC
    -DHAVE_IFINFO
    -DHAVE_NETPLAYDISCOVERY
    -DRARCH_INTERNAL
    -DHAVE_FILTERS_BUILTIN
    -DHAVE_RGUI
    -DHAVE_MATERIALUI
    -DHAVE_XMB
    -DHAVE_OZONE
    -DHAVE_SHADERPIPELINE
    -DHAVE_LIBRETRODB
    -DHAVE_STB_FONT
    -DHAVE_IMAGEVIEWER
    -DHAVE_ONLINE_UPDATER
    -DHAVE_UPDATE_ASSETS
    #-DHAVE_UPDATE_CORES
    -DHAVE_UPDATE_CORE_INFO
    -DHAVE_CC_RESAMPLER
    -DHAVE_KEYMAPPER
    -DHAVE_FLAC
    -DHAVE_DR_FLAC
    -DHAVE_DR_MP3
    -DHAVE_CHD
    -DWANT_SUBCODE
    -DWANT_RAW_DATA_SECTOR
    -DHAVE_RUNAHEAD
    -DHAVE_AUDIOMIXER
    -DHAVE_RWAV
    -DHAVE_ACCESSIBILITY
    -DHAVE_TRANSLATE
    -DWANT_IFADDRS
    -DHAVE_XDELTA
    -DHAVE_CORE_INFO_CACHE
    -DHAVE_BUILTINMBEDTLS
    -DHAVE_SSL
    -DHAVE_7ZIP
    -D_7ZIP_ST
    -DHAVE_SL
    -DFLAC_PACKAGE_VERSION="retroarch"
    -DHAVE_LROUND
    -DFLAC__HAS_OGG=0
)
if(HAVE_GFX_WIDGETS)
    add_definitions(-DHAVE_GFX_WIDGETS)
endif()
if(HAVE_VULKAN)
    add_definitions(
        -DHAVE_VULKAN
        -DHAVE_SLANG
        -DHAVE_GLSLANG
        -DHAVE_BUILTINGLSLANG
        -DHAVE_SPIRV_CROSS
        -DWANT_GLSLANG
        -D__STDC_LIMIT_MACROS
    )
endif()
if(HAVE_CHEEVOS)
    add_definitions(-DHAVE_CHEEVOS -DRC_DISABLE_LUA)
endif()

if(HAVE_SAF)
    add_definitions(-DHAVE_SAF)
endif()

if(HAVE_LOGGER)
    add_definitions(-DHAVE_LOGGER)
endif()
if(GIT_VERSION)
    message("当前git版本"${GIT_VERSION})
    add_definitions(-DHAVE_GIT_VERSION -DGIT_VERSION=${GIT_VERSION})
endif()
# ohos support GLESv3
set(GLES_LIB GLESv3)
add_definitions(-DHAVE_OPENGLES3)

# SMB 客户端支持
if(HAVE_BUILTINSMBCLIENT)
    add_definitions(
        -DHAVE_BUILTINSMBCLIENT
        -D_U_=__attribute__\(\(unused\)\)
        -DHAVE_TIME_H
        -DHAVE_FCNTL_H
        -DHAVE_UNISTD_H
        -DHAVE_STDLIB_H
        -DSTDC_HEADERS
        -DHAVE_STRING_H
        -DHAVE_LINGER
        -DHAVE_SYS_UIO_H
        -DHAVE_POLL_H
        -DHAVE_NETDB_H
        -DHAVE_NETINET_TCP_H
        -DHAVE_NETINET_IN_H
        -DHAVE_SYS_SOCKET_H
        -DHAVE_ARPA_INET_H
        -DHAVE_SMBCLIENT
    )
endif()

# 源文件
set(SOURCES
    ${RARCH_DIR}/griffin/griffin.c
    ${RARCH_DIR}/griffin/griffin_cpp.cpp
)
if(HAVE_VULKAN)
    list(APPEND SOURCES ${RARCH_DIR}/griffin/griffin_glslang.cpp)
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${RARCH_DIR}/libretro-common/include
    ${RARCH_DIR}/deps
    ${RARCH_DIR}/deps/stb
    ${RARCH_DIR}/deps/7zip
    ${RARCH_DIR}/deps/zstd/lib
    ${DEPS_DIR}/stb
    ${DEPS_DIR}/7zip
    ${DEPS_DIR}/zstd/lib
    ${DEPS_DIR}/libFLAC/include
)
if(HAVE_CHEEVOS)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${DEPS_DIR}/rcheevos/include
    )
endif()

if(HAVE_BUILTINSMBCLIENT)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${DEPS_DIR}/libsmb2/include
        ${DEPS_DIR}/libsmb2/include/smb2
    )
endif()
if(HAVE_VULKAN)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${RARCH_DIR}/gfx/include
        ${DEPS_DIR}/glslang
        ${DEPS_DIR}/glslang/glslang/glslang/Public
        ${DEPS_DIR}/glslang/glslang/glslang/MachineIndependent
        ${DEPS_DIR}/glslang/glslang/SPIRV
        ${DEPS_DIR}/SPIRV-Cross
    )
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    #-std=gnu99
    -pthread
    -Wno-unused-function
    -fno-stack-protector
    -funroll-loops
    -Wno-sign-compare
    -Wno-unused-variable
    -Wno-parentheses
)

if(HAVE_LOGGER)
    find_library(LOG_LIB hilog_ndk.z)
    target_link_libraries(${PROJECT_NAME} ${LOG_LIB})
endif()

target_link_libraries(${PROJECT_NAME}
    ace_napi.z
    ace_ndk.z
    GLESv3
    EGL
    OpenSLES
    z
    native_window
    hilog_ndk.z
)
if(DEFINED SANITIZER)
    target_compile_options(${PROJECT_NAME} PRIVATE -g -fsanitize=${SANITIZER} -fno-omit-frame-pointer)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=${SANITIZER})
endif()

