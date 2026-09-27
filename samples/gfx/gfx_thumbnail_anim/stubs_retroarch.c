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

#include "../../../gfx/video_defines.h"

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

/* In-place update: the same oracle, on the same handle. gt_can_update
 * decides whether the surface takes this path or loads a replacement
 * per frame, so both are exercised. */
int gt_can_update = 1;
int gt_updates;
bool video_driver_texture_update(uintptr_t id, void *data)
{
   uintptr_t same = id;
   if (!id)
      return false;
   gt_updates++;
   video_driver_texture_load(data, 0, &same);
   return true;
}
bool video_driver_texture_can_update(void) { return gt_can_update != 0; }

/* --- the asynchronous path ---
 * gt_async_mode makes the wrapper look active. Loads are parked here
 * and completed by gt_async_flush(), which runs the CRC oracle, the
 * release and the done() callback in post order - the same contract
 * the real wrapper gives the main thread from video_thread_frame(). */
int gt_async_mode;
int gt_async_posted;
int gt_async_pending;
typedef struct gt_async_node
{
   struct gt_async_node *next;
   void *img;
   void (*done)(void *user, uintptr_t handle);
   void *user;
   void (*release)(void *img);
} gt_async_node_t;
static gt_async_node_t *gt_async_head, *gt_async_tail;

bool video_driver_thread_wrapper_active(void) { return gt_async_mode != 0; }
bool video_thread_texture_can_update(void)
{ return gt_async_mode != 0 && gt_can_update != 0; }

/* Caller-owned nodes (the surface's) are parked the same way and run
 * on flush by kind; they are never freed here. Layout of the node as
 * the wrapper declares it: next, img, user, done, release, handle,
 * filter, kind, caller_owned. */
typedef struct gt_post_node
{
   struct gt_post_node *next;
   void *img;
   void *user;
   void (*done)(void *user, uintptr_t handle);
   void (*release)(void *img);
   uintptr_t handle;
   int filter;
   uint8_t kind;
   uint8_t caller_owned;
} gt_post_node_t;
static gt_post_node_t *gt_post_head, *gt_post_tail;

bool video_thread_async_post(void *node)
{
   gt_post_node_t *n = (gt_post_node_t*)node;
   if (!gt_async_mode)
      return false;
   n->next         = NULL;
   n->caller_owned = 1;
   if (gt_post_tail) gt_post_tail->next = n; else gt_post_head = n;
   gt_post_tail = n;
   gt_async_posted++;
   gt_async_pending++;
   return true;
}

bool video_driver_texture_load_async(void *data, unsigned filter,
      void (*done)(void *user, uintptr_t handle), void *user,
      void (*release)(void *img))
{
   if (!gt_async_mode)
   {
      uintptr_t id = 0;
      video_driver_texture_load(data, filter, &id);
      if (release) release(data);
      if (done)    done(user, id);
      return true;
   }
   {
      gt_async_node_t *n = (gt_async_node_t*)calloc(1, sizeof(*n));
      if (!n) return false;
      n->img = data; n->done = done; n->user = user; n->release = release;
      if (gt_async_tail) gt_async_tail->next = n; else gt_async_head = n;
      gt_async_tail = n;
      gt_async_posted++;
      gt_async_pending++;
   }
   return true;
}

void gt_async_flush(void)
{
   gt_async_node_t *n = gt_async_head;
   gt_post_node_t  *p = gt_post_head;
   gt_async_head = gt_async_tail = NULL;
   gt_post_head  = gt_post_tail  = NULL;
   while (n)
   {
      gt_async_node_t *next = n->next;
      uintptr_t id = 0;
      video_driver_texture_load(n->img, 0, &id);
      if (n->release) n->release(n->img);
      gt_async_pending--;
      if (n->done)    n->done(n->user, id);
      free(n);
      n = next;
   }
   while (p)
   {
      gt_post_node_t *next = p->next;
      uintptr_t id = 0;
      if (p->kind == 1)
         id = video_driver_texture_update(p->handle, p->img) ? p->handle : 0;
      else
         video_driver_texture_load(p->img, 0, &id);
      gt_async_pending--;
      if (p->done)    p->done(p->user, id);
      p = next;
   }
}
unsigned video_driver_get_disp_flags(void) { return 0; }
void video_driver_get_video_output_size(unsigned *dims,
      char *n, size_t l) { *dims = VIDEO_SCALE_PACK(1920, 1080); (void)n; (void)l; }
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

/* gfx_thumbnail_draw() reaches the display driver through this rather
 * than through one of the helpers, so it needs its own stub even
 * though nothing here draws.
 *
 * The two pointer parameters are void* rather than their real types.
 * This file declares its own view of the frontend and including
 * gfx_display.h to name them drags in a video_driver.h that conflicts
 * with those declarations; C linkage does not carry parameter types,
 * so the symbol matches what gfx_thumbnail.c calls either way. */
void gfx_display_draw(void *dispctx, void *draw, void *data,
      unsigned video_dims)
{
   unsigned video_width  = VIDEO_SCALE_W(video_dims);
   unsigned video_height = VIDEO_SCALE_H(video_dims);
 (void)dispctx; (void)draw; (void)data;
  (void)video_width; (void)video_height; }

/* Blending goes through gfx_display now, on the same terms as the
 * draw above: void* for the same reason, and nothing to do here. */
void gfx_display_blend_begin(void *dispctx, void *data)
{ (void)dispctx; (void)data; }
void gfx_display_blend_end(void *dispctx, void *data)
{ (void)dispctx; (void)data; }

/* The surface layer asks the driver what it wants before a decode
 * (gfx_surface_query_requirements): here there is no driver, so the
 * answers are the software defaults - no 10-bit source, no compressed
 * sampling. */
bool video_driver_test_all_flags(int flags)
{ (void)flags; return false; }
bool video_driver_supports_texture_format(int fmt)
{ (void)fmt; return false; }
