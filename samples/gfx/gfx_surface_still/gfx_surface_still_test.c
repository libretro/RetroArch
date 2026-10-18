/* gfx_surface stills: a still keeps its texture across uploads,
 * decodes and a free in flight, under the threaded wrapper and
 * without, and a set of files is decoded across threads and goes up
 * in order. Stub driver, simulated wrapper, task queue and image
 * decoder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queues/task_queue.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

#include "gfx/video_driver.h"
#include "gfx/video_thread_wrapper.h"
#include "gfx/gfx_surface.h"
#include <formats/image_yuv_blit.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; \
   printf("[FAIL] " __VA_ARGS__); printf("\n"); } } while (0)

/* --- stub driver ------------------------------------------------- */
static uintptr_t st_next = 1;
static int       st_live, st_loads, st_unloads, st_async;
/* Updates the stub driver drops, and updates it was handed without
 * an image to read */
static int       st_drop, st_updates, st_null_updates, st_refuse, st_load_fail, st_post_fail;
static enum texture_filter_type st_last_filter;
static enum video_texture_update st_import_result = VIDEO_TEXTURE_UPDATE_REFUSED;
static unsigned st_abandoned, st_released;
static bool st_lend, st_fp16;
static uint32_t st_lent_pixels[2][64];
static size_t st_lend_pitch[2];
static unsigned st_lend_busy;
static int st_load_lent;
static uintptr_t st_last_unloaded;
static video_thread_async_load_t *st_head, *st_tail;
/* The first and last word of the last frame the driver read */
static uint32_t st_px_first, st_px_last;
/* The planes and colour space of the last planar frame read */
static struct texture_planar st_planar;
static int st_planar_reads;

static void st_saw(void *data)
{
   const struct texture_image *ti = (const struct texture_image*)data;
   if (ti && ti->planar)
   {
      st_planar = *ti->planar;
      st_planar_reads++;
      return;
   }
   if (ti && ti->pixels && ti->width && ti->height)
   {
      st_px_first = ti->pixels[0];
      st_px_last  = ti->pixels[(size_t)ti->width * ti->height * (ti->fp16 ? 2 : 1) - 1];
   }
}

bool video_driver_texture_load(void *data,
      enum texture_filter_type filter, uintptr_t *id)
{
   st_last_filter = filter;
   if (st_load_fail)
   {
      st_load_fail--;
      *id = 0;
      return false;
   }
   if (st_lend_pitch[0] && data)
   {
      const struct texture_image *img = (const struct texture_image*)data;
      if (img->pixels == st_lent_pixels[0] || img->pixels == st_lent_pixels[1])
         st_load_lent++;
   }
   st_saw(data);
   *id = st_next++;
   st_loads++;
   st_live++;
   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   if (*id)
   {
      st_last_unloaded = *id;
      st_unloads++;
      st_live--;
   }
   *id = 0;
   return true;
}

enum video_texture_update video_driver_texture_update(uintptr_t id,
      void *data)
{
   (void)id;
   st_updates++;
   st_saw(data);
   if (!data)
      st_null_updates++;
   if (st_refuse)
   {
      st_refuse--;
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   }
   if (st_drop > 0)
   {
      st_drop--;
      return VIDEO_TEXTURE_UPDATE_DROPPED;
   }
   return VIDEO_TEXTURE_UPDATE_DONE;
}

bool video_driver_texture_can_update(void) { return true; }
bool video_driver_thread_wrapper_active(void) { return st_async != 0; }
bool task_is_on_main_thread(void) { return true; }
unsigned video_driver_get_disp_flags(void) { return 0; }
/* Whether the stub driver converts YCbCr itself */
static int st_yuv420;
bool video_driver_supports_texture_format(enum texture_gpu_format fmt)
{
   return (fmt == TEXTURE_GPU_FORMAT_YUV420 && st_yuv420)
      || (st_fp16 && (fmt == TEXTURE_GPU_FORMAT_RGBA16F || fmt == TEXTURE_GPU_FORMAT_SCRGB));
}

/* As the frontend fits an image, from this stub's own answers */
bool video_driver_texture_fit(struct texture_image *ti)
{
   if (ti->planar)
      return video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_YUV420);
   if (ti->pix10 && !video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGB10A2))
      image_texture_narrow_10bit(ti);
   return !ti->fp16
      || video_driver_supports_texture_format(TEXTURE_GPU_FORMAT_RGBA16F);
}
void *video_driver_texture_lend(uintptr_t id, unsigned slot, size_t pitch, size_t *out_pitch)
{
   size_t actual = slot < 2 && st_lend_pitch[slot] ? st_lend_pitch[slot] : pitch;
   if (st_lend && id && slot < 2 && pitch == 4 * (st_fp16 ? 8 : sizeof(uint32_t))
         && actual >= pitch && (out_pitch || actual == pitch))
   {
      if (out_pitch)
         *out_pitch = actual;
      return st_lent_pixels[slot];
   }
   return NULL;
}
enum video_texture_update video_driver_texture_import(uintptr_t *id,
      const video_frame_import_t *frame, enum texture_filter_type filter)
{
   (void)filter;
   CHECK(frame->output_pixfmt == (frame->color.transfer == 16 ? IMAGE_PIXFMT_FP16 : 0),
         "import lost destination format");
   CHECK(frame->color.primaries == 9 && frame->color.matrix == 9
         && (frame->color.transfer == 16 || frame->color.transfer == 18)
         && frame->color.range == 1 && frame->color.max_cll == 1000
         && frame->color.max_fall == 400
         && frame->color.mastering.present == VIDEO_MASTERING_DISPLAY_COMPLETE
         && frame->color.mastering.chromaticity[0] == 25000
         && frame->color.mastering.luminance_max == 10240000,
         "import lost source HDR metadata");
   if (st_import_result == VIDEO_TEXTURE_UPDATE_DONE && !*id)
   {
      *id = st_next++;
      st_live++;
   }
   return st_import_result;
}

bool video_driver_texture_lend_ready(uintptr_t id, unsigned slot)
{
   (void)id;
   return !(st_lend_busy & (1u << slot));
}

bool video_thread_lend_ready(int idx)
{
   return !(st_lend_busy & (1u << idx));
}

bool video_thread_async_post(video_thread_async_load_t *n)
{
   if (st_post_fail)
   {
      st_post_fail--;
      return false;
   }
   if (!st_async)
      return false;
   n->link.next = NULL;
   if (st_tail)
      st_tail->link.next = &n->link;
   else
      st_head = n;
   st_tail = n;
   return true;
}

/* The video thread's frame, then the main thread's completions. As
 * the wrapper does, the node's image is let go once it has run:
 * a completion that sends the node again must give it back. */
static void st_flush(void)
{
   video_thread_async_load_t *n = st_head;
   st_head = st_tail = NULL;
   while (n)
   {
      video_thread_async_load_t *next = n->link.next
         ? (video_thread_async_load_t*)n->link.next : NULL;
      uintptr_t id = 0;
      if (n->kind == VIDEO_THREAD_ASYNC_IMPORT)
      {
         enum video_texture_update result;
         id = n->handle;
         result = video_driver_texture_import(&id,
               (const video_frame_import_t*)n->img, n->filter);
         n->dropped = result != VIDEO_TEXTURE_UPDATE_DONE;
         n->made = result == VIDEO_TEXTURE_UPDATE_DONE && id != n->handle;
      }
      else if (n->kind == VIDEO_THREAD_ASYNC_UPDATE)
      {
         enum video_texture_update result;
         id = n->handle;
         result = video_driver_texture_update(id, n->img);
         n->dropped = result == VIDEO_TEXTURE_UPDATE_DROPPED;
         if (result == VIDEO_TEXTURE_UPDATE_REFUSED)
            id = 0;
         if (result == VIDEO_TEXTURE_UPDATE_DONE && n->lend)
         {
            const struct texture_image *img = (const struct texture_image*)n->img;
            unsigned slot;
            for (slot = 0; slot < 2; slot++)
               if (n->lend & (1u << slot))
               {
                  size_t pitch = img->width * (img->fp16 ? 8 : 4);
                  n->lent_mem[slot] = video_driver_texture_lend(id, slot, pitch,
                        n->lend_padded ? &pitch : NULL);
                  n->lent_pitch[slot] = pitch;
                  n->lent_idx[slot] = n->lent_mem[slot] ? (int8_t)slot : -1;
               }
         }
      }
      else
         video_driver_texture_load(n->img, n->filter, &id);
      n->img = NULL;
      if (n->done)
         n->done(n->user, id);
      n = next;
   }
}

/* The image decoder: a file "<w>.png" is a w by w image; any other
 * name is no file. Each decode takes a moment and notes the thread
 * it ran on. */
#define ST_DECODE_THREADS 64
static uintptr_t st_decode_thread[ST_DECODE_THREADS];
static unsigned  st_decode_threads;
static slock_t  *st_decode_lock;

bool image_texture_load_request(struct texture_image *img,
      const char *path, const image_texture_request_t *req,
      bool (*should_abort)(void *ud), void *ud)
{
   unsigned w = (unsigned)atoi(path);
   (void)req; (void)should_abort; (void)ud;
   if (!w)
      return false;
   retro_sleep(2);
   img->width  = img->height = w;
   img->pixels = (uint32_t*)calloc(w * w, 4);
   slock_lock(st_decode_lock);
   {
      uintptr_t id = sthread_get_current_thread_id();
      unsigned i;
      for (i = 0; i < st_decode_threads; i++)
         if (st_decode_thread[i] == id)
            break;
      if (i == st_decode_threads && i < ST_DECODE_THREADS)
         st_decode_thread[st_decode_threads++] = id;
   }
   slock_unlock(st_decode_lock);
   return true;
}

/* The set's own threads hand back their read pools as they finish;
 * the caller's thread keeps its own. Noted by thread. */
static uintptr_t st_flush_thread[ST_DECODE_THREADS];
static unsigned  st_flush_threads;
void data_transfer_pool_flush(void)
{
   slock_lock(st_decode_lock);
   if (st_flush_threads < ST_DECODE_THREADS)
      st_flush_thread[st_flush_threads++] = sthread_get_current_thread_id();
   slock_unlock(st_decode_lock);
}

bool image_texture_load_buffer_request(struct texture_image *img,
      enum image_type_enum type, const void *s, size_t len,
      const image_texture_request_t *req,
      bool (*should_abort)(void *ud), void *ud)
{
   (void)img; (void)type; (void)s; (void)len; (void)req;
   (void)should_abort; (void)ud;
   return false;
}

void image_texture_narrow_10bit(struct texture_image *img) { (void)img; }

void image_texture_free(struct texture_image *img)
{
   if (!img)
      return;
   free(img->pixels);
   img->pixels = NULL;
   img->width  = img->height = 0;
}

/* The task queue: one decode held until the test answers it */
static retro_task_callback_t st_decode_cb;
static void                 *st_decode_user;
static unsigned              st_decodes;

bool task_push_image_load(const char *fullpath, bool supports_rgba,
      unsigned upscale_threshold, unsigned downscale_cap,
      retro_task_callback_t cb, void *user_data)
{
   (void)fullpath; (void)supports_rgba; (void)upscale_threshold;
   (void)downscale_cap;
   st_decode_cb   = cb;
   st_decode_user = user_data;
   st_decodes++;
   return true;
}

/* Answers the held decode with an image of @w x @w, or nothing */
static void st_decode_done(unsigned w)
{
   retro_task_callback_t cb = st_decode_cb;
   void *user               = st_decode_user;
   struct texture_image *img = NULL;
   st_decode_cb = NULL;
   if (w)
   {
      img = (struct texture_image*)calloc(1, sizeof(*img));
      img->width  = img->height = w;
      img->pixels = (uint32_t*)calloc(w * w, 4);
   }
   if (cb)
      cb(NULL, img, user, NULL);
}

static struct texture_image *st_image(unsigned w)
{
   struct texture_image *img = (struct texture_image*)calloc(1, sizeof(*img));
   img->width  = img->height = w;
   img->pixels = (uint32_t*)calloc(w * w, 4);
   return img;
}

/* --- the test ---------------------------------------------------- */


/* The path of slot @i in the set lane: "<8 + i>.png", but slot 3's
 * file does not exist and slot 5 has no still */
static void st_set_path(unsigned i, void *ud, char *buf, size_t len)
{
   (void)ud;
   if (i == 3)
      snprintf(buf, len, "missing.png");
   else
      snprintf(buf, len, "%u.png", 8 + i);
}

static void st_abandon_import(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &st_abandoned && done == 42 && value == 7,
         "retired import lost its release destination");
   st_abandoned++;
}
static void st_release_import(void *user, gfx_surface_t *surface, unsigned slot)
{
   (void)user; (void)surface; (void)slot;
   st_released++;
}
static void st_retired_imports(void)
{
   enum video_texture_update results[3] = {
      VIDEO_TEXTURE_UPDATE_REFUSED, VIDEO_TEXTURE_UPDATE_DROPPED,
      VIDEO_TEXTURE_UPDATE_DONE };
   unsigned i;
   for (i = 0; i < 9; i++)
   {
      video_frame_import_t frame = {0};
      gfx_surface_t *surface = gfx_surface_new_import(VIDEO_SCALE_PACK(4, 4),
            TEXTURE_FILTER_NEAREST, st_release_import, NULL);
      CHECK(surface != NULL, "import surface allocation failed");
      if (!surface) continue;
      frame.dims = VIDEO_SCALE_PACK(4, 4);
      frame.pixfmt = IMAGE_PIXFMT_NV12;
      frame.image = 1;
      frame.color.primaries = frame.color.matrix = 9;
      frame.color.transfer = i % 2 ? 16 : 18; frame.color.range = 1;
      frame.output_pixfmt = i % 2 ? IMAGE_PIXFMT_FP16 : 0;
      frame.color.max_cll = 1000; frame.color.max_fall = 400;
      frame.color.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
      frame.color.mastering.chromaticity[0] = 25000;
      frame.color.mastering.luminance_max = 10240000;
      frame.done = 42;
      frame.done_value = 7;
      frame.owner = &st_abandoned;
      frame.abandon = st_abandon_import;
      st_abandoned = st_released = 0;
      if (i % 3 == 1)
      {
         st_async = 0;
         st_import_result = VIDEO_TEXTURE_UPDATE_DONE;
         CHECK(gfx_surface_submit_import(surface, &frame) == GFX_SURFACE_SUBMIT_DONE,
               "initial import failed");
      }
      st_async = 1;
      st_import_result = results[i % 3];
      CHECK(gfx_surface_submit_import(surface, &frame) == GFX_SURFACE_SUBMIT_QUEUED,
            "retired import was not queued");
      if (i >= 3)
         CHECK(gfx_surface_submit_path(surface, "4.png", false),
               "retired surface decode was not queued");
      gfx_surface_free(surface);
      CHECK(st_abandoned == 0, "import released before wrapper completion");
      memset(&frame, 0, sizeof(frame));
      if (i >= 6)
         st_decode_done(4);
      st_flush();
      CHECK(st_abandoned == (i % 3 == 2 ? 0 : 1),
            "retired import result %u abandoned %u times", i, st_abandoned);
      if (i >= 3 && i < 6)
         st_decode_done(4);
      CHECK(st_abandoned == (i % 3 == 2 ? 0 : 1),
            "decode completion changed import release count");
      CHECK(st_released == 0, "retired surface called its owner");
      CHECK(st_live == 0, "%d live textures after retired import", st_live);
   }
   st_import_result = VIDEO_TEXTURE_UPDATE_REFUSED;
   st_async = 0;
}
static void st_retired_lent_textures(void)
{
   unsigned order;
   for (order = 0; order < 3; order++)
   {
      uint32_t pixels[16] = {0};
      uintptr_t old;
      gfx_surface_t *surface = gfx_surface_new(VIDEO_SCALE_PACK(4, 4), 1,
            IMAGE_PIXFMT_8888, TEXTURE_FILTER_NEAREST, NULL, NULL);
      CHECK(surface != NULL, "lent surface allocation failed");
      if (!surface) continue;
      st_async = 0;
      st_lend = true;
      memcpy(surface->slots[0], pixels, sizeof(pixels));
      CHECK(gfx_surface_submit(surface, 0, false) == GFX_SURFACE_SUBMIT_DONE
            && surface->lent, "initial texture memory was not lent");
      old = surface->handle;
      CHECK(gfx_surface_slot_begin(surface, 0) == st_lent_pixels[0],
            "producer did not get the lent slot");
      st_async = 1;
      CHECK(gfx_surface_submit(surface, 0, true) == GFX_SURFACE_SUBMIT_QUEUED,
            "replacement texture was not queued");
      if (order)
         CHECK(gfx_surface_submit_path(surface, "4.png", false),
               "lent surface decode was not queued");
      /* The producer has stopped; cancellation owns its remaining slot. */
      gfx_surface_free(surface);
      if (order == 2) st_decode_done(4);
      st_flush();
      if (order == 1) st_decode_done(4);
      CHECK(st_live == 0, "cancelled lent replacement order %u leaked %d textures",
            order, st_live);
      if (st_live) video_driver_texture_unload(&old);
      st_lend = false;
   }
   st_async = 0;
}
static void st_pitched_write(gfx_surface_t *s, unsigned slot, uint32_t value)
{
   unsigned x, y;
   unsigned words = 4 * IMAGE_PIXFMT_BPP(s->pixfmt) / sizeof(uint32_t);
   for (y = 0; y < 4; y++)
      for (x = 0; x < words; x++)
         ((uint32_t*)((uint8_t*)s->slots[slot] + y * s->pitch[slot]))[x]
            = value + y * words + x;
}

static void st_pitched_lending(void)
{
   unsigned threaded, active, format, i;
   for (format = 0; format < 2; format++)
   for (threaded = 0; threaded < 2; threaded++)
   for (active = 0; active < 2; active++)
   {
      gfx_surface_t *s;
      uintptr_t old;
      int unloaded;
      st_async = (int)threaded;
      st_lend = true;
      st_fp16 = format != 0;
      st_lend_pitch[0] = format ? 48 : 24;
      st_lend_pitch[1] = format ? 64 : 32;
      st_load_lent = 0;
      memset(st_lent_pixels, 0xa5, sizeof(st_lent_pixels));
      s = gfx_surface_new(VIDEO_SCALE_PACK(4, 4), 2,
            format ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888,
            TEXTURE_FILTER_LINEAR, NULL, NULL);
      CHECK(s != NULL, "pitched surface allocation");
      if (!s) continue;
      gfx_surface_submit(s, 0, false);
      st_flush();
      gfx_surface_submit(s, 0, false);
      st_flush();
      CHECK(!s->lent, "packed producer received padded memory");
      s->padded = 1;
      for (i = 0; i < 2; i++)
      {
         gfx_surface_submit(s, i, false);
         st_flush();
      }
      CHECK(s->lent == 3 && s->pitch[0] == st_lend_pitch[0] && s->pitch[1] == st_lend_pitch[1],
            "padded pitches not carried through lending");
      if (active)
         gfx_surface_slot_begin(s, 1);
      st_pitched_write(s, 0, 0x100);
      st_pitched_write(s, 1, 0x200);
      CHECK(st_lent_pixels[0][format ? 8 : 4] == 0xa5a5a5a5u
            && st_lent_pixels[1][format ? 8 : 4] == 0xa5a5a5a5u, "row padding overwritten");
      old = s->handle;
      unloaded = st_unloads;
      s->filter = TEXTURE_FILTER_NEAREST;
      gfx_surface_submit(s, 0, false);
      st_flush();
      CHECK(!st_load_lent && st_px_first == 0x100 && st_px_last == 0x100 + (format ? 32 : 16) - 1,
            "replacement load did not repack padded rows");
      if (active)
      {
         CHECK(s->retired_handle == old && st_unloads == unloaded,
               "writing producer lost its texture");
         gfx_surface_slot_end(s, 1);
         CHECK(!s->retired_handle, "settled producer retained texture");
      }
      for (i = 0; i < (format ? 32u : 16u); i++)
         CHECK(s->own_slots[1][i] == 0x200 + i,
               "replacement lost %s producer pixel %u", active ? "writing" : "ready", i);
      gfx_surface_free(s);
      st_flush();
      st_lend_pitch[0] = st_lend_pitch[1] = 0;
      st_lend = false;
   }
   st_async = 0;
   st_fp16 = false;
   {
      gfx_surface_t *s = gfx_surface_new(VIDEO_SCALE_PACK(4, 4), 1,
            IMAGE_PIXFMT_8888, TEXTURE_FILTER_LINEAR, NULL, NULL);
      uint32_t pixels[16];
      if (!s) return;
      s->padded = 1;
      st_lend = true;
      st_lend_pitch[0] = 24;
      st_lend_pitch[1] = 32;
      gfx_surface_submit(s, 0, false);
      CHECK(s->pitch[0] == 24 && s->lent_spare_pitch == 32, "one-slot pitches");
      st_lend_busy = 1;
      CHECK(gfx_surface_slot_writable(s, 0) && s->pitch[0] == 32,
            "one-slot pitch did not follow spare");
      for (i = 0; i < 16; i++) pixels[i] = i + 0x300;
      gfx_surface_submit_pixels(s, pixels, false);
      CHECK(st_lent_pixels[1][8] == 0x304 && st_lent_pixels[1][24] == 0x30c,
            "external canvas was not copied row by row");
      st_lend_busy = 0;
      gfx_surface_free(s);
      st_lend = false;
      st_lend_pitch[0] = st_lend_pitch[1] = 0;
   }
}

static void st_still_recovery(void)
{
   unsigned fail_load;
   for (fail_load = 0; fail_load < 2; fail_load++)
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      uintptr_t old;
      int loads;
      CHECK(gfx_surface_submit_image(s, st_image(8)), "recovery: initial image");
      st_flush();
      old = s->handle;
      loads = st_loads;
      st_refuse = 1;
      CHECK(gfx_surface_submit_image(s, st_image(8)), "recovery: new image");
      st_flush();
      CHECK(s->inflight && s->payload && s->handle == old,
            "refused update lost its image before replacement");
      st_load_fail = (int)fail_load;
      st_flush();
      CHECK(!s->inflight && !s->payload, "recovery did not settle its payload");
      if (fail_load)
         CHECK(s->dropped && s->handle == old, "failed replacement was not reported");
      else
         CHECK(!s->dropped && s->handle != old && st_loads == loads + 1,
               "refused update did not load the retained image");
      gfx_surface_free(s);
      CHECK(st_live == 0, "recovery leaked textures");
   }
   st_load_fail = 0;
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      CHECK(gfx_surface_submit_image(s, st_image(8)), "filter: initial image");
      gfx_surface_still(&s, TEXTURE_FILTER_LINEAR);
      st_flush();
      CHECK(s->inflight && s->payload, "in-flight load lost a changed filter");
      st_flush();
      CHECK(!s->inflight && st_last_filter == TEXTURE_FILTER_LINEAR,
            "requested filter did not replace the installed one");
      CHECK(gfx_surface_submit_image(s, st_image(8)), "filter: update image");
      gfx_surface_still(&s, TEXTURE_FILTER_NEAREST);
      st_flush();
      CHECK(s->inflight && s->payload, "in-flight update lost a changed filter");
      st_flush();
      CHECK(!s->inflight && st_last_filter == TEXTURE_FILTER_NEAREST,
            "update retained the old texture filter");
      gfx_surface_free(s);
   }
   {
      enum texture_filter_type filters[2] = {
         TEXTURE_FILTER_MIPMAP_LINEAR, TEXTURE_FILTER_MIPMAP_NEAREST};
      unsigned i;
      for (i = 0; i < 2; i++)
      {
         gfx_surface_t *s = gfx_surface_new_still(filters[i]);
         int loads, updates;
         CHECK(gfx_surface_submit_image(s, st_image(8)), "mips: initial image");
         st_flush();
         loads = st_loads;
         updates = st_updates;
         CHECK(gfx_surface_submit_image(s, st_image(8)), "mips: replacement image");
         st_flush();
         CHECK(st_loads == loads + 1 && st_updates == updates && st_last_filter == filters[i],
               "mipmapped still reused a stale chain");
         gfx_surface_free(s);
      }
   }
   {
      unsigned phase;
      for (phase = 0; phase < 2; phase++)
      {
         gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
         CHECK(gfx_surface_submit_image(s, st_image(8)), "cancel: initial image");
         st_flush();
         st_refuse = 1;
         CHECK(gfx_surface_submit_image(s, st_image(8)), "cancel: replacement image");
         if (phase)
            st_flush();
         gfx_surface_free(s);
         st_flush();
         CHECK(st_live == 0 && !st_head, "cancelled recovery leaked work or textures");
      }
   }
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      uintptr_t old;
      CHECK(gfx_surface_submit_image(s, st_image(8)), "post failure: initial image");
      st_flush();
      old = s->handle;
      st_refuse = 1;
      CHECK(gfx_surface_submit_image(s, st_image(8)), "post failure: replacement image");
      st_post_fail = 1;
      st_flush();
      CHECK(!s->inflight && !s->payload && s->dropped && s->handle == old,
            "failed recovery post did not settle ownership and failure");
      CHECK(gfx_surface_submit_image(s, st_image(8)), "post failure: subsequent image");
      st_flush();
      CHECK(s->handle != old && !s->dropped, "surface did not recover after post failure");
      gfx_surface_free(s);
   }
   CHECK(st_live == 0, "still recovery/filter tests leaked textures");
}

int main(void)
{
   uintptr_t a, b;

   st_decode_lock = slock_new();
   st_retired_imports();
   st_retired_lent_textures();
   st_pitched_lending();
   st_async = 1;
   st_still_recovery();

   /* 1. threaded: a same-size image updates the texture up in place
    *    once landed, another size replaces it, and one given while
    *    the first is in flight goes up after it */
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      int loads;
      unsigned retired;
      CHECK(s != NULL, "no still");
      CHECK(gfx_surface_submit_image(s, st_image(8)), "first image refused");
      CHECK(s->handle == 0, "a queued image landed early");
      st_flush();
      a = s->handle;
      CHECK(a != 0, "first image never landed");
      loads = st_loads;
      CHECK(gfx_surface_submit_image(s, st_image(8)), "second image refused");
      CHECK(gfx_surface_submit_image(s, st_image(16)),
            "an image during the flight refused");
      CHECK(s->handle == a && st_live == 1, "the texture up went early");
      retired = gfx_surface_retired();
      st_flush();
      CHECK(s->handle == a && st_loads == loads,
            "a same-size image did not update in place");
      CHECK(gfx_surface_retired() == retired,
            "an update in place counted as a retired texture");
      st_flush();
      b = s->handle;
      /* A handle kept from before is the caller's to take again */
      CHECK(gfx_surface_retired() != retired,
            "a replacement retired a texture without saying so");
      retired = gfx_surface_retired();
      CHECK(b != 0 && b != a && VIDEO_SCALE_W(s->dims) == 16 && st_live == 1,
            "the image given in flight did not go up after (%d live)",
            st_live);
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d textures live after the still went", st_live);
      CHECK(gfx_surface_retired() != retired,
            "a freed still retired its texture without saying so");
   }

   /* 2. a still freed with an upload in flight: the completion frees
    *    it, texture and all */
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      CHECK(gfx_surface_submit_image(s, st_image(8)), "image refused");
      gfx_surface_submit_image(s, st_image(8)); /* queued behind */
      gfx_surface_free(s);
      st_flush();
      CHECK(st_live == 0, "a still freed in flight left %d live", st_live);
   }

   /* 3. a decode: the file's image goes up when the decode answers;
    *    a newer path makes the older decode land nowhere; a free
    *    while decoding is honoured at the answer */
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      retro_task_callback_t first_cb; void *first_user;
      CHECK(gfx_surface_submit_path(s, "a.png", true), "decode refused");
      first_cb   = st_decode_cb;
      first_user = st_decode_user;
      CHECK(gfx_surface_submit_path(s, "b.png", true), "second decode refused");
      /* the first answers: stale, lands nowhere */
      {
         struct texture_image *img = st_image(4);
         first_cb(NULL, img, first_user, NULL);
      }
      st_flush();
      CHECK(s->handle == 0 && st_live == 0, "a stale decode landed");
      st_decode_done(8);
      st_flush();
      CHECK(s->handle != 0 && VIDEO_SCALE_W(s->dims) == 8 && st_live == 1,
            "the decode did not land");
      CHECK(gfx_surface_submit_path(s, "c.png", true), "third decode refused");
      gfx_surface_free(s);
      CHECK(st_live == 1, "a still freed while decoding went at once");
      st_decode_done(8);
      st_flush();
      CHECK(st_live == 0, "a still freed while decoding left %d live",
            st_live);
   }

   /* 5. threaded: an update the driver drops is sent again, with its
    *    image, and lands */
   st_async = 1;
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      int updates;
      CHECK(gfx_surface_submit_image(s, st_image(8)), "first image refused");
      st_flush();
      a = s->handle;
      updates = st_updates;
      st_drop = 1;
      CHECK(gfx_surface_submit_image(s, st_image(8)), "second image refused");
      st_flush();                 /* dropped: sent again */
      CHECK(s->inflight, "a dropped update was not sent again");
      st_flush();                 /* lands */
      CHECK(!s->inflight && s->handle == a && st_updates == updates + 2,
            "the update sent again did not land (%d updates)",
            st_updates - updates);
      CHECK(st_null_updates == 0, "%d update(s) reached the driver with no image",
            st_null_updates);
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d live after the dropped-update still went",
            st_live);
   }

   /* 6. a decode out while many newer ones come and go lands nowhere,
    *    however many: the generation does not come round on it */
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      retro_task_callback_t first_cb; void *first_user;
      unsigned n;
      CHECK(gfx_surface_submit_path(s, "old.png", true), "decode refused");
      first_cb   = st_decode_cb;
      first_user = st_decode_user;
      for (n = 0; n < 256; n++)
      {
         CHECK(gfx_surface_submit_path(s, "new.png", true),
               "decode %u refused", n);
         st_decode_done(8);
         st_flush();
      }
      b = s->handle;
      CHECK(b != 0 && st_live == 1, "the newest decode did not land");
      first_cb(NULL, st_image(4), first_user, NULL);
      st_flush();
      CHECK(s->handle == b && VIDEO_SCALE_W(s->dims) == 8 && st_live == 1,
            "a decode %u requests old landed", n);
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d live after the still went", st_live);
   }

   /* 7. threaded: a set of files is decoded across threads and goes
    *    up in slot order; a file that is not there and a slot that is
    *    not there are skipped */
   {
      enum { SET_N = 70 };
      gfx_surface_t *slots[SET_N];
      unsigned i, up, loads = st_loads;
      memset(slots, 0, sizeof(slots));
      slots[5] = (gfx_surface_t*)NULL;
      st_decode_threads = 0;
      st_flush_threads  = 0;
      up = gfx_surface_submit_named(slots, SET_N, TEXTURE_FILTER_NEAREST,
            st_set_path, NULL, true);
      CHECK(up == SET_N - 1, "%u of %u files went up, wanted all but the missing one",
            up, SET_N);
      CHECK(slots[5] != NULL, "a slot with no still was not given one");
      CHECK(slots[3] && slots[3]->handle == 0 && slots[3]->dims == 0,
            "the missing file's slot got an image");
      st_flush();
      CHECK(st_loads - loads == SET_N - 1, "%d textures loaded for the set",
            st_loads - loads);
      for (i = 0; i < SET_N; i++)
      {
         if (i == 3)
            continue;
         CHECK(slots[i] && VIDEO_SCALE_W(slots[i]->dims) == 8 + i,
               "slot %u holds a %u-wide image, wanted %u", i,
               slots[i] ? VIDEO_SCALE_W(slots[i]->dims) : 0, 8 + i);
      }
      if (cpu_features_get_core_amount() > 1)
         CHECK(st_decode_threads > 1,
               "the set was decoded on one thread with %u cores",
               cpu_features_get_core_amount());
      /* every thread but this one that decoded let its pool go */
      {
         uintptr_t self = sthread_get_current_thread_id();
         unsigned j, k, missing = 0, own = 0;
         for (j = 0; j < st_decode_threads; j++)
         {
            bool found = false;
            for (k = 0; k < st_flush_threads; k++)
               if (st_flush_thread[k] == st_decode_thread[j])
                  found = true;
            if (st_decode_thread[j] != self && !found)
               missing++;
         }
         for (k = 0; k < st_flush_threads; k++)
            if (st_flush_thread[k] == self)
               own++;
         CHECK(!missing && !own,
               "%u decode threads exited holding their read pools, "
               "%u pool flushes on the caller's thread", missing, own);
      }
      for (i = 0; i < SET_N; i++)
         gfx_surface_free(slots[i]);
      CHECK(st_live == 0, "%d live after the set went", st_live);
   }

   /* 8. direct: the image goes up at once */
   st_async = 0;
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      CHECK(gfx_surface_submit_image(s, st_image(8)) && s->handle != 0,
            "direct image did not land");
      a = s->handle;
      CHECK(gfx_surface_submit_image(s, st_image(8)) && s->handle != a
            && st_last_unloaded == a && st_live == 1,
            "direct replacement left %d live", st_live);
      /* an image the caller keeps: its pixels move to the still */
      {
         struct texture_image *img = st_image(16);
         CHECK(gfx_surface_take_image(s, img) && !img->pixels
               && VIDEO_SCALE_W(s->dims) == 16 && st_live == 1,
               "a taken image did not go up");
         image_texture_free(img);
         free(img);
      }
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d textures live at the end", st_live);
   }

   /* 9. planar: converted once from where the planes lie, the slot
    * never lent, the colour space the surface's */
   st_async = 0;
   {
      static uint8_t ext[2][64];
      gfx_surface_planes_t p;
      gfx_surface_t *s = gfx_surface_new(VIDEO_SCALE_PACK(5, 3), 1,
            IMAGE_PIXFMT_I420, TEXTURE_FILTER_NEAREST, NULL, NULL);
      uint8_t *f;
      CHECK(!gfx_surface_new(VIDEO_SCALE_PACK(4, 4), 1, IMAGE_PIXFMT_P010,
               TEXTURE_FILTER_NEAREST, NULL, NULL), "P010 surface made");
      CHECK(s && GFX_SURFACE_IS_PLANAR(s) && !s->rgb, "no planar surface");
      /* 5x3: 15 luma, two 3x2 chroma planes */
      f = (uint8_t*)gfx_surface_slot_begin(s, 0);
      memset(f, 235, 15);
      memset(f + 15, 128, 12);
      f[14] = 16;                   /* the odd corner: black */
      gfx_surface_slot_end(s, 0);
      CHECK(gfx_surface_submit(s, 0, false) == GFX_SURFACE_SUBMIT_DONE
            && st_px_first == 0xffffffffu && st_px_last == 0xff000000u,
            "I420 slot read %08x..%08x", (unsigned)st_px_first,
            (unsigned)st_px_last);
      CHECK(!s->lent, "a planar slot was lent the driver's memory");
      CHECK(gfx_surface_submit(s, 0, false) == GFX_SURFACE_SUBMIT_DONE
            && st_live == 1, "update made %d textures", st_live);
      /* the caller's planes, padded rows, full-range BT.709 grey */
      memset(ext, 128, sizeof(ext));
      p.planes[0]   = ext[0];
      p.planes[1]   = ext[1];
      p.planes[2]   = ext[1] + 32;
      p.strides[0]  = 8;
      p.strides[1]  = p.strides[2] = 4;
      p.chroma_step = 1;
      gfx_surface_set_yuv(s, IMAGE_YUV_FLAG_BT709 | IMAGE_YUV_FLAG_FULL_RANGE);
      CHECK(gfx_surface_submit_planes(s, &p, true)
               == GFX_SURFACE_SUBMIT_DONE
            && st_px_first == 0xff808080u && st_live == 1,
            "external planes read %08x, %d live", (unsigned)st_px_first,
            st_live);
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d live after the planar surface", st_live);
   }
   /* NV12 under the wrapper: queued, busy until it lands */
   st_async = 1;
   {
      gfx_surface_t *s = gfx_surface_new(VIDEO_SCALE_PACK(2, 2), 2,
            IMAGE_PIXFMT_NV12, TEXTURE_FILTER_NEAREST, NULL, NULL);
      uint8_t *f = (uint8_t*)s->slots[1];
      memset(f, 16, 4);
      f[4] = 128;                    /* Cb */
      f[5] = 240;                    /* Cr: red */
      CHECK(gfx_surface_submit(s, 1, false) == GFX_SURFACE_SUBMIT_QUEUED,
            "NV12 not queued");
      CHECK(gfx_surface_submit_pixels(s, f, false)
            == GFX_SURFACE_SUBMIT_BUSY, "second NV12 frame not busy");
      st_flush();
      CHECK(st_live == 1 && (st_px_first & 0x00ff0000u) > 0x00a00000u
            && (st_px_first & 0xffu) < 0x10u,
            "NV12 red read %08x", (unsigned)st_px_first);
      CHECK(gfx_surface_submit_pixels(s, f, false)
               == GFX_SURFACE_SUBMIT_QUEUED && !s->node.lend,
            "planar update asked to be lent");
      st_flush();
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d live after NV12", st_live);
   }
   st_async = 0;

   /* A driver that converts on the GPU: given the planes as they lie,
    * nothing converted, the caller's planes copied only for the
    * wrapper, Cr first swapped back to Cb first */
   st_yuv420 = 1;
   {
      static uint8_t frame[64];
      gfx_surface_planes_t p;
      gfx_surface_t *s = gfx_surface_new(VIDEO_SCALE_PACK(4, 4), 1,
            IMAGE_PIXFMT_NV12, TEXTURE_FILTER_NEAREST, NULL, NULL);
      int reads = st_planar_reads;
      memset(frame, 0, sizeof(frame));
      frame[16] = 1;  /* Cb, Cr of the first pair */
      frame[17] = 2;
      p.planes[0]   = frame;
      p.planes[1]   = frame + 16;
      p.planes[2]   = frame + 17;
      p.strides[0]  = 4;
      p.strides[1]  = p.strides[2] = 8;  /* padded chroma rows */
      p.chroma_step = 2;
      st_async = 0;
      gfx_surface_set_yuv(s, IMAGE_YUV_FLAG_VU | IMAGE_YUV_FLAG_BT709);
      CHECK(gfx_surface_submit_planes(s, &p, false)
               == GFX_SURFACE_SUBMIT_DONE
            && st_planar_reads == reads + 1
            && st_planar.planes[0] == frame
            && st_planar.planes[1] == frame + 17
            && st_planar.planes[2] == frame + 16
            && st_planar.strides[1] == 8
            && st_planar.yuv == IMAGE_YUV_FLAG_BT709
            && !s->rgb,
            "direct planes not handed over as they lie");
      st_async = 1;
      CHECK(gfx_surface_submit_planes(s, &p, false)
            == GFX_SURFACE_SUBMIT_QUEUED, "wrapper planes not queued");
      memset(frame, 0xee, sizeof(frame)); /* the caller's again */
      st_flush();
      CHECK(st_planar.planes[0] == (const uint8_t*)s->slots[0]
            && st_planar.planes[1][0] == 2 && st_planar.planes[2][0] == 1
            && st_planar.strides[1] == 4 && st_planar.planes[0][0] == 0
            && !s->rgb, "wrapper planes not copied into the slot");
      st_async = 0;
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d live after GPU planar", st_live);
   }
   st_yuv420 = 0;

   printf("%s\n", failures ? "FAILED" : "PASS");
   return failures ? 1 : 0;
}

bool video_driver_texture_lend_needs_refresh(void) { return false; }
bool video_thread_lend_get(int idx, void **mem, size_t *pitch)
{
   (void)idx;
   (void)mem;
   (void)pitch;
   return false;
}
