/* verbosity.c's console attach/detach hooks reach the frontend
 * layer; the harness links video_filter's world, not the frontend's. */
void frontend_driver_attach_console(void) { }
void frontend_driver_detach_console(void) { }
