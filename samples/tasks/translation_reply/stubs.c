/* What tasks/task_translation.c links against outside the reply's
 * image path: none of it runs in these lanes, so each is an empty
 * stand-in, declared without the headers since only its name has to
 * resolve. */

#include <stddef.h>

int  accessibility_speak_priority(void) { return 0; }
int  audio_driver_is_ai_service_speech_running(void) { return 0; }
int  audio_driver_mixer_add_stream(void) { return 0; }
void *base64(void) { return NULL; }
int  command_event(void) { return 0; }
void *core_info_get_current_core(void) { return NULL; }
void *frontend_state_get_ptr(void) { return NULL; }
int  input_driver_ai_gamepad_held(void) { return 0; }
void input_driver_ai_gamepad_press(void) { }
int  is_accessibility_enabled(void) { return 0; }
const char *path_basename(const char *p) { return p; }
const char *path_get(int t) { (void)t; return ""; }
void *playlist_get_cached(void) { return NULL; }
void playlist_get_index_by_path(void) { }
void rjson_free(void) { }
int  rjson_get_context_count(void) { return 0; }
int  rjson_get_context_type(void) { return 0; }
const char *rjson_get_string(void) { return NULL; }
int  rjson_next(void) { return 0; }
void *rjson_open_buffer(void) { return NULL; }
void rjsonwriter_add_string(void) { }
void rjsonwriter_add_string_len(void) { }
int  rjsonwriter_free(void) { return 0; }
char *rjsonwriter_get_memory_buffer(void) { return NULL; }
void *rjsonwriter_open_memory(void) { return NULL; }
void rjsonwriter_raw(void) { }
void rjsonwriter_rawf(void) { }
void *rpng_save_image_bgr24_string(void) { return NULL; }
void runloop_msg_queue_push(void) { }
int  task_get_flags(void) { return 0; }
void *task_init(void) { return NULL; }
void *task_push_http_post_transfer(void) { return NULL; }
int  task_queue_push(void) { return 0; }
void task_set_flags(void) { }
void *unbase64(void) { return NULL; }
int  video_driver_cached_frame_read(void) { return 0; }
int  video_driver_get_viewport_info(void) { return 0; }
