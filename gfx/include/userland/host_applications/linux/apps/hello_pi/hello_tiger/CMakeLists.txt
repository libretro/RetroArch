set(EXEC hello_tiger.bin)
set(SRCS main.c tiger.c)
add_definitions(-D__RASPBERRYPI__)

add_executable(${EXEC} ${SRCS})
target_link_libraries(${EXEC} ${HELLO_PI_LIBS})

install(TARGETS ${EXEC}
        RUNTIME DESTINATION bin)
