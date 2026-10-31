/* tasks/task_translation.c's reply image, drawn straight into the
 * frame when the video driver has no widget overlay.
 *
 * The image comes from the translation server and is untrusted: a
 * reply that is short, truncated, or describes more pixels than it
 * holds draws nothing and reads nothing past itself. A well-formed
 * one is drawn at the frame's size whatever its own - narrower,
 * wider, a width whose rows BMP pads to four bytes, bottom-up or
 * top-down - with its rows where they belong, in either output
 * format. A PNG that does not decode draws nothing. Every lane runs
 * the reply through the real scaler and leaves nothing allocated,
 * which ASan's leak check holds to.
 *
 * The frontend edges and rpng are stood in for here.  One lane runs
 * the auto-translate task, which waits for the speech it follows. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

static void *reply_malloc(size_t size);
static void *reply_calloc(size_t count, size_t size);
static void reply_free(void *ptr);
#define malloc reply_malloc
#define calloc reply_calloc
#define free reply_free
#include TRANSLATION_SOURCE
#undef malloc
#undef calloc
#undef free

static unsigned failures = 0;

static void check(int ok, const char *fmt, ...)
{
   va_list ap;
   if (ok)
      return;
   printf("FAIL: ");
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   printf("\n");
   failures++;
}

static struct { void *ptr; size_t size; } g_allocations[8];
static size_t g_live, g_peak, g_total;
static unsigned g_alloc_calls;
static int g_alloc_budget = -1;
static const void *g_response_ptr;
static size_t g_response_size;
static void *g_decoded_pixels;

static void *reply_malloc(size_t size)
{
   void *ptr;
   unsigned i;
   g_alloc_calls++;
   if (g_alloc_budget == 0)
      return NULL;
   if (g_alloc_budget > 0)
      g_alloc_budget--;
   if (!(ptr = malloc(size)))
      return NULL;
   for (i = 0; i < 8; i++)
      if (!g_allocations[i].ptr)
      {
         g_allocations[i].ptr = ptr;
         g_allocations[i].size = size;
         g_live += size;
         g_total += size;
         if (g_live > g_peak)
            g_peak = g_live;
         return ptr;
      }
   check(0, "allocation tracker full");
   free(ptr);
   return NULL;
}

static void *reply_calloc(size_t count, size_t size)
{
   void *ptr;
   if (size && count > (size_t)-1 / size)
      return NULL;
   ptr = reply_malloc(count * size);
   if (ptr)
      memset(ptr, 0, count * size);
   return ptr;
}

static void reply_free(void *ptr)
{
   unsigned i;
   if (ptr && g_response_ptr && (uintptr_t)ptr >= (uintptr_t)g_response_ptr
         && (uintptr_t)ptr - (uintptr_t)g_response_ptr < g_response_size)
   {
      check(0, "callback freed borrowed response bytes");
      return;
   }
   for (i = 0; i < 8; i++)
      if (ptr && g_allocations[i].ptr == ptr)
      {
         g_live -= g_allocations[i].size;
         g_allocations[i].ptr = NULL;
         break;
      }
   if (ptr == g_decoded_pixels)
      g_decoded_pixels = NULL;
   free(ptr);
}

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

/* ---- the frontend ---- */
static settings_t           g_settings;
static video_driver_state_t g_video;
static access_state_t       g_access;
static unsigned             g_dims;

settings_t *config_get_ptr(void) { return &g_settings; }
video_driver_state_t *video_state_get_ptr(void) { return &g_video; }
access_state_t *access_state_get_ptr(void) { return &g_access; }
uint32_t runloop_get_flags(void) { return 0; }

/* The task queue and the mixer, for the auto-translate lane: a task the
 * test runs by hand, and speech that is playing until the lane says. */
static retro_task_t *g_pushed      = NULL;
static unsigned      g_parks       = 0;
static unsigned      g_calls       = 0;
static int           g_speech      = 0;
static int           g_hw_frame;
bool audio_driver_is_ai_service_speech_running(void) { return g_speech != 0; }
bool command_event(enum event_command cmd, void *data)
{ (void)data; if (cmd == CMD_EVENT_AI_SERVICE_CALL) g_calls++; return true; }
retro_task_t *task_init(void) { return (retro_task_t*)calloc(1, sizeof(retro_task_t)); }
bool task_queue_push(retro_task_t *task) { g_pushed = task; return true; }
uint8_t task_get_flags(retro_task_t *task) { return (uint8_t)task->flags; }
void task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{ if (set) task->flags |= flags; else task->flags &= ~flags; }
void task_queue_park(retro_task_t *task, retro_time_t horizon_usec)
{ (void)task; (void)horizon_usec; g_parks++; }
bool video_driver_cached_frame_is_hw_render(void) { return g_hw_frame != 0; }
bool video_driver_cached_frame_info(unsigned *dims, size_t *pitch,
      bool *has_cpu_pixels)
{
   if (dims)
      *dims = g_dims;
   if (pitch)
      *pitch = 0;
   if (has_cpu_pixels)
      *has_cpu_pixels = true;
   return true;
}

/* What was drawn: the frame is copied out whole, so a frame that
 * claims more than was written is read past its end under ASan. */
static unsigned  g_frames;
static unsigned  g_frame_w, g_frame_h;
static size_t    g_frame_pitch;
static uint8_t  *g_frame;
static int       g_reused_decode;

void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   size_t n = pitch * height;
   g_frames++;
   g_reused_decode = data == g_decoded_pixels;
   g_frame_w     = width;
   g_frame_h     = height;
   g_frame_pitch = pitch;
   free(g_frame);
   if ((g_frame = (uint8_t*)malloc(n ? n : 1)))
      memcpy(g_frame, data, n);
}

/* ---- rpng: native ARGB texels with varying alpha, or a failure ---- */
static int g_png_ok;
static unsigned g_png_width = 2, g_png_height = 2;
struct rpng { int unused; };
static struct rpng g_rpng;

rpng_t *rpng_alloc(void) { return &g_rpng; }
void rpng_free(rpng_t *r) { (void)r; }
bool rpng_set_buf_ptr(rpng_t *r, void *d, size_t n)
{ (void)r; (void)d; (void)n; return true; }
bool rpng_start(rpng_t *r) { (void)r; return true; }
bool rpng_iterate_image(rpng_t *r) { (void)r; return false; }
int rpng_process_image(rpng_t *r, void **data, size_t size,
      unsigned *w, unsigned *h, bool rgba)
{
   unsigned x, y;
   uint32_t *pixels;
   (void)r; (void)size; (void)rgba;
   if (!g_png_ok)
      return IMAGE_PROCESS_ERROR;
   *w    = g_png_width;
   *h    = g_png_height;
   pixels = (uint32_t*)malloc((size_t)*w * *h * 4);
   *data = g_decoded_pixels = pixels;
   if (pixels)
      for (y = 0; y < *h; y++)
         for (x = 0; x < *w; x++)
            pixels[(size_t)y * *w + x] = (((x + y * 3) * 67 & 255) << 24)
               | (((x * 31 + 17) & 255) << 16)
               | (((y * 53 + 29) & 255) << 8)
               | ((x * 19 + y * 7 + 13) & 255);
   return *data ? IMAGE_PROCESS_END : IMAGE_PROCESS_ERROR;
}

/* ---- replies ---- */

/* A 24-bit BMP whose pixel at column x, row y from the top is
 * B = x, G = y, R = 0x80; rows padded to four bytes. */
static uint8_t *make_bmp(int w, int h, size_t *len)
{
   int      ah  = h < 0 ? -h : h;
   size_t   row = ((size_t)w * 3 + 3) & ~(size_t)3;
   size_t   n   = 54 + row * ah;
   uint8_t *b   = (uint8_t*)calloc(1, n);
   int      x, y;
   if (!b)
      return NULL;
   b[0] = 'B'; b[1] = 'M';
   b[10] = 54;
   b[14] = 40;
   b[18] = w & 0xff; b[19] = (w >> 8) & 0xff;
   b[22] = h & 0xff; b[23] = (h >> 8) & 0xff;
   b[24] = (h >> 16) & 0xff; b[25] = (h >> 24) & 0xff;
   b[26] = 1;
   b[28] = 24;
   for (y = 0; y < ah; y++)
   {
      /* bottom-up unless the height is negative */
      int      mem = h < 0 ? y : ah - 1 - y;
      uint8_t *p   = b + 54 + row * mem;
      for (x = 0; x < w; x++)
      {
         p[x * 3 + 0] = (uint8_t)x;
         p[x * 3 + 1] = (uint8_t)y;
         p[x * 3 + 2] = 0x80;
      }
   }
   *len = n;
   return b;
}

static void reply(const void *data, size_t len)
{
   translation_response_t r;
   memset(&r, 0, sizeof(r));
   r.image_data = (void*)data;
   r.image_size = len;
   check(g_live == 0, "live allocations before reply");
   g_response_ptr = data;
   g_response_size = len;
   g_peak = g_total = 0;
   g_alloc_calls = 0;
   handle_translation_response(&r, NULL);
   check(g_live == 0, "reply leaked callback allocations");
   check(g_decoded_pixels == NULL, "reply leaked decoded PNG pixels");
   g_response_ptr = NULL;
}

/* Green and blue of the drawn pixel at (x, y). */
static void drawn(unsigned x, unsigned y, unsigned *g, unsigned *b)
{
   if (g_video.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
   {
      uint32_t p;
      memcpy(&p, g_frame + y * g_frame_pitch + x * 4, 4);
      *g = (p >> 8) & 0xff;
      *b = p & 0xff;
   }
   else
   {
      uint16_t p;
      memcpy(&p, g_frame + y * g_frame_pitch + x * 2, 2);
      *g = ((p >> 5) & 0x3f) << 2;
      *b = (p & 0x1f) << 3;
   }
}

static void start(unsigned fw, unsigned fh, enum retro_pixel_format fmt)
{
   g_dims          = VIDEO_SCALE_PACK(fw, fh);
   g_video.pix_fmt = fmt;
   g_frames        = 0;
   g_reused_decode = 0;
}

/* Same size as the frame, so the point scaler maps pixel to pixel:
 * rows land where they belong and each is read at its padded stride. */
static void lane_same_size(const char *tag, int w, int h,
      enum retro_pixel_format fmt)
{
   size_t   len;
   uint8_t *bmp = make_bmp(w, h, &len);
   unsigned ah  = (unsigned)(h < 0 ? -h : h);
   unsigned g, b;
   unsigned tol = fmt == RETRO_PIXEL_FORMAT_XRGB8888 ? 0 : 7;

   start((unsigned)w, ah, fmt);
   reply(bmp, len);
   check(g_frames == 1, "%s: drew %u frames", tag, g_frames);
   if (g_frames == 1)
   {
      check(g_frame_w == (unsigned)w && g_frame_h == ah,
            "%s: drew %ux%u", tag, g_frame_w, g_frame_h);
      drawn(0, 0, &g, &b);
      check(g <= tol && b <= tol, "%s: top-left is row %u col %u", tag, g, b);
      drawn(0, 1, &g, &b);
      check(g >= 1 - (tol ? 1 : 0) && g <= 1 + tol && b <= tol,
            "%s: second row starts with row %u col %u", tag, g, b);
      drawn((unsigned)w - 1, ah - 1, &g, &b);
      check(g + tol >= ah - 1 && b + tol >= (unsigned)w - 1,
            "%s: bottom-right is row %u col %u", tag, g, b);
   }
   free(bmp);
   printf("      %-48s done\n", tag);
}

/* A different size from the frame: drawn at the frame's size, read
 * only within the image. */
static void lane_scaled(const char *tag, int w, int h, unsigned fw,
      unsigned fh)
{
   size_t   len;
   uint8_t *bmp = make_bmp(w, h, &len);
   unsigned g, b;

   start(fw, fh, RETRO_PIXEL_FORMAT_XRGB8888);
   reply(bmp, len);
   check(g_frames == 1, "%s: drew %u frames", tag, g_frames);
   if (g_frames == 1)
   {
      check(g_frame_w == fw && g_frame_h == fh,
            "%s: drew %ux%u for a %ux%u frame", tag, g_frame_w, g_frame_h,
            fw, fh);
      drawn(0, 0, &g, &b);
      check(g == 0 && b == 0, "%s: top-left is row %u col %u", tag, g, b);
      drawn(0, fh - 1, &g, &b);
      check(g >= (unsigned)h / 2, "%s: bottom row is image row %u", tag, g);
   }
   free(bmp);
   printf("      %-48s done\n", tag);
}

/* A reply that must draw nothing. */
static void lane_refused(const char *tag, const void *data, size_t len)
{
   start(64, 48, RETRO_PIXEL_FORMAT_XRGB8888);
   reply(data, len);
   check(g_frames == 0, "%s: drew %u frames", tag, g_frames);
   printf("      %-48s done\n", tag);
}

/* Speech mode: the follow-up request waits for the speech to end.  It
 * runs on the main thread, where the request it sets off belongs, and
 * parks between looks. */
static void lane_auto_translate_waits(void)
{
   bool was_paused = false;

   g_pushed = NULL;
   g_parks  = 0;
   g_calls  = 0;
   g_speech = 1;
   ai_service_call_auto_translate_task(&g_access, 1, &was_paused);
   check(g_pushed != NULL, "auto-translate: no task pushed");
   if (!g_pushed)
      return;
   check((g_pushed->flags & RETRO_TASK_FLG_MAIN_THREAD) != 0,
         "auto-translate: the task is not a main-thread task");

   g_pushed->handler(g_pushed);
   check(g_parks == 1, "auto-translate: %u parks while speech plays", g_parks);
   check(!(g_pushed->flags & RETRO_TASK_FLG_FINISHED) && g_calls == 0,
         "auto-translate: finished while speech plays");

   g_speech = 0;
   g_pushed->handler(g_pushed);
   check((g_pushed->flags & RETRO_TASK_FLG_FINISHED) && g_calls == 1,
         "auto-translate: the next request did not follow the speech");
   free(g_pushed);
   g_pushed = NULL;
   printf("      %-48s done\n", "auto-translate waits for the speech");
}

static const uint8_t png_reply[32] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };

#ifdef HAVE_RPNG
static uint32_t png_pixel(unsigned x, unsigned y)
{
   return 0xff000000u | (((x * 31 + 17) & 255) << 16)
      | (((y * 53 + 29) & 255) << 8) | ((x * 19 + y * 7 + 13) & 255);
}

static void lane_png(unsigned fw, unsigned fh, enum retro_pixel_format fmt)
{
   unsigned x, y;
   int xs = (7 << 16) / fw, ys = (5 << 16) / fh;
   int xp = (7 << 15) / fw - (1 << 15);
   int yp = (5 << 15) / fh - (1 << 15);
   if (xp < 0) xp = 0;
   if (yp < 0) yp = 0;
   g_png_width = 7;
   g_png_height = 5;
   g_png_ok = 1;
   start(fw, fh, fmt);
   reply(png_reply, sizeof(png_reply));
   check(g_frames == 1, "PNG %ux%u: no frame", fw, fh);
   if (g_frames != 1)
      return;
   if (fw == 7 && fh == 5)
      check(g_alloc_calls == 0 && g_total == 0 && g_reused_decode,
            "native PNG frame was not reused without allocations");
   else
      check(g_alloc_calls == 2 && g_total == sizeof(struct scaler_ctx)
            + (size_t)fw * fh * (fmt == RETRO_PIXEL_FORMAT_XRGB8888 ? 4 : 2)
            && !g_reused_decode, "PNG conversion did not use its own output");
   for (y = 0; y < fh; y++)
      for (x = 0; x < fw; x++)
      {
         uint32_t p = png_pixel((xp + x * xs) >> 16, (yp + y * ys) >> 16);
         if (fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         {
            uint32_t got;
            memcpy(&got, g_frame + y * g_frame_pitch + x * 4, 4);
            check(got == p, "PNG pixel %u,%u: %08x != %08x", x, y, got, p);
         }
         else
         {
            uint16_t got;
            uint16_t want = (uint16_t)((((p >> 19) & 31) << 11)
                  | (((p >> 10) & 63) << 5) | ((p >> 3) & 31));
            memcpy(&got, g_frame + y * g_frame_pitch + x * 2, 2);
            check(got == want, "PNG RGB565 pixel %u,%u", x, y);
         }
      }
}

static void lane_png_guards(void)
{
   g_png_ok = 1;
   g_png_width = 7;
   g_png_height = 5;
   start(7, 5, RETRO_PIXEL_FORMAT_XRGB8888);
   g_hw_frame = 1;
   reply(png_reply, sizeof(png_reply));
   check(g_frames == 0, "native PNG bypassed the hardware-frame guard");
   g_hw_frame = 0;
   start(0, 5, RETRO_PIXEL_FORMAT_XRGB8888);
   reply(png_reply, sizeof(png_reply));
   check(g_frames == 0, "native PNG accepted a zero-width frame");
   start(7, 5, RETRO_PIXEL_FORMAT_SCRGB_RGBA16F);
   reply(png_reply, sizeof(png_reply));
   check(g_frames == 0, "native PNG accepted an HDR software frame");
}
#endif

static void lane_allocation_failures(void)
{
   size_t len;
   uint8_t *bmp = make_bmp(7, 5, &len);
   int png, budget, formats = 1;
#ifdef HAVE_RPNG
   formats = 5;
#endif
   for (png = 0; png < formats; png++)
      for (budget = 0; budget <= 2; budget++)
      {
         start(png == 2 || png == 4 ? 3 : 7, png == 2 || png == 4 ? 2 : 5,
               png >= 3 ? RETRO_PIXEL_FORMAT_RGB565 : RETRO_PIXEL_FORMAT_XRGB8888);
         g_png_ok = 1;
         g_png_width = 7;
         g_png_height = 5;
         g_alloc_budget = budget;
         reply(png ? png_reply : bmp, png ? sizeof(png_reply) : len);
         g_alloc_budget = -1;
         check(g_frames == (png == 1 || png == 3 || budget == 2 ? 1u : 0u),
               "image case %d allocation budget %d: %u frames", png,
               budget, g_frames);
      }
   free(bmp);
}

static void lane_odd_offset(void)
{
   size_t len;
   uint8_t *bmp = make_bmp(7, 5, &len);
   uint8_t *shifted = (uint8_t*)malloc(len + 1);
   unsigned g, b;
   memcpy(shifted, bmp, 54);
   shifted[10] = 55;
   shifted[54] = 0xa5;
   memcpy(shifted + 55, bmp + 54, len - 54);
   start(7, 5, RETRO_PIXEL_FORMAT_XRGB8888);
   reply(shifted, len + 1);
   check(g_frames == 1, "unaligned BMP pixels: no frame");
   if (g_frames == 1)
   {
      drawn(6, 4, &g, &b);
      check(g == 4 && b == 6, "unaligned BMP pixels: wrong color");
      check(g_alloc_calls == 2, "BMP copied borrowed pixels");
   }
   free(bmp);
   free(shifted);
}

static void benchmark(void)
{
   size_t len;
   uint8_t *bmp = make_bmp(1920, 1080, &len);
   unsigned png, i, formats = 1;
#ifdef HAVE_RPNG
   formats = 2;
#endif
   g_png_ok = 1;
   g_png_width = 1920;
   g_png_height = 1080;
   for (png = 0; png < formats; png++)
   {
      clock_t begin = clock();
      for (i = 0; i < 20; i++)
      {
         start(640, 360, RETRO_PIXEL_FORMAT_XRGB8888);
         reply(png ? png_reply : bmp, png ? sizeof(png_reply) : len);
      }
      printf("%s 1920x1080 -> 640x360: %.3f ms, callback peak %lu bytes, %u allocations\n",
            png ? "PNG" : "BMP", 1000.0 * (clock() - begin) / CLOCKS_PER_SEC / 20,
            (unsigned long)g_peak, g_alloc_calls);
   }
#ifdef HAVE_RPNG
   for (png = 0; png < 2; png++)
   {
      clock_t begin = clock();
      for (i = 0; i < 20; i++)
      {
         start(1920, 1080, png ? RETRO_PIXEL_FORMAT_RGB565 : RETRO_PIXEL_FORMAT_XRGB8888);
         reply(png_reply, sizeof(png_reply));
      }
      printf("PNG native 1920x1080 %s: %.3f ms, callback peak %lu bytes, %u allocations, reused=%d\n",
            png ? "RGB565" : "XRGB8888", 1000.0 * (clock() - begin) / CLOCKS_PER_SEC / 20,
            (unsigned long)g_peak, g_alloc_calls, g_reused_decode);
   }
#endif
   free(bmp);
   free(g_frame);
   g_frame = NULL;
}

int main(int argc, char **argv)
{
   size_t   len;
   uint8_t *bmp;
   (void)argv;
   if (argc > 1)
   {
      benchmark();
      return failures ? 1 : 0;
   }

   lane_same_size("same size, XRGB8888",              64, 48,
         RETRO_PIXEL_FORMAT_XRGB8888);
   lane_same_size("same size, padded rows",           37, 20,
         RETRO_PIXEL_FORMAT_XRGB8888);
   lane_same_size("same size, top-down",              37, -20,
         RETRO_PIXEL_FORMAT_XRGB8888);
   lane_same_size("same size, RGB565",                64, 48,
         RETRO_PIXEL_FORMAT_RGB565);
   lane_scaled("narrower than the frame",             32, 48, 64, 48);
   lane_auto_translate_waits();
   lane_scaled("larger than the frame",               128, 96, 64, 48);
#ifdef HAVE_RPNG
   lane_png(7, 5, RETRO_PIXEL_FORMAT_XRGB8888);
   lane_png(3, 2, RETRO_PIXEL_FORMAT_XRGB8888);
   lane_png(13, 9, RETRO_PIXEL_FORMAT_XRGB8888);
   lane_png(7, 5, RETRO_PIXEL_FORMAT_RGB565);
   lane_png(3, 2, RETRO_PIXEL_FORMAT_RGB565);
   lane_png(13, 9, RETRO_PIXEL_FORMAT_RGB565);
   lane_png_guards();
#endif
   lane_allocation_failures();
   lane_odd_offset();

   bmp = make_bmp(64, 48, &len);
   lane_refused("BMP shorter than its pixels",        bmp, len - 100);
   lane_refused("BMP shorter than its header",        bmp, 40);
   bmp[28] = 32;
   lane_refused("BMP that is not 24-bit",             bmp, len);
   bmp[28] = 24;
   bmp[18] = bmp[19] = 0;
   lane_refused("BMP with no width",                  bmp, len);
   free(bmp);
   lane_refused("three bytes",                        "BMx", 3);

   {
      static const uint8_t png[32] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
      g_png_ok = 0;
      lane_refused("PNG that does not decode",        png, sizeof(png));
      g_png_ok = 1;
      start(64, 48, RETRO_PIXEL_FORMAT_XRGB8888);
      reply(png, sizeof(png));
#ifdef HAVE_RPNG
      check(g_frames == 1 && g_frame_w == 64 && g_frame_h == 48,
            "a PNG that decodes is drawn at the frame's size");
#else
      check(g_frames == 0, "PNG drawn without RPNG support");
#endif
      printf("      %-48s done\n", "PNG decode support");
   }

   free(g_frame);

   if (failures)
   {
      printf("[fail] translation_reply_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] translation_reply_test\n");
   return 0;
}
