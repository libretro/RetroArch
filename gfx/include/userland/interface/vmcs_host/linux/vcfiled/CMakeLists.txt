
add_definitions(-Werror)

# vcfiled - serves files to videocore. used for media handlers from
# OpenMAX/IL and loading VLLs.
add_executable(vcfiled vcfiled.c)

# library to check if vcfiled is running or not
add_library(vcfiled_check vcfiled_check.c)

target_link_libraries(vcfiled
		      vcfiled_check
                      vchostif
                      vchiq_arm
                      vcos)

install(TARGETS vcfiled 
        RUNTIME DESTINATION sbin)

configure_file (etc/init.d/vcfiled ${PROJECT_BINARY_DIR}/etc/init.d/vcfiled)

# script to start up vcfiled at start of day
install(PROGRAMS ${PROJECT_BINARY_DIR}/etc/init.d/vcfiled
        DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/init.d)
# install locally to the installation directory too
install(PROGRAMS ${PROJECT_BINARY_DIR}/etc/init.d/vcfiled
        DESTINATION ${VMCS_INSTALL_PREFIX}/share/install)

# test program for vcfiled_check library
add_executable(vcfiled_lock_test vcfiled_lock_test.c)
target_link_libraries(vcfiled_lock_test vcfiled_check)

install(TARGETS vcfiled_check DESTINATION lib)
