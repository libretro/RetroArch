/* Link-level stubs for gfx_thumbnail_anim_test: the frontend surface
 * gfx_thumbnail.c calls into, all inert except the video driver's
 * texture load, which is the oracle - it CRCs each upload so the test
 * can count DISTINCT frames rather than trusting internal state.
 *
 * No RetroArch headers on purpose: several stubbed functions have
 * heavyweight real signatures, and C linkage does not check types
 * across translation units.  What matters is that every stub is
 * behaviourally inert. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

extern int      gt_uploads;
extern unsigned gt_last_crc;

/* --- the oracle --- */
bool video_driver_texture_load(void *data, unsigned filter, uintptr_t *id)
{
   struct { void *px; unsigned w, h; } *img = data;
   (void)filter;
   if (img && img->px)
   {
      unsigned c = 0, n = img->w * img->h, i;
      const uint32_t *q = (const uint32_t*)img->px;
      for (i = 0; i < n; i += 97)
         c = c * 33 + q[i];
      if (c != gt_last_crc)
      {
         gt_uploads++;
         gt_last_crc = c;
      }
   }
   *id = 2;
   return true;
}
bool video_driver_texture_unload(uintptr_t *id) { *id = 0; return true; }
unsigned video_driver_get_disp_flags(void) { return 0; }
void video_driver_get_video_output_size(unsigned *w, unsigned *h,
      char *n, size_t l) { *w = 1920; *h = 1080; (void)n; (void)l; }
void video_driver_get_viewport_info(void *vp) { (void)vp; }

/* --- inert frontend surface --- */
typedef struct { char pad[65536]; } gt_blob_t;
static gt_blob_t gt_blob;
void *config_get_ptr(void)        { return &gt_blob; }
void *disp_get_ptr(void)          { return &gt_blob; }
void *menu_state_get_ptr(void)    { return &gt_blob; }
void *runloop_state_get_ptr(void) { return &gt_blob; }
void *video_state_get_ptr(void)   { return &gt_blob; }
void dir_set(int t, const char *p) { (void)t; (void)p; }
void runloop_path_set_redirect(void *a, const char *b, const char *c)
{ (void)a; (void)b; (void)c; }
const char *msg_hash_to_str(unsigned id) { (void)id; return ""; }
bool gfx_animation_push(void *e) { (void)e; return true; }
bool gfx_animation_kill_by_tag(uintptr_t *t) { (void)t; return true; }
float gfx_display_rotate_z(void *a, void *b) { (void)a; (void)b; return 0.0f; }
unsigned gfx_display_texture_filter(void) { return 0; }
bool path_is_media_type(const char *path) { (void)path; return false; }
void *playlist_get_cached(void) { return NULL; }
const char *playlist_get_conf_path(void *p) { (void)p; return NULL; }
const char *playlist_get_db_name(void *p, size_t i)
{ (void)p; (void)i; return NULL; }
void playlist_get_index(void *p, size_t i, void **e)
{ (void)p; (void)i; *e = NULL; }
size_t playlist_get_size(void *p) { (void)p; return 0; }
unsigned playlist_get_thumbnail_mode(void *p, unsigned t)
{ (void)p; (void)t; return 0; }
bool task_image_detach_video_stream(void *t, void **s, int *ty,
      void **x, void **b, size_t *l)
{ (void)t; (void)s; (void)ty; (void)x; (void)b; (void)l; return false; }
/* No task, no verdict: the real accessor answers -1 (unknown) for
 * anything that is not a completed PNG image task, and unknown is
 * exactly what sends the open down its historical file probe. */
int task_image_png_probe(void *t) { (void)t; return -1; }

bool task_push_image_load(const char *a, bool b, unsigned c, unsigned d,
      void *e, void *f)
{ (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; return false; }
