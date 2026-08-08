set(SOURCES init.c
            krb5-wrapper.c
            libnfs.c
            libnfs-sync.c
            libnfs-zdr.c
            multithreading.c
            nfs_v3.c
            nfs_v4.c
            pdu.c
            socket.c
)

# deal with version info in "dll" case
if(WIN32 AND BUILD_SHARED_LIBS)
  configure_file(../win32/version.rc.template version.rc @ONLY)
  list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/version.rc libnfs-win32.def)
endif()

add_library(nfs ${SOURCES})
target_link_libraries(nfs PRIVATE ${CORE_LIBRARIES} PUBLIC ${SYSTEM_LIBRARIES})

# Set version properties for the library
set_target_properties(nfs PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${SOVERSION}
)

# Set the output filename to "libnfs" consistently across platforms
set_target_properties(nfs PROPERTIES
    OUTPUT_NAME "libnfs"  # Ensures consistent output name (e.g., libnfs.dll on Windows, libnfs.so on Unix)
    PREFIX ""             # Removes the "lib" prefix on Unix-like systems to avoid "liblibnfs.so"
)


install(TARGETS nfs EXPORT libnfs
                    RUNTIME DESTINATION bin
                    ARCHIVE DESTINATION lib
                    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
