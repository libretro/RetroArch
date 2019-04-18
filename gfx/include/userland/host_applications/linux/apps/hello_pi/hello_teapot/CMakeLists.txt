set(EXEC hello_teapot.bin)
set(SRCS triangle.c video.c models.c)

add_executable(${EXEC} ${SRCS})
target_link_libraries(${EXEC} ${HELLO_PI_LIBS})

install(TARGETS ${EXEC}
        RUNTIME DESTINATION bin)
