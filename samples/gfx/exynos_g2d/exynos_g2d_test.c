/* gfx/drivers/exynos_gfx.c against a fake kernel and a fake G2D.
 *
 * The G2D here does what the block does - fills, scaled copies, scaled
 * blends - into the GEM buffers it is handed, so what a page holds when
 * it is flipped to is what the screen shows. The KMS side counts the
 * framebuffers, the flips and the waits for them, and the cards opened
 * and closed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

#include "../../../gfx/drivers/exynos_gfx.c"
#include "../../../gfx/common/drm_common.c"

/* ---- frontend pieces the driver reaches ------------------------------ */

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];
static float drv_aspect = 4.0f / 3.0f;
float video_driver_get_aspect_ratio(void) { return drv_aspect; }
static video_driver_state_t video_st;
video_driver_state_t *video_state_get_ptr(void) { return &video_st; }
static unsigned retires;
void video_driver_cached_frame_retire(void) { retires++; }
static unsigned output_dims;
void video_driver_set_output_dims(unsigned dims) { output_dims = dims; }
static enum rarch_display_type display_type;
void video_driver_display_type_set(enum rarch_display_type t) { display_type = t; }
static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }

static unsigned memcpys;
void *memcpy_neon(void *dst, const void *src, size_t n)
{
   memcpys++;
   return memcpy(dst, src, n);
}

/* A font of one glyph, 'A': a solid 3x5 block from the pen */
static uint8_t           font_cov[16 * 16];
static struct font_atlas font_atl;
static struct font_glyph font_a;
static void *fake_font_init(uint8_t *d, size_t l, unsigned f, float s,
      enum font_atlas_format fmt)
{ (void)d; (void)l; (void)f; (void)s; (void)fmt; return &font_atl; }
static struct font_atlas *fake_font_atlas(void *d) { (void)d; return &font_atl; }
static const struct font_glyph *fake_font_glyph(void *d, uint32_t c)
{ (void)d; return c == 'A' ? &font_a : NULL; }
static void fake_font_free(void *d) { (void)d; }
static void fake_font_metrics(void *d, struct font_line_metrics **m)
{
   static struct font_line_metrics lm = { 10.0f, 8.0f, 2.0f };
   (void)d;
   *m = &lm;
}
static font_renderer_driver_t fake_font = { fake_font_init,
   fake_font_atlas, fake_font_glyph, fake_font_free, NULL, "fake",
   fake_font_metrics, false };
int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *path, unsigned size,
      enum font_atlas_format fmt)
{
   unsigned x, y;
   (void)path; (void)size; (void)fmt;
   memset(font_cov, 0, sizeof(font_cov));
   for (y = 0; y < 5; y++)
      for (x = 0; x < 3; x++)
         font_cov[y * 16 + x] = 255;
   font_atl.buffer  = font_cov;
   font_atl.width   = 16;
   font_atl.height  = 16;
   font_a.dims      = VIDEO_SCALE_PACK(3, 5);
   font_a.advance_x = 4;
   *drv             = &fake_font;
   *handle          = &font_atl;
   return 1;
}

/* ---- GEM buffers: memory behind each handle -------------------------- */

#define MAX_BO 64
static struct exynos_bo *bos[MAX_BO];
static uint8_t *bo_mem[MAX_BO];
static unsigned bos_alive;
static struct exynos_device fake_dev;

struct exynos_device *exynos_device_create(int fd)
{ fake_dev.fd = fd; return &fake_dev; }
void exynos_device_destroy(struct exynos_device *dev) { (void)dev; }
struct exynos_bo *exynos_bo_create(struct exynos_device *dev, size_t size,
      uint32_t flags)
{
   unsigned h;
   struct exynos_bo *bo;
   (void)flags;
   for (h = 1; h < MAX_BO && bos[h]; h++);
   if (h == MAX_BO || !(bo = (struct exynos_bo*)calloc(1, sizeof(*bo))))
      return NULL;
   bo->dev    = dev;
   bo->handle = h;
   bo->size   = size;
   bo_mem[h]  = (uint8_t*)calloc(1, size);
   bos[h]     = bo;
   bos_alive++;
   return bo;
}
void *exynos_bo_map(struct exynos_bo *bo)
{
   bo->vaddr = bo_mem[bo->handle];
   return bo->vaddr;
}
void exynos_bo_destroy(struct exynos_bo *bo)
{
   if (!bo)
      return;
   free(bo_mem[bo->handle]);
   bo_mem[bo->handle] = NULL;
   bos[bo->handle]    = NULL;
   bos_alive--;
   free(bo);
}

/* ---- the G2D ---------------------------------------------------------- */

struct g2d_context { int fd; };
static struct g2d_context g2d_ctx;
static unsigned g2d_execs, blends, fills_font;
/* The last fill and blend of the font image, x y w h */
static unsigned fill_rect[4], blend_src[4], blend_dst[4];

struct g2d_context *g2d_init(int fd) { g2d_ctx.fd = fd; return &g2d_ctx; }
void g2d_fini(struct g2d_context *ctx) { (void)ctx; }
int g2d_exec(struct g2d_context *ctx) { (void)ctx; g2d_execs++; return 0; }

static unsigned bpp_of(const struct g2d_image *img)
{
   switch (img->color_mode & G2D_COLOR_FMT_MASK)
   {
      case G2D_COLOR_FMT_XRGB8888:
      case G2D_COLOR_FMT_ARGB8888:
         return 4;
      default:
         return 2;
   }
}

/* The pixel at (x, y) as A, R, G, B in 0..255 */
static void read_px(const struct g2d_image *img, unsigned x, unsigned y,
      unsigned *a, unsigned *r, unsigned *g, unsigned *b)
{
   const uint8_t *row = bo_mem[img->bo[0]] + (size_t)y * img->stride;
   unsigned mode      = img->color_mode & G2D_COLOR_FMT_MASK;
   unsigned order     = img->color_mode & G2D_ORDER_MASK;
   if (bpp_of(img) == 4)
   {
      uint32_t p = ((const uint32_t*)row)[x];
      if (order == G2D_ORDER_RGBAX)
      { *r = p >> 24; *g = (p >> 16) & 0xff; *b = (p >> 8) & 0xff; *a = p & 0xff; }
      else
      { *a = mode == G2D_COLOR_FMT_ARGB8888 ? p >> 24 : 255;
        *r = (p >> 16) & 0xff; *g = (p >> 8) & 0xff; *b = p & 0xff; }
   }
   else
   {
      uint32_t p = ((const uint16_t*)row)[x];
      if (mode == G2D_COLOR_FMT_RGB565)
      {
         *a = 255;
         *r = ((p >> 11) & 31) * 255 / 31;
         *g = ((p >> 5) & 63) * 255 / 63;
         *b = (p & 31) * 255 / 31;
      }
      else if (order == G2D_ORDER_RGBAX)
      { *r = (p >> 12) * 17; *g = ((p >> 8) & 15) * 17;
        *b = ((p >> 4) & 15) * 17; *a = (p & 15) * 17; }
      else
      { *a = (p >> 12) * 17; *r = ((p >> 8) & 15) * 17;
        *g = ((p >> 4) & 15) * 17; *b = (p & 15) * 17; }
   }
}

/* Pages are XRGB8888 */
static void write_px(struct g2d_image *img, unsigned x, unsigned y,
      unsigned r, unsigned g, unsigned b)
{
   uint8_t *row = bo_mem[img->bo[0]] + (size_t)y * img->stride;
   ((uint32_t*)row)[x] = (r << 16) | (g << 8) | b;
}

static int is_font(const struct g2d_image *img)
{
   return (img->color_mode & G2D_COLOR_FMT_MASK) == G2D_COLOR_FMT_ARGB4444
      && (img->color_mode & G2D_ORDER_MASK) == G2D_ORDER_AXRGB;
}

int g2d_solid_fill(struct g2d_context *ctx, struct g2d_image *img,
      unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
   unsigned i, j;
   (void)ctx;
   if (!bo_mem[img->bo[0]] || x + w > img->width || y + h > img->height)
      return -1;
   if (is_font(img))
   {
      fills_font++;
      fill_rect[0] = x; fill_rect[1] = y; fill_rect[2] = w; fill_rect[3] = h;
   }
   for (j = y; j < y + h; j++)
   {
      uint8_t *row = bo_mem[img->bo[0]] + (size_t)j * img->stride;
      for (i = x; i < x + w; i++)
      {
         if (bpp_of(img) == 4)
            ((uint32_t*)row)[i] = img->color;
         else
            ((uint16_t*)row)[i] = (uint16_t)img->color;
      }
   }
   return 0;
}

int g2d_copy_with_scale(struct g2d_context *ctx, struct g2d_image *src,
      struct g2d_image *dst, unsigned int sx, unsigned int sy,
      unsigned int sw, unsigned int sh, unsigned int dx, unsigned int dy,
      unsigned int dw, unsigned int dh, unsigned int negative)
{
   unsigned x, y;
   (void)ctx; (void)negative;
   if (     !bo_mem[src->bo[0]] || !bo_mem[dst->bo[0]] || !sw || !sh
         || dx + dw > dst->width || dy + dh > dst->height)
      return -1;
   for (y = 0; y < dh; y++)
      for (x = 0; x < dw; x++)
      {
         unsigned a, r, g, b;
         read_px(src, sx + x * sw / dw, sy + y * sh / dh, &a, &r, &g, &b);
         write_px(dst, dx + x, dy + y, r, g, b);
      }
   return 0;
}

int g2d_scale_and_blend(struct g2d_context *ctx, struct g2d_image *src,
      struct g2d_image *dst, unsigned int sx, unsigned int sy,
      unsigned int sw, unsigned int sh, unsigned int dx, unsigned int dy,
      unsigned int dw, unsigned int dh, enum e_g2d_op op)
{
   unsigned x, y;
   (void)ctx; (void)op;
   if (     !bo_mem[src->bo[0]] || !bo_mem[dst->bo[0]] || !sw || !sh
         || sx + sw > src->width || sy + sh > src->height
         || dx + dw > dst->width || dy + dh > dst->height)
      return -1;
   blends++;
   if (is_font(src))
   {
      blend_src[0] = sx; blend_src[1] = sy; blend_src[2] = sw; blend_src[3] = sh;
      blend_dst[0] = dx; blend_dst[1] = dy; blend_dst[2] = dw; blend_dst[3] = dh;
   }
   for (y = 0; y < dh; y++)
      for (x = 0; x < dw; x++)
      {
         unsigned a, r, g, b, da, dr, dg, db;
         read_px(src, sx + x * sw / dw, sy + y * sh / dh, &a, &r, &g, &b);
         read_px(dst, dx + x, dy + y, &da, &dr, &dg, &db);
         write_px(dst, dx + x, dy + y, (r * a + dr * (255 - a)) / 255,
               (g * a + dg * (255 - a)) / 255, (b * a + db * (255 - a)) / 255);
      }
   return 0;
}

/* ---- the KMS side ------------------------------------------------------ */

#define CARD_FD  7
#define CRTC_ID  41
#define CONN_ID  90
#define ENC_ID   61
#define MAX_FB   64
static drmModeModeInfo modes[3];
static int      fds_open, connected = 1;
static uint32_t crtc_fb = 9;
static drmModeModeInfo crtc_mode;
static unsigned fb_handle[MAX_FB], fbs_alive, rmfb_wrong_fd;
static uint32_t next_fb = 20;

int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...)
{
   if (!strcmp(path, "/dev/dri/card0")) { fds_open++; return CARD_FD; }
   if (!strncmp(path, "/dev/dri/", 9))  return -1;
   return __real_open(path, flags, 0644);
}
int __real_close(int fd);
int __wrap_close(int fd)
{
   if (fd == CARD_FD) { fds_open--; return 0; }
   return __real_close(fd);
}

drmVersionPtr drmGetVersion(int fd)
{
   drmVersionPtr v = (drmVersionPtr)calloc(1, sizeof(*v));
   (void)fd;
   v->name = strdup("exynos");
   return v;
}
void drmFreeVersion(drmVersionPtr v) { if (v) { free(v->name); free(v); } }
int drmSetClientCap(int fd, uint64_t c, uint64_t v)
{ (void)fd; (void)c; (void)v; return 0; }
int drmIoctl(int fd, unsigned long r, void *a)
{ (void)fd; (void)r; (void)a; return -1; }

static uint32_t res_crtcs[1] = { CRTC_ID }, res_conns[1] = { CONN_ID },
                res_encs[1]  = { ENC_ID };
drmModeResPtr drmModeGetResources(int fd)
{
   drmModeResPtr r = (drmModeResPtr)calloc(1, sizeof(*r));
   (void)fd;
   r->count_crtcs = 1;      r->crtcs      = res_crtcs;
   r->count_connectors = 1; r->connectors = res_conns;
   r->count_encoders = 1;   r->encoders   = res_encs;
   return r;
}
void drmModeFreeResources(drmModeResPtr p) { free(p); }
drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t id)
{
   drmModeConnectorPtr c = (drmModeConnectorPtr)calloc(1, sizeof(*c));
   (void)fd;
   c->connector_id   = id;
   c->connection     = connected ? DRM_MODE_CONNECTED : DRM_MODE_DISCONNECTED;
   c->encoder_id     = ENC_ID;
   c->count_modes    = connected ? 3 : 0;
   c->modes          = (drmModeModeInfo*)malloc(sizeof(modes));
   memcpy(c->modes, modes, sizeof(modes));
   c->count_encoders = 1;
   c->encoders       = (uint32_t*)malloc(sizeof(uint32_t));
   c->encoders[0]    = ENC_ID;
   return c;
}
void drmModeFreeConnector(drmModeConnectorPtr p)
{ if (p) { free(p->modes); free(p->encoders); free(p); } }
drmModeEncoderPtr drmModeGetEncoder(int fd, uint32_t id)
{
   drmModeEncoderPtr e = (drmModeEncoderPtr)calloc(1, sizeof(*e));
   (void)fd;
   e->encoder_id     = id;
   e->crtc_id        = CRTC_ID;
   e->possible_crtcs = 1;
   return e;
}
void drmModeFreeEncoder(drmModeEncoderPtr p) { free(p); }
drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t id)
{
   drmModeCrtcPtr c = (drmModeCrtcPtr)calloc(1, sizeof(*c));
   (void)fd;
   c->crtc_id    = id;
   c->buffer_id  = crtc_fb;
   c->mode_valid = 1;
   c->mode       = modes[0];
   return c;
}
void drmModeFreeCrtc(drmModeCrtcPtr p) { free(p); }
int drmModeSetCrtc(int fd, uint32_t c, uint32_t b, uint32_t x, uint32_t y,
      uint32_t *conn, int n, drmModeModeInfoPtr m)
{
   (void)fd; (void)c; (void)x; (void)y; (void)conn; (void)n;
   crtc_fb = b;
   if (m)
      crtc_mode = *m;
   return 0;
}
int drmModeAddFB2(int fd, uint32_t w, uint32_t h, uint32_t fmt,
      const uint32_t handles[4], const uint32_t pitches[4],
      const uint32_t offsets[4], uint32_t *id, uint32_t flags)
{
   (void)fd; (void)w; (void)h; (void)fmt; (void)pitches; (void)offsets;
   (void)flags;
   if (next_fb >= MAX_FB || !bos[handles[0]])
      return -1;
   *id = next_fb++;
   fb_handle[*id] = handles[0];
   fbs_alive++;
   return 0;
}
int drmModeRmFB(int fd, uint32_t id)
{
   if (fd != CARD_FD)
   {
      rmfb_wrong_fd++;
      return -1;
   }
   if (id >= MAX_FB || !fb_handle[id])
      return -1;
   fb_handle[id] = 0;
   fbs_alive--;
   return 0;
}

/* A flip goes out at the next vblank(); a wait sees the vblank come */
static void *flip_data;
static uint32_t flip_fb, shown_fb;
static int flip_pending, flip_done;
static unsigned flips, blocking_waits;
static void vblank(void) { if (flip_pending) flip_done = 1; }
int drmModePageFlip(int fd, uint32_t crtc, uint32_t fb, uint32_t flags,
      void *data)
{
   (void)fd; (void)crtc; (void)flags;
   if (flip_pending)
      return -16;
   flip_pending = 1;
   flip_done    = 0;
   flip_fb      = fb;
   flip_data    = data;
   flips++;
   return 0;
}
int __wrap_poll(struct pollfd *fds, nfds_t n, int timeout)
{
   if (n != 1 || fds[0].fd != CARD_FD)
      return 0;
   if (timeout)
   {
      blocking_waits++;
      if (flip_pending)
         flip_done = 1;
   }
   fds[0].revents = (flip_pending && flip_done) ? POLLIN : 0;
   return fds[0].revents ? 1 : 0;
}
int drmHandleEvent(int fd, drmEventContextPtr ev)
{
   if (!flip_pending || !flip_done)
      return 0;
   flip_pending = flip_done = 0;
   shown_fb     = flip_fb;
   if (ev && ev->page_flip_handler)
      ev->page_flip_handler(fd, 0, 0, 0, flip_data);
   return 0;
}

static void set_mode(drmModeModeInfo *m, unsigned w, unsigned h, unsigned hz)
{
   m->hdisplay = (uint16_t)w;
   m->vdisplay = (uint16_t)h;
   m->htotal   = (uint16_t)w;
   m->vtotal   = (uint16_t)h;
   m->clock    = w * h * hz / 1000;
   m->vrefresh = hz;
}

/* ---- the test --------------------------------------------------------- */

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("   FAIL "); \
   printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t frame_px[320 * 240];

static uint32_t pattern_px(unsigned x, unsigned y)
{ return ((x & 0xff) << 16) | ((y & 0xff) << 8) | 0x40; }

/* The screen: the page last flipped to */
static const uint32_t *screen(unsigned *pitch)
{
   unsigned h = shown_fb < MAX_FB ? fb_handle[shown_fb] : 0;
   *pitch     = 1280;
   return h && bo_mem[h] ? (const uint32_t*)bo_mem[h] : NULL;
}

static void frame(struct exynos_video *v, const void *f, unsigned w,
      unsigned h, unsigned pitch, const char *msg, video_frame_info_t *vi)
{
   exynos_frame(v, f, VIDEO_SCALE_PACK(w, h), 0, pitch, msg, vi);
   vblank();
   drm_wait_flip(0);
}

int main(void)
{
   static video_info_t info;
   static video_frame_info_t vi;
   struct exynos_video *v;
   struct exynos_data *pd;
   unsigned i, x, y, before;

   setvbuf(stdout, NULL, _IONBF, 0);
   set_mode(&modes[0], 1280, 720, 60);
   set_mode(&modes[1], 1920, 1080, 60);
   set_mode(&modes[2], 640, 480, 60);
   settings.floats.video_refresh_rate = 60.0f;
   info.rgb32       = true;
   info.vsync       = true;
   info.font_enable = true;
   info.font_size   = 32.0f;

   v = (struct exynos_video*)exynos_init(&info);
   CHECK(v && fds_open == 1 && fbs_alive == 3 && crtc_mode.hdisplay == 1280,
         "init: the card, three pages, the native mode");
   CHECK(display_type == RARCH_DISPLAY_KMS
         && output_dims == VIDEO_SCALE_PACK(1280, 720),
         "init: the KMS display server is given the mode");
   if (!v)
      return 1;
   pd = v->data;

   /* ---- aspect: the frame at the aspect it is shown at ---- */
   before = failures;
   for (y = 0; y < 224; y++)
      for (x = 0; x < 256; x++)
         frame_px[y * 256 + x] = pattern_px(x, y);
   frame(v, frame_px, 256, 224, 256 * 4, NULL, &vi);
   CHECK(pd->blit_params[2] == 960 && pd->blit_params[3] == 720
         && pd->blit_params[0] == 160,
         "a 256x224 frame at 4:3 fills 960x720 at 160, not %ux%u at %u",
         pd->blit_params[2], pd->blit_params[3], pd->blit_params[0]);
   {
      unsigned pitch;
      const uint32_t *s = screen(&pitch);
      CHECK(s && (s[360 * pitch + 640] & 0xffffff)
            == pattern_px(128, 112),
            "the frame reaches the screen through the G2D");
      CHECK(s && !(s[10 * pitch + 10] & 0xffffff), "black beside it");
   }
   aspectratio_lut[1].value = 16.0f / 9.0f;
   exynos_set_aspect_ratio(v, 1);
   frame(v, frame_px, 256, 224, 256 * 4, NULL, &vi);
   CHECK(pd->blit_params[2] == 1280 && pd->blit_params[3] == 720,
         "a 16:9 aspect chosen fills the screen (%ux%u)",
         pd->blit_params[2], pd->blit_params[3]);
   drv_aspect = 4.0f / 3.0f;
   exynos_apply_state_changes(v);
   frame(v, frame_px, 256, 224, 256 * 4, NULL, &vi);
   CHECK(pd->blit_params[2] == 960, "the CRT switch's aspect taken up");
   if (failures == before)
      printf("   ok   the frame shown at its aspect ratio\n");

   /* ---- the core renders into the buffer the G2D reads ---- */
   before = failures;
   {
      struct retro_framebuffer fb;
      unsigned m0, pitch;
      const uint32_t *s;
      memset(&fb, 0, sizeof(fb));
      fb.width = 256; fb.height = 224;
      fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
      CHECK(exynos_get_current_software_framebuffer(v, &fb)
            && fb.data == pd->buf[EXYNOS_BUFFER_MAIN]->vaddr
            && fb.pitch == 1024, "the frame's GEM buffer is lent");
      if (!fb.data)
      {
         /* Not lent: the core renders into a buffer of its own */
         static uint32_t own[256 * 224];
         fb.data  = own;
         fb.pitch = 1024;
      }
      for (y = 0; y < 224; y++)
         for (x = 0; x < 256; x++)
            ((uint32_t*)fb.data)[y * 256 + x] = pattern_px(255 - x, y);
      m0 = memcpys;
      frame(v, fb.data, 256, 224, (unsigned)fb.pitch, NULL, &vi);
      s = screen(&pitch);
      CHECK(memcpys == m0, "a frame rendered into it is not copied");
      CHECK(s && (s[360 * pitch + 640] & 0xffffff) == pattern_px(255 - 128, 112),
            "and reaches the screen");
      fb.access_flags = RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ;
      CHECK(!exynos_get_current_software_framebuffer(v, &fb),
            "not lent to a core that reads its frame back");
      m0 = memcpys;
      frame(v, frame_px, 256, 224, 256 * 4, NULL, &vi);
      CHECK(memcpys == m0 + 1, "a frame of the core's own is copied");
      /* A bigger loan: the cached frame in the old buffer retired */
      fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
      fb.width = 1024; fb.height = 1024;
      m0 = retires;
      CHECK(exynos_get_current_software_framebuffer(v, &fb)
            && retires == m0 + 1, "the buffer grown under a loan retires the cached frame");
   }
   if (failures == before)
      printf("   ok   the core renders into the buffer the G2D reads\n");

   /* ---- vsync off: no waits, a frame before the flip not shown ---- */
   before = failures;
   {
      unsigned w0, f0;
      exynos_set_nonblock_state(v, true, false, 0);
      exynos_frame(v, frame_px, VIDEO_SCALE_PACK(256, 224), 0, 1024, NULL, &vi);
      w0 = blocking_waits;
      f0 = flips;
      for (i = 0; i < 5; i++)
         exynos_frame(v, frame_px, VIDEO_SCALE_PACK(256, 224), 0, 1024, NULL, &vi);
      CHECK(blocking_waits == w0 && flips == f0,
            "without vsync, frames before the flip is done neither wait nor"
            " flip (%u waits, %u flips)", blocking_waits - w0, flips - f0);
      vblank();
      exynos_frame(v, frame_px, VIDEO_SCALE_PACK(256, 224), 0, 1024, NULL, &vi);
      CHECK(flips == f0 + 1 && blocking_waits == w0,
            "the next after it flips, still without waiting");
      exynos_set_nonblock_state(v, false, false, 0);
      w0 = blocking_waits;
      exynos_frame(v, frame_px, VIDEO_SCALE_PACK(256, 224), 0, 1024, NULL, &vi);
      CHECK(blocking_waits > w0, "with vsync, the flip is waited for");
      vblank();
      drm_wait_flip(0);
   }
   if (failures == before)
      printf("   ok   vsync off neither waits nor queues flips\n");

   /* ---- messages: the area they cover, from the frame's info ---- */
   before = failures;
   {
      unsigned f0 = fills_font, b0 = blends, pitch, fw;
      const uint32_t *s;
      struct g2d_image *fi = pd->src[EXYNOS_IMAGE_FONT];
      settings.floats.video_msg_pos_x = 0.5f;
      vi.font_msg_pos_x   = 0.1f;
      vi.font_msg_pos_y   = 0.9f;
      vi.font_msg_color_r = 1.0f;
      fw = fi->width;
      frame(v, frame_px, 256, 224, 1024, "A", &vi);
      CHECK(fills_font == f0, "nothing to clear before the first message");
      CHECK(blends == b0 + 1 && blend_src[2] == 3 && blend_src[3] == 5
            && blend_src[0] == (unsigned)(0.1f * fw)
            && blend_src[1] == (unsigned)(0.1f * fi->height),
            "only the glyph's area is blended, where the frame's info puts it"
            " (%u,%u %ux%u)", blend_src[0], blend_src[1], blend_src[2],
            blend_src[3]);
      s = screen(&pitch);
      x = blend_dst[0] + blend_dst[2] / 2;
      y = blend_dst[1] + blend_dst[3] / 2;
      CHECK(s && ((s[y * pitch + x] >> 16) & 0xff) > 0xc0
            && !(s[y * pitch + x] & 0xff00),
            "in the message colour (%06x)", s ? s[y * pitch + x] & 0xffffff : 0);
      frame(v, frame_px, 256, 224, 1024, NULL, &vi);
      CHECK(fills_font == f0 + 1 && fill_rect[2] == 3 && fill_rect[3] == 5
            && blends == b0 + 1,
            "the message gone: only its area cleared, nothing blended"
            " (%ux%u)", fill_rect[2], fill_rect[3]);
      frame(v, frame_px, 256, 224, 1024, NULL, &vi);
      CHECK(fills_font == f0 + 1, "and nothing cleared after that");
      /* The statistics, drawn by the driver itself */
      vi.statistics_show   = true;
      vi.stat_text         = "AA";
      vi.stat_text_len     = 2;
      vi.osd_stat_params.x = 0.0f;
      vi.osd_stat_params.y = 0.5f;
      b0 = blends;
      frame(v, frame_px, 256, 224, 1024, NULL, &vi);
      CHECK(blends == b0 + 1 && blend_src[2] == 7 && blend_src[0] == 0,
            "the statistics are drawn (%u wide at %u)", blend_src[2],
            blend_src[0]);
      vi.statistics_show = false;
      frame(v, frame_px, 256, 224, 1024, NULL, &vi);
   }
   if (failures == before)
      printf("   ok   messages and statistics: only the area they cover\n");

   /* ---- the menu's alpha, scaled as it is copied ---- */
   before = failures;
   {
      static uint16_t rgui[64 * 32];
      const uint16_t *aux;
      for (i = 0; i < 64 * 32; i++)
         rgui[i] = 0xf00f;
      exynos_set_texture_frame(v, rgui, false, VIDEO_SCALE_PACK(64, 32), 0.5f);
      aux = (const uint16_t*)pd->buf[EXYNOS_BUFFER_AUX]->vaddr;
      CHECK((aux[0] & 0xf) == 7 && (aux[0] & 0xfff0) == 0xf000,
            "half the menu's alpha, its colour kept (%04x)", aux[0]);
      exynos_set_texture_frame(v, rgui, false, VIDEO_SCALE_PACK(64, 32), 1.0f);
      CHECK(aux[0] == 0xf00f, "opaque at full alpha");
   }
   if (failures == before)
      printf("   ok   the menu's alpha is applied\n");

   /* ---- screenshots read the screen ---- */
   before = failures;
   {
      struct video_viewport vp;
      uint8_t *rgb;
      unsigned pitch, bad = 0, w, h;
      const uint32_t *s;
      frame(v, frame_px, 256, 224, 256 * 4, NULL, &vi);
      memset(&vp, 0, sizeof(vp));
      exynos_viewport_info(v, &vp);
      w   = VIDEO_SCALE_W(vp.dims);
      h   = VIDEO_SCALE_H(vp.dims);
      CHECK(vp.pos == VIDEO_POS_PACK(160, 0) && w == 960 && h == 720,
            "the viewport is where the frame is on the screen");
      rgb = (uint8_t*)malloc((size_t)w * h * 3);
      s   = screen(&pitch);
      CHECK(rgb && s && exynos_read_viewport(v, rgb, false),
            "the screen is read back");
      for (y = 0; rgb && s && y < h; y++)
         for (x = 0; x < w; x++)
         {
            uint32_t p = s[(y) * pitch + 160 + x];
            const uint8_t *q = rgb + ((size_t)(h - 1 - y) * w + x) * 3;
            if (q[0] != (p & 0xff) || q[1] != ((p >> 8) & 0xff)
                  || q[2] != ((p >> 16) & 0xff))
               bad++;
         }
      CHECK(!bad, "as BGR24, bottom row first (%u differ)", bad);
      free(rgb);
   }
   if (failures == before)
      printf("   ok   screenshots read the frame on the screen\n");

   /* ---- a mode switch, and a CRT modeline ---- */
   before = failures;
   {
      videocrt_switch_t *sw = &video_st.crt_switch_st;
      exynos_set_video_mode(v, VIDEO_SCALE_PACK(640, 480), true);
      CHECK(crtc_mode.hdisplay == 640 && pd->dst->width == 640
            && fbs_alive == 3 && output_dims == VIDEO_SCALE_PACK(640, 480),
            "640x480: new pages at its size, the old ones gone (%u fbs)",
            fbs_alive);
      frame(v, frame_px, 256, 224, 256 * 4, NULL, &vi);
      CHECK(pd->blit_params[3] == 480 && pd->blit_params[2] == 640,
            "the frame placed on the new mode");
      memset(sw, 0, sizeof(*sw));
      sw->clock = 6700; sw->hdisplay = 320; sw->hsync_start = 336;
      sw->hsync_end = 367; sw->htotal = 426; sw->vdisplay = 240;
      sw->vsync_start = 244; sw->vsync_end = 247; sw->vtotal = 262;
      sw->vrefresh = 60;
      exynos_set_video_mode(v, VIDEO_SCALE_PACK(320, 240), true);
      CHECK(crtc_mode.hdisplay == 320 && crtc_mode.htotal == 426
            && !sw->vdisplay && fbs_alive == 3, "a CRT modeline set");
      frame(v, frame_px, 256, 224, 256 * 4, NULL, &vi);
   }
   if (failures == before)
      printf("   ok   mode switches and CRT modelines\n");

   /* ---- the free leaves nothing ---- */
   before = failures;
   exynos_free(v);
   CHECK(!fbs_alive && !rmfb_wrong_fd && !bos_alive && !fds_open,
         "the free removes every framebuffer on the card, every buffer, the"
         " card (%u fbs, %u to the wrong fd, %u bos, %u open)", fbs_alive,
         rmfb_wrong_fd, bos_alive, fds_open);
   connected = 0;
   v = (struct exynos_video*)exynos_init(&info);
   CHECK(!v && !fds_open, "no display: init fails, the card closed (%u open)",
         fds_open);
   connected = 1;
   if (failures == before)
      printf("   ok   the free and a failed init leave nothing\n");

   if (failures)
   {
      printf("FAIL exynos_g2d_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS exynos_g2d_test\n");
   return 0;
}
