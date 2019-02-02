# setup environment for cross compile to arm-linux

if (DEFINED CMAKE_TOOLCHAIN_FILE)
else()
   message(WARNING
	"  *********************************************************\n"
   	"  *   CMAKE_TOOLCHAIN_FILE not defined                    *\n"
	"  *   This is correct for compiling on the Raspberry Pi   *\n"
	"  *                                                       *\n"
	"  *   If you are cross-compiling on some other machine    *\n"
	"  *   then DELETE the build directory and re-run with:    *\n"
	"  *   -DCMAKE_TOOLCHAIN_FILE=toolchain_file.cmake         *\n"
	"  *                                                       *\n"
   	"  *   Toolchain files are in makefiles/cmake/toolchains.  *\n"
	"  *********************************************************"
       )
endif()

# pull in headers for android
if(ANDROID)
    #
    # work out where android headers and library are
    #

    set(ANDROID_NDK_ROOT $ENV{ANDROID_NDK_ROOT} CACHE INTERNAL "" FORCE)
    set(ANDROID_LIBS $ENV{ANDROID_LIBS} CACHE INTERNAL "" FORCE)
    set(ANDROID_BIONIC $ENV{ANDROID_BIONIC} CACHE INTERNAL "" FORCE)
    set(ANDROID_LDSCRIPTS $ENV{ANDROID_LDSCRIPTS} CACHE INTERNAL "" FORCE)
      
    if("${ANDROID_NDK_ROOT}" STREQUAL "")
        find_program(ANDROID_COMPILER arm-eabi-gcc)
        get_filename_component(ANDROID_BIN ${ANDROID_COMPILER} PATH CACHE)
        find_path(_ANDROID_ROOT Makefile PATHS ${ANDROID_BIN}
                  PATH_SUFFIXES ../../../../..
                  NO_DEFAULT_PATH)
                if("${_ANDROID_ROOT}" STREQUAL "_ANDROID_ROOT-NOTFOUND")
                    set(_ANDROID_ROOT "" CACHE INTERNAL "" FORCE)
                endif()
                if("${_ANDROID_ROOT}" STREQUAL "")
            message(FATAL_ERROR "Cannot find android root directory")
        endif()
        get_filename_component(ANDROID_ROOT ${_ANDROID_ROOT} ABSOLUTE CACHE)
        #
        # top level of cross-compiler target include and lib directory structure
        #
        set(ANDROID_NDK_ROOT
            "${ANDROID_ROOT}/prebuilt/ndk" CACHE INTERNAL "" FORCE)
        set(ANDROID_BIONIC
            "${ANDROID_ROOT}/bionic" CACHE INTERNAL "" FORCE)
        set(ANDROID_LDSCRIPTS
            "${ANDROID_ROOT}/build/core" CACHE INTERNAL "" FORCE)
        set(ANDROID_LIBS
            "${ANDROID_ROOT}/out/target/product/${ANDROID_PRODUCT}/obj/lib"
            CACHE INTERNAL "" FORCE)
    endif()

    if("${ANDROID_NDK_ROOT}" STREQUAL "")
        message(FATAL_ERROR "Cannot find Android NDK root directory")
    endif()
    if("${ANDROID_BIONIC}" STREQUAL "")
        message(FATAL_ERROR "Cannot find Android BIONIC directory")
    endif()
    if("${ANDROID_LDSCRIPTS}" STREQUAL "")
        message(FATAL_ERROR "Cannot find Android LD scripts directory")
    endif()
    
    set(CMAKE_SYSTEM_PREFIX_PATH "${ANDROID_NDK_ROOT}/android-ndk-r${ANDROID_NDK_RELEASE}/platforms/android-${ANDROID_NDK_PLATFORM}/arch-${CMAKE_SYSTEM_PROCESSOR}/usr")
    
    if("${ANDROID_LIBS}" STREQUAL "")
        set(ANDROID_LIBS "${CMAKE_SYSTEM_PREFIX_PATH}/lib"
            CACHE INTERNAL "" FORCE)
        # message(FATAL_ERROR "Cannot find android libraries")
    endif()

    #
    # add include directories for pthreads
    #
    include_directories("${CMAKE_SYSTEM_PREFIX_PATH}/include" BEFORE SYSTEM)
    include_directories("${ANDROID_BIONIC}/libc/include" BEFORE SYSTEM)
    include_directories("${ANDROID_BIONIC}/libc/include/arch-arm/include" BEFORE SYSTEM)
    include_directories("${ANDROID_BIONIC}/libc/kernel/arch-arm" BEFORE SYSTEM)
    include_directories("${ANDROID_BIONIC}/libc/kernel/common" BEFORE SYSTEM)
    include_directories("${ANDROID_BIONIC}/libm/include" BEFORE SYSTEM)
    include_directories("${ANDROID_BIONIC}/libm/include/arch/arm" BEFORE SYSTEM)
    include_directories("${ANDROID_BIONIC}/libstdc++/include" BEFORE SYSTEM)
    

    #
    # Pull in Android link options manually
    #
    set(ANDROID_CRTBEGIN "${ANDROID_LIBS}/crtbegin_dynamic.o")
    set(ANDROID_CRTEND "${ANDROID_LIBS}/crtend_android.o")
    set(CMAKE_SHARED_LINKER_FLAGS "-nostdlib ${ANDROID_CRTBEGIN} -Wl,-Bdynamic -Wl,-T${ANDROID_LDSCRIPTS}/armelf.x")

    link_directories(${ANDROID_LIBS})
    set(CMAKE_EXE_LINKER_FLAGS "-nostdlib ${ANDROID_CRTBEGIN} -nostdlib -Wl,-z,noexecstack") 
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dynamic-linker,/system/bin/linker -Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-T${ANDROID_LDSCRIPTS}/armelf.x -Wl,--gc-sections")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,nocopyreloc -Wl,-z,noexecstack -Wl,--fix-cortex-a8 -Wl,--no-undefined")

    set(CMAKE_C_STANDARD_LIBRARIES "-llog -lc -lgcc ${ANDROID_CRTEND}" CACHE INTERNAL "" FORCE)
    
    set(SHARED "")
else()
    set(SHARED "SHARED")
endif()


# All linux systems have sbrk()
add_definitions(-D_HAVE_SBRK)

# pull in declarations of lseek64 and friends
add_definitions(-D_LARGEFILE64_SOURCE)
	
# test for glibc malloc debugging extensions
try_compile(HAVE_MTRACE
            ${CMAKE_BINARY_DIR}
            ${PROJECT_SOURCE_DIR}/makefiles/cmake/srcs/test-mtrace.c
            OUTPUT_VARIABLE foo)

# test for existence of execinfo.h header
include(CheckIncludeFile)
check_include_file(execinfo.h HAVE_EXECINFO_H)

add_definitions(-DHAVE_CMAKE_CONFIG)
configure_file (
    "makefiles/cmake/cmake_config.h.in"
    "${PROJECT_BINARY_DIR}/cmake_config.h"
    )
 
