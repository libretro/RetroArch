add_executable(tvservice tvservice.c)
target_link_libraries(tvservice vchostif)

install(TARGETS tvservice
        RUNTIME DESTINATION bin)
