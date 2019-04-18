set(EXEC hello_font.bin)
set(SRCS main.c)

add_executable(${EXEC} ${SRCS})
target_link_libraries(${EXEC} ${HELLO_PI_LIBS})
target_link_libraries(${EXEC} vgfont freetype z)

install(TARGETS ${EXEC}
        RUNTIME DESTINATION bin)
