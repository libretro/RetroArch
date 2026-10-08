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
 * The frontend edges and rpng are stood in for here. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../../../tasks/task_translation.c"

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
bool video_driver_cached_frame_is_hw_render(void) { return false; }
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

void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   size_t n = pitch * height;
   g_frames++;
   g_frame_w     = width;
   g_frame_h     = height;
   g_frame_pitch = pitch;
   free(g_frame);
   if ((g_frame = (uint8_t*)malloc(n ? n : 1)))
      memcpy(g_frame, data, n);
}

/* ---- rpng: a decode that fails, or one that hands back 2x2 ---- */
static int g_png_ok;
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
   (void)r; (void)size; (void)rgba;
   if (!g_png_ok)
      return IMAGE_PROCESS_ERROR;
   *w    = 2;
   *h    = 2;
   *data = calloc(2 * 2, 4);
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
   handle_translation_response(&r, NULL);
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

int main(void)
{
   size_t   len;
   uint8_t *bmp;

   lane_same_size("same size, XRGB8888",              64, 48,
         RETRO_PIXEL_FORMAT_XRGB8888);
   lane_same_size("same size, padded rows",           37, 20,
         RETRO_PIXEL_FORMAT_XRGB8888);
   lane_same_size("same size, top-down",              37, -20,
         RETRO_PIXEL_FORMAT_XRGB8888);
   lane_same_size("same size, RGB565",                64, 48,
         RETRO_PIXEL_FORMAT_RGB565);
   lane_scaled("narrower than the frame",             32, 48, 64, 48);
   lane_scaled("larger than the frame",               128, 96, 64, 48);

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
      check(g_frames == 1 && g_frame_w == 64 && g_frame_h == 48,
            "a PNG that decodes is drawn at the frame's size");
      printf("      %-48s done\n", "PNG that decodes");
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
