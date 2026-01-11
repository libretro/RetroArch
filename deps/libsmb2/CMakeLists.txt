if(NOT ESP_PLATFORM)
  
  if(CMAKE_SYSTEM_NAME MATCHES NintendoSwitch OR CMAKE_SYSTEM_NAME MATCHES Nintendo3DS OR CMAKE_SYSTEM_NAME MATCHES Dreamcast OR CMAKE_SYSTEM_NAME MATCHES NintendoGameCube OR CMAKE_SYSTEM_NAME MATCHES NintendoDS OR CMAKE_SYSTEM_NAME MATCHES NintendoWii OR CMAKE_SYSTEM_NAME MATCHES CafeOS)
    cmake_minimum_required(VERSION 3.13)
  elseif(VITA)
    cmake_minimum_required(VERSION 3.2)  
  elseif(PICO_BOARD)
    cmake_minimum_required(VERSION 3.12)
    message(STATUS "Configuring libsmb2 for ${PICO_BOARD}")
  else()
    cmake_minimum_required(VERSION 3.5)
  endif()

  if(NOT IOP AND BUILD_IRX)
    project(smb2man
            LANGUAGES C
            VERSION 2.3.0
    )
    set(VERSION 2.3.0)    
    set(PACKAGE "smb2man")
    set(PACKAGE_BUGREPORT "ronniesahlberg@gmail.com or andregui17@outlook.com")
    set(PACKAGE_NAME "smb2man")
    set(PACKAGE_VERSION "${VERSION}")
    set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
    set(PACKAGE_TARNAME "${PACKAGE}")
    set(PACKAGE_URL "https://github.com/sahlberg/libsmb2")
  elseif(PICO_BOARD)
    project(libsmb2 C CXX ASM)
  else()
    project(libsmb2
            LANGUAGES C
            VERSION 6.1.0
    )  
    set(VERSION 6.1.0)
    set(PACKAGE "libsmb2")
    set(PACKAGE_BUGREPORT "ronniesahlberg@gmail.com")
    set(PACKAGE_NAME "libsmb2")
    set(PACKAGE_VERSION "${VERSION}")
    set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
    set(PACKAGE_TARNAME "${PACKAGE}")
    set(PACKAGE_URL "https://github.com/sahlberg/libsmb2")
  endif()
  configure_file(libsmb2.pc.in libsmb2.pc @ONLY)
endif()

  if(NOT ESP_PLATFORM)
    set(SOVERSION 1 CACHE STRING "" FORCE)
    if(NOT IOP AND NOT BUILD_IRX)
      set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE PATH "Installation directory for libraries")
      set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")
      set(INSTALL_PKGCONFIG_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/pkgconfig" CACHE PATH "Installation directory for pkgconfig (.pc) files")
      set(INSTALL_CMAKE_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}/cmake/libsmb2" CACHE PATH "Installation directory for cmake (.cmake) files")
    endif()
    if(CMAKE_SYSTEM_NAME MATCHES NintendoSwitch OR CMAKE_SYSTEM_NAME MATCHES Nintendo3DS OR PS4 OR PS2 OR VITA OR CMAKE_SYSTEM_NAME MATCHES Dreamcast OR CMAKE_SYSTEM_NAME MATCHES NintendoGameCube OR CMAKE_SYSTEM_NAME MATCHES NintendoDS OR CMAKE_SYSTEM_NAME MATCHES NintendoWii OR CMAKE_SYSTEM_NAME MATCHES CafeOS)
      option(BUILD_SHARED_LIBS "Build shared libraries" OFF)  
    else()
      option(BUILD_SHARED_LIBS "Build shared libraries" ON)
    endif()
      option(ENABLE_EXAMPLES "Build example programs" OFF)
      option(ENABLE_LIBKRB5 "Enable libkrb5 support" ON)
      option(ENABLE_GSSAPI "Enable gssapi support" ON)
      list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
  endif()

  if(CMAKE_SYSTEM_NAME MATCHES Linux)
    if (ENABLE_LIBKRB5)
      find_package(LibKrb5)
    endif()
  elseif(IOS)
    if (ENABLE_GSSAPI)
      find_package(GSSAPI)  
    endif()
  endif()

  if(NOT ESP_PLATFORM)
      include(cmake/ConfigureChecks.cmake)
  endif()

  if(PICO_BOARD)
      include_directories(libsmb2 STATIC
            ${PICO_SDK_PATH}/lib/lwip/src/include
            ${PICO_SDK_PATH}/src/rp2_common/pico_lwip/include
            lib
            include
            include/smb2
            include/picow
    )
    elseif(ESP_PLATFORM)
      set(COMPONENT_ADD_INCLUDEDIRS
      "lib"
      include
      include/smb2
      include/esp
     )
    else()
      include_directories(${CMAKE_CURRENT_BINARY_DIR}
                        include
                        include/smb2
      )
    endif()
    
    if(CMAKE_SYSTEM_NAME MATCHES Linux)
      if (ENABLE_LIBKRB5)
        set(core_DEPENDS ${LIBKRB5_LIBRARY} CACHE STRING "" FORCE)  
      endif()
    elseif(IOS)
      if (ENABLE_GSSAPI)
        set(core_DEPENDS ${GSSAPI_LIBRARIES} CACHE STRING "" FORCE)   
      endif()
    endif()

    if(MSVC AND BUILD_SHARED_LIBS)
      # If we are building dll with msvc, then generate a def file according to the syms file
      set(SYMS_FILE_PATH "${PROJECT_SOURCE_DIR}/lib/libsmb2.syms")
      set(DEF_FILE_PATH "${PROJECT_BINARY_DIR}/lib/libsmb2.def")
      file(READ ${SYMS_FILE_PATH} EXPORT_SYMBOLS)
      file(WRITE ${DEF_FILE_PATH} "LIBRARY smb2\nEXPORTS\n\n${EXPORT_SYMBOLS}")
      # Pass def file to the linker
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEF:${DEF_FILE_PATH}")
    endif()

    if(CMAKE_SYSTEM_NAME MATCHES Windows)
      list(APPEND CORE_LIBRARIES ws2_32.lib)
      add_definitions(-DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS -DHAVE_LINGER -DNEED_GETLOGIN_R -DNEED_GETPID -DNEED_RANDOM -DNEED_SRANDOM)
    elseif(CMAKE_SYSTEM_NAME MATCHES NintendoSwitch)
      add_definitions(-DNEED_READV -DNEED_WRITEV -DNEED_GETLOGIN_R)
    elseif(CMAKE_SYSTEM_NAME MATCHES Nintendo3DS)
      add_definitions(-DARM11 -D_3DS -D__3DS__ -DNEED_READV -DNEED_WRITEV -DNEED_GETLOGIN_R)
    elseif(CMAKE_SYSTEM_NAME MATCHES NintendoDS)
      add_definitions(-DNEED_READV -DHAVE_SOCKADDR_STORAGE -DNEED_WRITEV -DNEED_GETLOGIN_R -DNEED_GETADDRINFO -DNEED_FREEADDRINFO -DNEED_POLL)
    elseif(CMAKE_SYSTEM_NAME MATCHES NintendoGameCube)      
      add_definitions(-DNEED_READV -DNEED_WRITEV -DNEED_GETLOGIN_R -DNEED_GETADDRINFO -DNEED_FREEADDRINFO -DNEED_POLL)
    elseif(CMAKE_SYSTEM_NAME MATCHES NintendoWii)
      add_definitions(-DNEED_READV -DNEED_WRITEV -DNEED_GETLOGIN_R -DNEED_GETADDRINFO -DNEED_FREEADDRINFO -DNEED_POLL)
    elseif(CMAKE_SYSTEM_NAME MATCHES CafeOS)
      add_definitions(-DNEED_READV -DNEED_WRITEV -DNEED_GETLOGIN_R)
    elseif(PS4)
      add_definitions(-DPS4_PLATFORM)
      set(CMAKE_C_FLAGS "-funwind-tables")
      #-fuse-init-array -emit-llvm broken...
      set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")    
      add_compile_options("${CMAKE_C_FLAGS}")
    elseif(EE)
      add_definitions(-DNEED_BE64TOH -DNEED_POLL -DNEED_WRITEV -DNEED_READV -DNEED_GETLOGIN_R)
      if(EE AND PS2RPC)
        add_definitions(-DPS2RPC)
      endif()      
    elseif(IOP)
      add_definitions(-G 0 -DHAVE_CONFIG_H -DNEED_BE64TOH -DNEED_STRDUP -DNEED_READV -DNEED_WRITEV -DNEED_POLL -DNEED_GETPID -DNEED_RANDOM -DNEED_SRANDOM -DNEED_GETLOGIN_R -DHAVE_SYS_FCNTL_H)
      if(IOP AND BUILD_IRX)
        add_definitions(-D__ps2sdk_iop__)
        if(DEBUG)
          add_definitions(-DDEBUG)
        endif()
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -fno-builtin")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")                
      endif()
      set(CMAKE_C_FLAGS "-Wall") 
      include_directories(${CMAKE_CURRENT_BINARY_DIR} include/ps2)
      add_compile_options("${CMAKE_C_FLAGS}")
    elseif(VITA)
      add_definitions(-DNEED_READV -DNEED_WRITEV -DNEED_GETLOGIN_R)  
    elseif(PICO_BOARD)
      add_definitions(-DNEED_BE64TOH -DNEED_POLL -DNEED_GETLOGIN_R)
    elseif(ESP_PLATFORM)    
      if(NOT CMAKE_BUILD_EARLY_EXPANSION)
        add_definitions(-DHAVE_CONFIG_H)
      endif()
      add_definitions(-DNEED_READV -DNEED_WRITEV -DNEED_GETLOGIN_R -DNEED_RANDOM -DNEED_SRANDOM)    
    elseif(CMAKE_SYSTEM_NAME MATCHES Dreamcast)
      add_definitions(-DNEED_WRITEV -DNEED_READV -DHAVE_CONFIG_H -DNEED_GETLOGIN_R)             
    elseif(CMAKE_SYSTEM_NAME STREQUAL Solaris)
      find_library(SOCKET_LIBRARY socket)
      find_library(NSL_LIBRARY nsl)
      list(APPEND CORE_LIBRARIES ${SOCKET_LIBRARY} ${NSL_LIBRARY})
    endif()


    if(ENABLE_EXAMPLES)
      add_subdirectory(examples)
    endif()

    add_subdirectory(lib)

  if(NOT PICO_BOARD OR ESP_PLATFORM)
      include(CMakePackageConfigHelpers)
      write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/libsmb2-config-version.cmake
                                    VERSION ${PROJECT_VERSION}
                                    COMPATIBILITY AnyNewerVersion)
    if(NOT IOP AND NOT BUILD_IRX)
      configure_file(cmake/libsmb2.pc.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/libsmb2.pc @ONLY)

      install(DIRECTORY include/smb2
            DESTINATION ${INSTALL_INC_DIR})

      install(FILES cmake/FindSMB2.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/libsmb2-config-version.cmake
            DESTINATION ${INSTALL_CMAKE_DIR})

      install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libsmb2.pc
            DESTINATION ${INSTALL_PKGCONFIG_DIR})
    endif()
  endif() 

