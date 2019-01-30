add_library (mmal_core ${LIBRARY_TYPE}
   mmal_format.c
   mmal_port.c
   mmal_port_clock.c
   mmal_component.c
   mmal_buffer.c
   mmal_queue.c
   mmal_pool.c
   mmal_events.c
   mmal_logging.c
   mmal_clock.c
)

target_link_libraries (mmal_core vcos)

install(TARGETS mmal_core DESTINATION lib)
install(FILES
   mmal_buffer_private.h
   mmal_clock_private.h
   mmal_component_private.h
   mmal_core_private.h
   mmal_port_private.h
   mmal_events_private.h
   DESTINATION include/interface/mmal/core
)
