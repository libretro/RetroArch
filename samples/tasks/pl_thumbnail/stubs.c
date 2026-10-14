/* The frontend surface tasks/task_pl_thumbnail_download.c references
 * beyond what the test drives.  The per-entry handler the lanes run
 * starts on a handle the test builds, so none of these is reached;
 * they stand in only so the unit links. */
#include <stdlib.h>

#include <boolean.h>
#include <queues/message_queue.h>

#include "../../../configuration.h"
#include "../../../playlist.h"
#include "../../../gfx/gfx_thumbnail.h"

static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
}

gfx_thumbnail_path_data_t *gfx_thumbnail_path_init(void)
{ return (gfx_thumbnail_path_data_t*)calloc(1, sizeof(gfx_thumbnail_path_data_t)); }
bool gfx_thumbnail_set_system(gfx_thumbnail_path_data_t *path_data,
      const char *system, playlist_t *playlist)
{ (void)path_data; (void)system; (void)playlist; return false; }
bool gfx_thumbnail_set_content_playlist(gfx_thumbnail_path_data_t *path_data,
      playlist_t *playlist, size_t idx)
{ (void)path_data; (void)playlist; (void)idx; return false; }
void gfx_thumbnail_dir_config_capture(gfx_thumbnail_dir_config_t *cfg)
{ (void)cfg; }
bool gfx_thumbnail_update_path_cfg(gfx_thumbnail_path_data_t *path_data,
      enum gfx_thumbnail_id thumbnail_id,
      const gfx_thumbnail_dir_config_t *cfg)
{ (void)path_data; (void)thumbnail_id; (void)cfg; return false; }
size_t gfx_thumbnail_get_content_dir(gfx_thumbnail_path_data_t *path_data,
      char *s, size_t len)
{ (void)path_data; (void)s; (void)len; return 0; }

bool playlist_config_copy(const playlist_config_t *src, playlist_config_t *dst)
{ (void)src; (void)dst; return false; }
void playlist_free(playlist_t *playlist) { (void)playlist; }
size_t playlist_size(playlist_t *playlist) { (void)playlist; return 0; }
void playlist_update_thumbnail_name_flag(playlist_t *playlist, size_t idx,
     enum playlist_thumbnail_name_flags thumbnail_flags)
{ (void)playlist; (void)idx; (void)thumbnail_flags; }
enum playlist_thumbnail_name_flags playlist_get_next_thumbnail_name_flag(
      playlist_t *playlist, size_t idx)
{ (void)playlist; (void)idx; return PLAYLIST_THUMBNAIL_FLAG_NONE; }
char *playlist_get_conf_path(playlist_t *playlist)
{ (void)playlist; return NULL; }
