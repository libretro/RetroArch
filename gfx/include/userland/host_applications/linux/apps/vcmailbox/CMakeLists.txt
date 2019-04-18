add_executable(vcmailbox vcmailbox.c)
target_link_libraries(vcmailbox vchostif)

install(TARGETS vcmailbox
        RUNTIME DESTINATION bin)
