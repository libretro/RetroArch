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

/* --- stub driver ------------------------------------------------- */
static uintptr_t st_next = 1;
static int       st_live, st_loads, st_unloads, st_async;
/* Updates the stub driver drops, and updates it was handed without
 * an image to read */
static int       st_drop, st_updates, st_null_updates;
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
      st_px_last  = ti->pixels[(size_t)ti->width * ti->height - 1];
   }
}

bool video_driver_texture_load(void *data,
      enum texture_filter_type filter, uintptr_t *id)
{
   (void)filter;
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
   return fmt == TEXTURE_GPU_FORMAT_YUV420 && st_yuv420;
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
void *video_driver_texture_lend(uintptr_t id, unsigned slot, size_t pitch)
{
   (void)id; (void)slot; (void)pitch;
   return NULL;
}
bool video_driver_texture_lend_ready(uintptr_t id, unsigned slot)
{
   (void)id; (void)slot;
   return true;
}

/* Nothing is lent here: the wrapper's records never exist */
bool video_thread_lend_ready(int idx) { (void)idx; return true; }

bool video_thread_async_post(video_thread_async_load_t *n)
{
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
      if (n->kind == VIDEO_THREAD_ASYNC_UPDATE)
      {
         id         = n->handle;
         n->dropped = video_driver_texture_update(id, n->img)
               == VIDEO_TEXTURE_UPDATE_DROPPED;
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
static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; \
   printf("[FAIL] " __VA_ARGS__); printf("\n"); } } while (0)

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

int main(void)
{
   uintptr_t a, b;

   st_decode_lock = slock_new();
   st_async = 1;

   /* 1. threaded: a same-size image updates the texture up in place
    *    once landed, another size replaces it, and one given while
    *    the first is in flight goes up after it */
   {
      gfx_surface_t *s = gfx_surface_new_still(TEXTURE_FILTER_NEAREST);
      int loads;
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
      st_flush();
      CHECK(s->handle == a && st_loads == loads,
            "a same-size image did not update in place");
      st_flush();
      b = s->handle;
      CHECK(b != 0 && b != a && VIDEO_SCALE_W(s->dims) == 16 && st_live == 1,
            "the image given in flight did not go up after (%d live)",
            st_live);
      gfx_surface_free(s);
      CHECK(st_live == 0, "%d textures live after the still went", st_live);
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
