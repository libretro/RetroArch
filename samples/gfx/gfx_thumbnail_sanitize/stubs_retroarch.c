/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stubs_retroarch.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Stubs for everything gfx/gfx_thumbnail.c reaches outside itself, so
 * it can be exercised in isolation under the sanitizers.
 *
 * Two of these are load-bearing rather than inert.
 *
 * task_push_image_load() records the request instead of queueing it,
 * so the test can fire the callback itself, on a thread of its
 * choosing.  That is what makes the producer/consumer pairing
 * reachable by TSan at all.
 *
 * image_texture_free() really frees the pixel buffer, because that is
 * what the real one does and it owns it.  Stubbing it as a no-op made
 * LSan report 80 KB leaked against the harness, which would have
 * masked a leak in the code under test.
 *
 * Signatures are copied from the headers rather than guessed; a stub
 * with the wrong prototype is a different function that happens to
 * link. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>

#include <queues/task_queue.h>
#include <formats/image.h>

/* --- captured image-load request --- */
typedef struct
{
   retro_task_callback_t cb;
   void                 *user_data;
   uint32_t              type_hash;
   char                  path[4096];
   int                   pending;
} stub_image_request_t;

stub_image_request_t g_stub_image_req;
int                  g_stub_texture_loads;
int                  g_stub_texture_unloads;

/* --- logging --- */
int g_stub_quiet = 1;
void RARCH_LOG (const char *fmt, ...) { if (!g_stub_quiet) { va_list a; va_start(a,fmt); vfprintf(stderr,fmt,a); va_end(a);} }
void RARCH_ERR (const char *fmt, ...) { if (!g_stub_quiet) { va_list a; va_start(a,fmt); vfprintf(stderr,fmt,a); va_end(a);} }
void RARCH_WARN(const char *fmt, ...) { if (!g_stub_quiet) { va_list a; va_start(a,fmt); vfprintf(stderr,fmt,a); va_end(a);} }
void RARCH_DBG (const char *fmt, ...) { (void)fmt; }

/* --- singletons --- */
static char s_settings[1 << 18];
static char s_menu_state[1 << 16];
static char s_video_state[1 << 16];
static char s_runloop_state[1 << 18];
static char s_disp[1 << 16];

void *config_get_ptr(void)      { return s_settings; }
void *menu_state_get_ptr(void)  { return s_menu_state; }
void *video_state_get_ptr(void) { return s_video_state; }
void *runloop_state_get_ptr(void) { return s_runloop_state; }
void *disp_get_ptr(void)        { return s_disp; }

const char *msg_hash_to_str(int e) { (void)e; return "stub"; }

/* --- video driver --- */
bool video_driver_texture_load(void *data, unsigned filter, uintptr_t *id)
{
   (void)data; (void)filter;
   g_stub_texture_loads++;
   /* Non-zero, and distinct per load so a stale handle is visible. */
   *id = (uintptr_t)(0x1000 + g_stub_texture_loads);
   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   (void)id;
   g_stub_texture_unloads++;
   return true;
}

void video_driver_get_video_output_size(unsigned *w, unsigned *h,
      char *d, size_t l)
{
   (void)d; (void)l;
   if (w)
      *w = 1920;
   if (h)
      *h = 1080;
}

bool video_driver_get_viewport_info(void *vp) { (void)vp; return false; }
uint32_t video_driver_get_disp_flags(void) { return 0; }

/* --- gfx --- */
void gfx_animation_kill_by_tag(uintptr_t *tag) { (void)tag; }
bool gfx_animation_push(void *entry) { (void)entry; return true; }
void gfx_display_rotate_z(void *draw, void *data) { (void)draw; (void)data; }
unsigned gfx_display_texture_filter(void) { return 0; }

/* --- image loading --- */
bool task_push_image_load(const char *fullpath, bool supports_rgba,
      unsigned upscale_threshold, uint32_t type_hash,
      retro_task_callback_t cb, void *user_data)
{
   (void)supports_rgba; (void)upscale_threshold;
   memset(&g_stub_image_req, 0, sizeof(g_stub_image_req));
   if (fullpath)
      strncpy(g_stub_image_req.path, fullpath,
            sizeof(g_stub_image_req.path) - 1);
   g_stub_image_req.cb        = cb;
   g_stub_image_req.user_data = user_data;
   g_stub_image_req.type_hash = type_hash;
   g_stub_image_req.pending   = 1;
   return true;
}

/* Matches the real one closely enough for ownership purposes: the
 * pixel buffer belongs to the image, and freeing it here is what makes
 * any remaining leak attributable to gfx_thumbnail.c rather than to
 * the harness. */
void image_texture_free(struct texture_image *img)
{
   if (!img)
      return;
   if (img->pixels)
      free(img->pixels);
   img->pixels = NULL;
}
enum image_type_enum image_texture_get_type(const char *path)
{ (void)path; return IMAGE_TYPE_NONE; }
bool rpng_is_apng_ex(const char *path, void *o) { (void)path; (void)o; return false; }

/* --- animated stream: never entered here, the harness feeds still
 * images, but the symbols have to resolve.  Signatures copied from
 * libretro-common/include/formats/image.h rather than guessed --
 * getting them wrong is how a stub silently becomes a different
 * function. --- */
void *image_transfer_anim_stream_new(void *buf, size_t len,
      enum image_type_enum type)
{ (void)buf; (void)len; (void)type; return NULL; }

void *image_transfer_anim_stream_new_avail(void *buf, size_t len,
      size_t avail, enum image_type_enum type, int *need_more)
{
   (void)buf; (void)len; (void)avail; (void)type;
   if (need_more)
      *need_more = 0;
   return NULL;
}

void image_transfer_anim_stream_free(void *stream,
      enum image_type_enum type)
{ (void)stream; (void)type; }

void image_transfer_anim_stream_get_info(void *stream,
      enum image_type_enum type, unsigned *width, unsigned *height,
      int *num_frames, int *loop_count)
{
   (void)stream; (void)type;
   if (width)
      *width = 0;
   if (height)
      *height = 0;
   if (num_frames)
      *num_frames = 0;
   if (loop_count)
      *loop_count = 0;
}

const uint32_t *image_transfer_anim_stream_next(void *stream,
      enum image_type_enum type, int *duration_ms)
{
   (void)stream; (void)type;
   if (duration_ms)
      *duration_ms = 0;
   return NULL;
}

bool image_transfer_anim_stream_set_argb(void *stream,
      enum image_type_enum type, int argb)
{ (void)stream; (void)type; (void)argb; return false; }

void image_transfer_anim_stream_set_avail(void *stream,
      enum image_type_enum type, size_t avail)
{ (void)stream; (void)type; (void)avail; }

size_t image_transfer_anim_stream_media_floor(void *stream,
      enum image_type_enum type)
{ (void)stream; (void)type; return 0; }

size_t image_transfer_anim_stream_consumed(void *stream,
      enum image_type_enum type)
{ (void)stream; (void)type; return 0; }

void image_transfer_anim_stream_complete_scan(void *stream,
      enum image_type_enum type, const void *buf, size_t len)
{ (void)stream; (void)type; (void)buf; (void)len; }

void image_transfer_anim_stream_rewind(void *stream,
      enum image_type_enum type)
{ (void)stream; (void)type; }

void task_image_detach_video_stream(void *t) { (void)t; }

/* --- data transfer window --- */
void  data_transfer_complete(void *t) { (void)t; }
void  data_transfer_failed(void *t) { (void)t; }
void  data_transfer_free(void *t) { (void)t; }
bool  data_transfer_iterate_while(void *t) { (void)t; return false; }
void *data_transfer_open_window(void *t, size_t n) { (void)t; (void)n; return NULL; }
bool  data_transfer_reserve_supported(void *t) { (void)t; return false; }
void *data_transfer_window_base(void *t) { (void)t; return NULL; }
bool  data_transfer_window_extend(void *t, size_t n) { (void)t; (void)n; return false; }
void  data_transfer_window_feed(void *t, size_t n) { (void)t; (void)n; }
bool  data_transfer_window_is_reserved(void *t) { (void)t; return false; }
void  mem_stats_free(void *s) { (void)s; }

/* --- task queue --- */
bool task_queue_find(task_finder_data_t *find_data)
{ (void)find_data; return false; }
void task_set_flags(retro_task_t *t, uint8_t f, bool set)
{ (void)t; (void)f; (void)set; }
bool  task_push_pl_entry_thumbnail_download(const char *sys,
      void *pl, unsigned idx, bool over, bool mute)
{ (void)sys; (void)pl; (void)idx; (void)over; (void)mute; return false; }

/* --- audio mixer --- */
bool audio_driver_mixer_add_stream(void *p) { (void)p; return false; }
void audio_driver_mixer_remove_stream(unsigned i) { (void)i; }
const char *audio_driver_mixer_get_stream_name(unsigned i) { (void)i; return ""; }

/* --- playlist --- */
void *playlist_get_cached(void) { return NULL; }
size_t playlist_get_size(void *pl) { (void)pl; return 0; }
void playlist_get_index(void *pl, size_t i, const void **e) { (void)pl; (void)i; if (e) *e = NULL; }
const char *playlist_get_conf_path(void *pl) { (void)pl; return ""; }
char *playlist_get_db_name(void *pl) { (void)pl; return NULL; }
int  playlist_get_thumbnail_mode(void *pl, unsigned id) { (void)pl; (void)id; return 0; }
int  playlist_get_curr_thumbnail_name_flag(void *pl) { (void)pl; return 0; }
void playlist_update_thumbnail_name_flag(void *pl, size_t i, int f) { (void)pl; (void)i; (void)f; }
bool playlist_thumbnail_match_with_filename(void *pl) { (void)pl; return false; }

/* --- misc --- */
void runloop_path_set_redirect(void *s, const char *a, const char *b, char *c, size_t d)
{ (void)s; (void)a; (void)b; (void)c; (void)d; }
void dir_set(unsigned id, const char *path) { (void)id; (void)path; }

/* path_is_media_type lives in file_path_special.c, which drags in the
 * frontend; the thumbnail code only asks so it can reject non-images. */
enum rarch_content_type { RARCH_CONTENT_NONE = 0, RARCH_CONTENT_IMAGE = 3 };
int path_is_media_type(const char *path)
{
   const char *ext = strrchr(path ? path : "", '.');
   if (ext && (!strcmp(ext, ".png") || !strcmp(ext, ".jpg")))
      return RARCH_CONTENT_IMAGE;
   return RARCH_CONTENT_NONE;
}

/* CDROM VFS is compiled out here. */
void *retro_vfs_file_open_cdrom(void *p, const char *a, unsigned b, unsigned c)
{ (void)p; (void)a; (void)b; (void)c; return NULL; }
int   retro_vfs_file_close_cdrom(void *s) { (void)s; return -1; }
int64_t retro_vfs_file_read_cdrom(void *s, void *b, uint64_t n)
{ (void)s; (void)b; (void)n; return -1; }
int64_t retro_vfs_file_seek_cdrom(void *s, int64_t o, int w)
{ (void)s; (void)o; (void)w; return -1; }
int64_t retro_vfs_file_tell_cdrom(void *s) { (void)s; return -1; }
int   retro_vfs_file_error_cdrom(void *s) { (void)s; return -1; }
