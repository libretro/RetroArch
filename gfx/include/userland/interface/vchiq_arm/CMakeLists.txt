
add_library(vchiq_arm SHARED
            vchiq_lib.c vchiq_util.c)

# pull in VCHI cond variable emulation
target_link_libraries(vchiq_arm vcos)

install(TARGETS vchiq_arm DESTINATION lib)
#install(FILES etc/10-vchiq.rules DESTINATION /etc/udev/rules.d)

include_directories(../..)

add_executable(vchiq_test
               vchiq_test.c)

target_link_libraries(vchiq_test
                      vchiq_arm
                      vcos)

install(TARGETS vchiq_test RUNTIME DESTINATION bin)
