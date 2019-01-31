set(EXEC hello_triangle2.bin)
set(SRCS triangle2.c)

add_executable(${EXEC} ${SRCS})
target_link_libraries(${EXEC} ${HELLO_PI_LIBS})

install(TARGETS ${EXEC}
        RUNTIME DESTINATION bin)
