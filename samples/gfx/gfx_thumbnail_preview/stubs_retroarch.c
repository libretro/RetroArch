/* Stubs letting the real gfx/gfx_thumbnail.c link standalone.
 *
 * Everything that decides thumbnail behaviour - data_transfer,
 * image_transfer, image_texture_get_type, path_*, mem_stats - is the
 * real implementation.  Only the frontend edges are faked: the video
 * driver hands back a fake texture id and counts uploads, the
 * animation driver records fade pushes, and the task queue records
 * whether a still load was ever requested.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "harness.h"

harness_probe_t hp;

/* ---- video driver ---- */
bool video_driver_texture_load(void *data, unsigned filter_type,
      uintptr_t *id)
{
   struct { unsigned w, h; } *img = (void*)data;
   hp.texture_uploads++;
   *id = 0x1000 + hp.texture_uploads;
   hp.last_tex_w = img->w;
   hp.last_tex_h = img->h;
   return true;
}
bool video_driver_texture_unload(uintptr_t *id)
{
   hp.texture_unloads++;
   *id = 0;
   return true;
}
unsigned video_driver_get_disp_flags(void) { return 0; }
void video_driver_get_video_output_size(unsigned *w, unsigned *h, char *d, size_t l)
{ *w = 1920; *h = 1080; (void)d; (void)l; }
void video_driver_get_viewport_info(void *vp) { (void)vp; }
void *video_state_get_ptr(void) { static char b[4096]; return b; }
unsigned gfx_display_texture_filter(void) { return 0; }
void gfx_display_rotate_z(void *a, void *b) { (void)a; (void)b; }
void *disp_get_ptr(void) { static char b[8192]; return b; }

/* ---- animation driver: this is what the fade goes through ---- */
void gfx_animation_push(void *entry)
{
   /* layout-independent: record the push and force completion, which
    * is what the real animation does over fade_duration */
   struct ge { float easing; uintptr_t tag; float duration;
               float target_value; float *subject; void *cb; void *ud; };
   (void)entry;
   hp.fade_pushes++;
}
bool gfx_animation_kill_by_tag(uintptr_t *tag) { (void)tag; return true; }

/* ---- task queue ---- */
bool task_push_image_load(const char *path, bool rgba, unsigned t,
      unsigned c, void *cb, void *ud)
{
   (void)path; (void)rgba; (void)t; (void)c; (void)cb;
   hp.still_loads++;
   free(ud);              /* the tag would be freed by the callback */
   return true;
}
bool task_image_detach_video_stream(void *t, void **s, int *ty, void **x,
      void **b, size_t *l)
{ (void)t; (void)s; (void)ty; (void)x; (void)b; (void)l; return false; }
/* No task, no verdict: the real accessor answers -1 (unknown) for
 * anything that is not a completed PNG image task, and unknown is
 * exactly what sends the open down its historical file probe. */
int task_image_png_probe(void *t) { (void)t; return -1; }

void *task_queue_find(void *id) { (void)id; return NULL; }
void task_set_flags(void *t, uint32_t f, bool s) { (void)t; (void)f; (void)s; }

/* ---- config ---- */
static char g_settings[1 << 20];
void *config_get_ptr(void)
{
   /* Exact offset of settings->bools.menu_thumbnail_preview_audio,
    * taken from offsetof against the real configuration.h - nothing
    * else in the blob is disturbed. */
   g_settings[7614] = hp.force_preview_audio ? 1 : 0;
   return g_settings;
}

/* ---- menu / runloop / playlist ---- */
void *menu_state_get_ptr(void) { static char b[65536]; return b; }
void *runloop_state_get_ptr(void) { static char b[1 << 18]; return b; }
void runloop_path_set_redirect(void *a, char *b, char *c)
{ (void)a; (void)b; (void)c; }
void dir_set(unsigned t, const char *p) { (void)t; (void)p; }
void *playlist_get_cached(void) { return NULL; }
void playlist_get_index(void *p, size_t i, const void **e)
{ (void)p; (void)i; if (e) *e = NULL; }
size_t playlist_get_size(void *p) { (void)p; return 0; }
char *playlist_get_conf_path(void *p) { (void)p; return NULL; }
void playlist_get_db_name(void *p, size_t i, const char **n)
{ (void)p; (void)i; if (n) *n = NULL; }
int playlist_get_thumbnail_mode(void *p, unsigned id) { (void)p; (void)id; return 0; }
const char *msg_hash_to_str(unsigned id) { (void)id; return ""; }

/* ---- threads: single-threaded harness, real locks not needed ---- */
void *slock_new(void) { return malloc(1); }
void slock_free(void *l) { free(l); }
void slock_lock(void *l) { (void)l; }
void slock_unlock(void *l) { (void)l; }
void *scond_new(void) { return malloc(1); }
void scond_free(void *c) { free(c); }
void scond_wait(void *c, void *l) { (void)c; (void)l; }
void scond_signal(void *c) { (void)c; }
void scond_broadcast(void *c) { (void)c; }
void *sthread_create(void *f, void *ud) { (void)f; (void)ud; return NULL; }
void sthread_join(void *t) { (void)t; }

bool path_is_directory(const char *p)
{ struct stat st; return stat(p, &st) == 0 && S_ISDIR(st.st_mode); }
bool path_mkdir(const char *d) { (void)d; return false; }

/* file_path.c gates these behind RARCH_INTERNAL; provide the real
 * semantics rather than stubs, since the thumbnail path resolution
 * depends on them. */
#include <streams/file_stream.h>
bool path_is_valid(const char *path)
{
   struct stat st;
   return path && *path && stat(path, &st) == 0;
}
int64_t path_get_size(const char *path)
{
   struct stat st;
   if (!path || !*path || stat(path, &st) != 0)
      return -1;
   return (int64_t)st.st_size;
}
int path_is_media_type(const char *path)
{
   const char *e = strrchr(path ? path : "", '.');
   if (!e) return 0;
   if (!strcasecmp(e, ".png") || !strcasecmp(e, ".jpg")
    || !strcasecmp(e, ".jpeg") || !strcasecmp(e, ".bmp")
    || !strcasecmp(e, ".tga") || !strcasecmp(e, ".webp"))
      return 4 /* RARCH_CONTENT_IMAGE */;
   return 0;
}

/* ---- audio mixer edge (GFX_THUMB_PREVIEW_AUDIO builds) ---- */
#include <audio/audio_mixer.h>
#include "../../../audio/audio_driver.h"
/* Model the mixer closely enough to catch the two failures that
 * matter: reading past the advertised bound, and the bound never
 * moving.  The "decoder" walks the container at a fixed byte rate on
 * every feeder tick, exactly as an audio thread would, and touches
 * every byte it is told is resident - so an over-claimed avail faults
 * under ASan instead of passing silently. */
static const uint8_t *hp_abuf;
static size_t hp_alen, hp_aavail, hp_atell;
static void *hp_aowner;
static void (*hp_aownerfree)(void*);
static int hp_alive;

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *params)
{
   hp.audio_streams++;
   hp.last_audio_bytes = params->bufsize;
   hp_abuf       = (const uint8_t*)params->buf;
   hp_alen       = params->bufsize;
   hp_aavail     = params->avail ? params->avail : params->bufsize;
   hp_atell      = 0;
   hp_aowner     = params->buf_owner;
   hp_aownerfree = params->buf_owner_free;
   hp_alive      = 1;
   if (params->out_slot)
      *params->out_slot = 7;
   return true;   /* borrows the window; released via remove_stream */
}

/* bytes the decoder consumes per feeder tick: 255 kbps at 60 fps */
static size_t hp_rate(void)
{
   const char *e = getenv("AUDIO_BYTES_PER_TICK");
   return e ? (size_t)atol(e) : 532;   /* 255 kbps at 60 fps */
}
#define HP_BYTES_PER_TICK hp_rate()

int64_t audio_driver_mixer_stream_byte_tell(unsigned i)
{
   volatile uint8_t sink = 0;
   size_t want;
   (void)i;
   if (!hp_alive)
      return -1;
   want = hp_atell + HP_BYTES_PER_TICK;
   if (want > hp_aavail)
   {
      want = hp_aavail;          /* stall at the bound, never past it */
      /* Only a bound BELOW the file end is the feeder falling behind.
       * At avail == len the whole container is resident and hitting
       * the wall is just end of stream before the loop - counting
       * that as a stall made a 10 MB fixture look like a starved
       * feeder. */
      if (hp_aavail < hp_alen)
         hp.audio_stalls++;
   }
   /* touch what we were promised is resident */
   while (hp_atell < want)
      sink ^= hp_abuf[hp_atell++];
   (void)sink;
   if (hp_atell >= hp_alen)
      hp_atell = 0;              /* loop */
   return (int64_t)hp_atell;
}

void audio_driver_mixer_stream_set_avail(unsigned i, size_t avail)
{
   (void)i;
   if (avail > hp_alen)
      avail = hp_alen;
   if (avail > hp_aavail)
      hp_aavail = avail;
   hp.audio_avail_raises++;
}
void audio_driver_mixer_stop_stream(unsigned i) { (void)i; hp.audio_stops++; }
void audio_driver_mixer_remove_stream(unsigned i)
{
   (void)i;
   hp_alive = 0;
   if (hp_aowner && hp_aownerfree)
      hp_aownerfree(hp_aowner);
   hp_aowner = NULL; hp_abuf = NULL;
}
const char *audio_driver_mixer_get_stream_name(unsigned i) { (void)i; return "__gfx_thumb_preview"; }
unsigned audio_driver_mixer_get_stream_state(unsigned i) { (void)i; return 0; }

bool audio_driver_mixer_stream_stop(unsigned i) { (void)i; return true; }


/* Resampler config edges.  The mixer instantiates a resampler when a
 * stream's rate differs from the mixer rate; none of these values
 * affect what this sample measures. */
void config_userdata_free(void *u) { (void)u; }
int  config_userdata_get_float(void *u, const char *k, float *v, float d)
{ (void)u; (void)k; if (v) *v = d; return 0; }
int  config_userdata_get_int(void *u, const char *k, int *v, int d)
{ (void)u; (void)k; if (v) *v = d; return 0; }
int  config_userdata_get_float_array(void *u, const char *k, float **v,
      unsigned *n) { (void)u; (void)k; (void)v; (void)n; return 0; }
int  config_userdata_get_int_array(void *u, const char *k, int **v,
      unsigned *n) { (void)u; (void)k; (void)v; (void)n; return 0; }
int  config_userdata_get_string(void *u, const char *k, char **v,
      const char *d) { (void)u; (void)k; (void)v; (void)d; return 0; }
