/* What the two driver files reach for and these tests never call. No
 * header is included, so the types need not match: nothing here runs. */
#include <stdarg.h>
#include <stddef.h>

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }

/* nothing but zeroes, and room for all of the settings */
static char no_settings[8 << 20];
void *config_get_ptr(void) { return no_settings; }
int input_autoconfigure_connect(void) { return 0; }
int input_autoconfigure_disconnect(void) { return 0; }
const char *input_config_get_device_name(unsigned port) { (void)port; return NULL; }
char *input_config_get_device_name_ptr(unsigned port) { static char name[256]; (void)port; return name; }
size_t input_config_get_device_name_size(unsigned port) { (void)port; return 256; }
void *task_init(void) { return NULL; }
int task_queue_push(void *t) { (void)t; return 0; }
void task_set_flags(void *t, unsigned a, int b) { (void)t; (void)a; (void)b; }
unsigned long long video_driver_window_get(void) { return 0; }

void *dylib_load(const char *path) { (void)path; return NULL; }
void *dylib_proc(void *lib, const char *proc) { (void)lib; (void)proc; return NULL; }
void dylib_close(void *lib) { (void)lib; }

#define CMD(name) int name(void) { return 0; }
CMD(input_autoconfigure_connect_ex)
CMD(fill_pathname_join)
CMD(filestream_read_file)
CMD(command_audio_reinit) CMD(command_close_content) CMD(command_drivers_reinit)
CMD(command_get_config_param) CMD(command_get_playlist) CMD(command_get_status)
CMD(command_help) CMD(command_list_cores) CMD(command_list_playlists)
CMD(command_load_content) CMD(command_load_core) CMD(command_load_savefiles)
CMD(command_load_state_slot) CMD(command_play_replay_slot) CMD(command_read_memory)
CMD(command_save_savefiles) CMD(command_save_state_slot) CMD(command_seek_replay)
CMD(command_show_osd_msg) CMD(command_start_core) CMD(command_unload_core)
CMD(command_version) CMD(command_video_reinit) CMD(command_write_memory)
