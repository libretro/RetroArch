
#ifndef RA_WAYLAND_MODULE
#define RA_WAYLAND_MODULE(modname)
#endif

#ifndef RA_WAYLAND_SYM
#define RA_WAYLAND_SYM(rc,fn,params)
#endif

#ifndef RA_WAYLAND_INTERFACE
#define RA_WAYLAND_INTERFACE(iface)
#endif

#include <stdbool.h>

#ifdef HAVE_LIBDECOR_H
RA_WAYLAND_MODULE(WAYLAND_LIBDECOR)
RA_WAYLAND_SYM(void, libdecor_unref, (struct libdecor *))
RA_WAYLAND_SYM(struct libdecor *, libdecor_new, (struct wl_display *, struct libdecor_interface *))
RA_WAYLAND_SYM(int, libdecor_dispatch, (struct libdecor *, int timeout))
RA_WAYLAND_SYM(struct libdecor_frame *, libdecor_decorate, (struct libdecor *,\
                                                             struct wl_surface *,\
                                                             struct libdecor_frame_interface *,\
                                                             void *))
RA_WAYLAND_SYM(void, libdecor_frame_unref, (struct libdecor_frame *))
RA_WAYLAND_SYM(void, libdecor_frame_set_title, (struct libdecor_frame *, const char *))
RA_WAYLAND_SYM(void, libdecor_frame_set_app_id, (struct libdecor_frame *, const char *))
RA_WAYLAND_SYM(void, libdecor_frame_set_max_content_size, (struct libdecor_frame *frame,\
                                                            int content_width,\
                                                            int content_height))
RA_WAYLAND_SYM(void, libdecor_frame_set_min_content_size, (struct libdecor_frame *frame,\
                                                            int content_width,\
                                                            int content_height))
RA_WAYLAND_SYM(void, libdecor_frame_resize, (struct libdecor_frame *,\
                                              struct wl_seat *,\
                                              uint32_t,\
                                              enum libdecor_resize_edge))
RA_WAYLAND_SYM(void, libdecor_frame_move, (struct libdecor_frame *,\
                                            struct wl_seat *,\
                                            uint32_t))
RA_WAYLAND_SYM(void, libdecor_frame_commit, (struct libdecor_frame *,\
                                              struct libdecor_state *,\
                                              struct libdecor_configuration *))
RA_WAYLAND_SYM(void, libdecor_frame_set_minimized, (struct libdecor_frame *))
RA_WAYLAND_SYM(void, libdecor_frame_set_maximized, (struct libdecor_frame *))
RA_WAYLAND_SYM(void, libdecor_frame_unset_maximized, (struct libdecor_frame *))
RA_WAYLAND_SYM(void, libdecor_frame_set_fullscreen, (struct libdecor_frame *, struct wl_output *))
RA_WAYLAND_SYM(void, libdecor_frame_unset_fullscreen, (struct libdecor_frame *))
RA_WAYLAND_SYM(void, libdecor_frame_set_capabilities, (struct libdecor_frame *, \
                                                        enum libdecor_capabilities))
RA_WAYLAND_SYM(void, libdecor_frame_unset_capabilities, (struct libdecor_frame *, \
                                                          enum libdecor_capabilities))
RA_WAYLAND_SYM(bool, libdecor_frame_has_capability, (struct libdecor_frame *, \
                                                      enum libdecor_capabilities))
RA_WAYLAND_SYM(void, libdecor_frame_set_visibility, (struct libdecor_frame *, bool))
RA_WAYLAND_SYM(bool, libdecor_frame_is_visible, (struct libdecor_frame *))
RA_WAYLAND_SYM(bool, libdecor_frame_is_floating, (struct libdecor_frame *))
RA_WAYLAND_SYM(void, libdecor_frame_set_parent, (struct libdecor_frame *,\
                                                  struct libdecor_frame *))
RA_WAYLAND_SYM(struct xdg_surface *, libdecor_frame_get_xdg_surface, (struct libdecor_frame *))
RA_WAYLAND_SYM(struct xdg_toplevel *, libdecor_frame_get_xdg_toplevel, (struct libdecor_frame *))
RA_WAYLAND_SYM(void, libdecor_frame_map, (struct libdecor_frame *))
RA_WAYLAND_SYM(struct libdecor_state *, libdecor_state_new, (int, int))
RA_WAYLAND_SYM(void, libdecor_state_free, (struct libdecor_state *))
RA_WAYLAND_SYM(bool, libdecor_configuration_get_content_size, (struct libdecor_configuration *,\
                                                                struct libdecor_frame *,\
                                                                int *,\
                                                                int *))
RA_WAYLAND_SYM(bool, libdecor_configuration_get_window_state, (struct libdecor_configuration *,\
                                                                enum libdecor_window_state *))
#endif

#undef RA_WAYLAND_MODULE
#undef RA_WAYLAND_SYM
#undef RA_WAYLAND_INTERFACE

