add_library (mmal_util ${LIBRARY_TYPE}
   mmal_il.c
   mmal_util.c
   mmal_connection.c
   mmal_graph.c
   mmal_list.c
   mmal_param_convert.c
   mmal_util_params.c
   mmal_component_wrapper.c
   mmal_util_rational.c
)

target_link_libraries (mmal_util vcos)

install(TARGETS mmal_util DESTINATION lib)
install(FILES
   mmal_component_wrapper.h
   mmal_connection.h
   mmal_default_components.h
   mmal_graph.h
   mmal_il.h
   mmal_list.h
   mmal_param_convert.h
   mmal_util.h
   mmal_util_params.h
   mmal_util_rational.h
   DESTINATION include/interface/mmal/util
)
