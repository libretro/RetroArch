/* gfx/drivers/exynos_gfx.c against a fake kernel and a fake G2D.
 *
 * The G2D here does what the block does - fills, scaled copies, scaled
 * blends - into the GEM buffers it is handed, so what a page holds when
 * it is flipped to is what the screen shows. The KMS side counts the
 * framebuffers, the flips and the waits for them, and the cards opened
 * and closed. Userptr memory is mapped as the kernel maps it: once per
 * address and size, and reused unsynced, so a frame that came through a
 * reused mapping shows what the memory held when it was made. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/stat.h>

/* The driver's copies, counted: its only memcpy calls are the frame's
 * and the overlays' */
static void *count_memcpy(void *dst, const void *src, size_t n);
#define memcpy(dst, src, n) count_memcpy(dst, src, n)
#include "../../../gfx/drivers/exynos_gfx.c"
#undef memcpy
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
static void *count_memcpy(void *dst, const void *src, size_t n)
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
static unsigned g2d_execs, blends, fills_font, overlay_blends;

/* The kernel's userptr mappings: one per address, the memory as it was
 * when the mapping was made - and, as the kernel syncs nothing when it
 * reuses one, as the G2D still reads it after the CPU wrote it again */
#define MAX_UP 16
static struct { uintptr_t addr; unsigned long size; uint8_t *seen; } ups[MAX_UP];
static unsigned up_new, up_reused, up_refused;
/* Memory that cannot be pinned for writing (read-only), and the ends of
 * the memory the test maps: pinning past them fails */
static uintptr_t ro_addr, mapped_lo[4], mapped_hi[4];
static unsigned long page_sz;

static const uint8_t *userptr_map(const struct g2d_image *img,
      unsigned long need)
{
   unsigned i, j;
   uintptr_t a = (uintptr_t)img->user_ptr[0].userptr;
   unsigned long size = img->user_ptr[0].size;
   uintptr_t end = (a + size + page_sz - 1) & ~(uintptr_t)(page_sz - 1);
   int ok = 0;
   if (!size || need > size || a == ro_addr)
   {
      up_refused++;
      return NULL;
   }
   for (j = 0; j < 4; j++)
      if (a >= mapped_lo[j] && end <= mapped_hi[j])
         ok = 1;
   if (!ok)
   {
      up_refused++;
      return NULL;
   }
   for (i = 0; i < MAX_UP; i++)
      if (ups[i].seen && ups[i].addr == a)
      {
         if (ups[i].size == size)
         {
            up_reused++;
            return ups[i].seen;
         }
         free(ups[i].seen);
         ups[i].seen = NULL;
      }
   for (i = 0; i < MAX_UP && ups[i].seen; i++);
   if (i == MAX_UP)
      return NULL;
   ups[i].addr = a;
   ups[i].size = size;
   ups[i].seen = (uint8_t*)malloc(size);
   memcpy(ups[i].seen, (const void*)a, size);
   up_new++;
   return ups[i].seen;
}

static const uint8_t *img_mem(const struct g2d_image *img)
{
   return img->buf_type == G2D_IMGBUF_USERPTR
      ? (const uint8_t*)img->mapped_ptr[1] : bo_mem[img->bo[0]];
}
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
   const uint8_t *row = img_mem(img) + (size_t)y * img->stride;
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
   if (src->buf_type == G2D_IMGBUF_USERPTR)
   {
      /* The last byte read must lie inside the size given */
      unsigned long need = (unsigned long)(sy + sh - 1) * src->stride
         + (sx + sw) * bpp_of(src);
      const uint8_t *m   = userptr_map(src, need);
      if (!m)
         return -14;
      src->mapped_ptr[1] = (void*)m;
   }
   else if (!bo_mem[src->bo[0]])
      return -1;
   if (     !bo_mem[dst->bo[0]] || !sw || !sh
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
   (void)ctx;
   if (     !bo_mem[src->bo[0]] || !bo_mem[dst->bo[0]] || !sw || !sh
         || sx + sw > src->width || sy + sh > src->height
         || dx + dw > dst->width || dy + dh > dst->height)
      return -1;
   blends++;
   if (op == G2D_OP_OVER)
      overlay_blends++;
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
         if (op == G2D_OP_OVER) /* premultiplied */
            write_px(dst, dx + x, dy + y, r + dr * (255 - a) / 255,
                  g + dg * (255 - a) / 255, b + db * (255 - a) / 255);
         else
            write_px(dst, dx + x, dy + y, (r * a + dr * (255 - a)) / 255,
                  (g * a + dg * (255 - a)) / 255,
                  (b * a + db * (255 - a)) / 255);
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
   page_sz               = (unsigned long)sysconf(_SC_PAGESIZE);
   /* No G2D device with an IOMMU: frames are copied */
   exynos_g2d_driver_dir = "/nonexistent/exynos-drm-g2d";
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

   /* ---- a G2D behind an IOMMU: frames read where the core wrote them ---- */
   {
      static char dir[] = "/tmp/exynos_g2d_XXXXXX";
      char path[128];
      FILE *f;
      if (mkdtemp(dir))
      {
         snprintf(path, sizeof(path), "%s/bind", dir);
         if ((f = fopen(path, "w")))
            fclose(f);
         snprintf(path, sizeof(path), "%s/10800000.g2d", dir);
         mkdir(path, 0700);
         snprintf(path, sizeof(path), "%s/10800000.g2d/iommu_group", dir);
         if ((f = fopen(path, "w")))
            fclose(f);
         exynos_g2d_driver_dir = dir;
      }
   }
   v = (struct exynos_video*)exynos_init(&info);
   CHECK(v && v->data->userptr, "a G2D in an IOMMU group reads userptr memory");
   if (!v)
      return 1;
   pd = v->data;

   before = failures;
   {
      /* Three frames in memory of the core's: one ending mid-page, one
       * ending on a page boundary, one the kernel will not pin */
      const size_t fsz = 256 * 224 * 4;
      uint8_t *blk[3];
      uint32_t *fr[3];
      unsigned k, m0, n0, pitch;
      const uint32_t *s;
      for (k = 0; k < 3; k++)
      {
         size_t len = (fsz / page_sz + 2) * page_sz;
         if (posix_memalign((void**)&blk[k], page_sz, len))
            return 1;
         mapped_lo[k] = (uintptr_t)blk[k];
         mapped_hi[k] = (uintptr_t)blk[k] + (k == 1 ? fsz : len);
      }
      fr[0]   = (uint32_t*)(blk[0] + 64);
      fr[1]   = (uint32_t*)blk[1];
      fr[2]   = (uint32_t*)(blk[2] + 64);
      ro_addr = (uintptr_t)fr[2];

      m0 = memcpys;
      n0 = up_new;
      for (k = 0; k < 4; k++)
      {
         /* The core draws a new frame into the same memory */
         for (y = 0; y < 224; y++)
            for (x = 0; x < 256; x++)
               fr[0][y * 256 + x] = pattern_px(x + k * 7, y);
         frame(v, fr[0], 256, 224, 1024, NULL, &vi);
         s = screen(&pitch);
         CHECK(s && (s[360 * pitch + 640] & 0xffffff)
               == pattern_px(128 + k * 7, 112),
               "frame %u of the same memory on the screen, not the one"
               " the kernel mapped before (%06x)", k,
               s ? s[360 * pitch + 640] & 0xffffff : 0);
      }
      CHECK(memcpys == m0 && up_new == n0 + 4 && !up_reused,
            "read where the core wrote it, mapped afresh each frame"
            " (%u copies, %u mapped, %u reused)", memcpys - m0,
            up_new - n0, up_reused);

      /* No room after it for a second size: the frame after is copied */
      m0 = memcpys;
      for (k = 0; k < 3; k++)
      {
         for (y = 0; y < 224; y++)
            for (x = 0; x < 256; x++)
               fr[1][y * 256 + x] = pattern_px(x, y + k * 5);
         frame(v, fr[1], 256, 224, 1024, NULL, &vi);
         s = screen(&pitch);
         CHECK(s && (s[360 * pitch + 640] & 0xffffff)
               == pattern_px(128, 112 + k * 5),
               "a frame ending on a page: frame %u shown", k);
      }
      CHECK(memcpys == m0 + 2 && !up_reused,
            "and the ones after the first copied, never a stale mapping"
            " (%u copies)", memcpys - m0);

      /* Memory the kernel refuses: copied, and not offered again */
      for (y = 0; y < 224; y++)
         for (x = 0; x < 256; x++)
            fr[2][y * 256 + x] = pattern_px(x ^ 3, y);
      m0 = memcpys;
      n0 = up_refused;
      frame(v, fr[2], 256, 224, 1024, NULL, &vi);
      frame(v, fr[2], 256, 224, 1024, NULL, &vi);
      s = screen(&pitch);
      CHECK(s && (s[360 * pitch + 640] & 0xffffff) == pattern_px(128 ^ 3, 112)
            && memcpys == m0 + 2 && up_refused == n0 + 1,
            "memory the kernel will not map copied, and only asked once"
            " (%u copies, %u refused)", memcpys - m0, up_refused - n0);
      for (k = 0; k < 3; k++)
         free(blk[k]);
   }
   if (failures == before)
      printf("   ok   frames read from the core's memory, never stale\n");

   /* ---- the input overlay, blended by the G2D ---- */
   before = failures;
   {
      static uint32_t red[16], white[16], grey[256 * 224];
      struct texture_image im[2];
      const video_overlay_interface_t *ov = NULL;
      unsigned k, b0, pitch;
      const uint32_t *s;

      for (i = 0; i < 16; i++)
      {
         red[i]   = 0xffff0000u; /* opaque */
         white[i] = 0x80ffffffu; /* half */
      }
      for (i = 0; i < 256 * 224; i++)
         grey[i] = 0x404040u;
      memset(im, 0, sizeof(im));
      im[0].pixels = red;   im[0].width = 4; im[0].height = 4;
      im[1].pixels = white; im[1].width = 4; im[1].height = 4;
      exynos_get_overlay_interface(v, &ov);
      CHECK(ov && ov->load(v, im, 2), "two overlay images loaded");
      ov->vertex_geom(v, 0, 0.0f, 0.0f, 0.25f, 0.25f);
      ov->vertex_geom(v, 1, 0.5f, 0.5f, 0.25f, 0.25f);
      ov->set_alpha(v, 7, 0.5f);
      ov->vertex_geom(v, 7, 0.0f, 0.0f, 1.0f, 1.0f);
      ov->tex_geom(v, 7, 0.0f, 0.0f, 1.0f, 1.0f);
      ov->enable(v, true);
      b0 = overlay_blends;
      frame(v, grey, 256, 224, 1024, NULL, &vi);
      s = screen(&pitch);
      /* The frame is 960x720 at 160: a quarter of it each */
      CHECK(overlay_blends == b0 + 2, "each image blended by the G2D (%u)",
            overlay_blends - b0);
      CHECK(s && (s[90 * pitch + 280] & 0xffffff) == 0xff0000,
            "an opaque image over the frame where it is placed (%06x)",
            s ? s[90 * pitch + 280] & 0xffffff : 0);
      CHECK(s && (s[450 * pitch + 760] & 0xffffff) == 0x9f9f9f,
            "a half-transparent one blended over it (%06x)",
            s ? s[450 * pitch + 760] & 0xffffff : 0);
      CHECK(s && (s[300 * pitch + 500] & 0xffffff) == 0x404040,
            "the frame elsewhere");
      ov->set_alpha(v, 0, 0.5f);
      frame(v, grey, 256, 224, 1024, NULL, &vi);
      s = screen(&pitch);
      CHECK(s && (s[90 * pitch + 280] & 0xffffff) == 0x9f2020,
            "an image at half alpha (%06x)",
            s ? s[90 * pitch + 280] & 0xffffff : 0);

      /* On the screen, partly where no frame is: drawn once a frame there,
       * not over what it left on that page before */
      ov->full_screen(v, true);
      ov->vertex_geom(v, 0, 0.5f, 0.5f, 0.1f, 0.1f);
      ov->vertex_geom(v, 1, 0.0f, 0.0f, 0.1f, 0.1f);
      for (k = 0; k < 6; k++)
      {
         frame(v, grey, 256, 224, 1024, NULL, &vi);
         s = screen(&pitch);
         CHECK(s && (s[10 * pitch + 10] & 0xffffff) == 0x808080,
               "beside the frame, frame %u: half white over black (%06x)", k,
               s ? s[10 * pitch + 10] & 0xffffff : 0);
      }
      ov->vertex_geom(v, 1, 0.5f, 0.0f, 0.1f, 0.1f);
      for (k = 0; k < 4; k++)
         frame(v, grey, 256, 224, 1024, NULL, &vi);
      s = screen(&pitch);
      CHECK(s && !(s[10 * pitch + 10] & 0xffffff),
            "moved: where it was is black again (%06x)",
            s ? s[10 * pitch + 10] & 0xffffff : 0);
      ov->enable(v, false);
      b0 = overlay_blends;
      for (k = 0; k < 4; k++)
         frame(v, grey, 256, 224, 1024, NULL, &vi);
      s = screen(&pitch);
      CHECK(overlay_blends == b0 && s
            && (s[400 * pitch + 680] & 0xffffff) == 0x404040
            && !(s[10 * pitch + 10] & 0xffffff),
            "disabled: nothing blended, nothing left");
      /* Mirrored: the quad's width negative, or its texels', not both */
      {
         static uint32_t rb[2] = { 0xffff0000u, 0xff0000ffu };
         struct texture_image one;
         memset(&one, 0, sizeof(one));
         one.pixels = rb; one.width = 2; one.height = 1;
         ov->enable(v, true);
         ov->full_screen(v, false);
         CHECK(ov->load(v, &one, 1), "a red and blue image");
         ov->vertex_geom(v, 0, 0.5f, 0.5f, -0.25f, 0.25f);
         frame(v, grey, 256, 224, 1024, NULL, &vi);
         s = screen(&pitch);
         CHECK(s && (s[450 * pitch + 420] & 0xffffff) == 0x0000ff
               && (s[450 * pitch + 620] & 0xffffff) == 0xff0000,
               "a quad drawn right to left shows the image mirrored"
               " (%06x %06x)", s ? s[450 * pitch + 420] & 0xffffff : 0,
               s ? s[450 * pitch + 620] & 0xffffff : 0);
         ov->vertex_geom(v, 0, 0.25f, 0.5f, 0.25f, 0.25f);
         ov->tex_geom(v, 0, 1.0f, 0.0f, -1.0f, 1.0f);
         frame(v, grey, 256, 224, 1024, NULL, &vi);
         s = screen(&pitch);
         CHECK(s && (s[450 * pitch + 420] & 0xffffff) == 0x0000ff
               && (s[450 * pitch + 620] & 0xffffff) == 0xff0000,
               "so do texels read right to left");
         ov->vertex_geom(v, 0, 0.5f, 0.5f, -0.25f, 0.25f);
         frame(v, grey, 256, 224, 1024, NULL, &vi);
         s = screen(&pitch);
         CHECK(s && (s[450 * pitch + 420] & 0xffffff) == 0xff0000
               && (s[450 * pitch + 620] & 0xffffff) == 0x0000ff,
               "and both: not mirrored");
         ov->vertex_geom(v, 0, 0.5f, 0.75f, 0.25f, -0.25f);
         ov->tex_geom(v, 0, 0.0f, 0.0f, 1.0f, 1.0f);
         frame(v, grey, 256, 224, 1024, NULL, &vi);
         s = screen(&pitch);
         CHECK(s && (s[450 * pitch + 660] & 0xffffff) == 0xff0000
               && (s[450 * pitch + 860] & 0xffffff) == 0x0000ff,
               "upside down, a one-row image as it was");
      }
      ov->enable(v, true);
      CHECK(ov->load(v, NULL, 0), "an empty page");
      ov->set_alpha(v, 0, 1.0f);
      ov->vertex_geom(v, 0, 0.0f, 0.0f, 1.0f, 1.0f);
      frame(v, grey, 256, 224, 1024, NULL, &vi);
   }
   if (failures == before)
      printf("   ok   the overlay: each image scaled and blended by the G2D\n");

   /* ---- a core's rotation, applied as the frame is copied ---- */
   before = failures;
   {
      static const struct { unsigned rot, w, h, x0, sx, sy; float a; } t[] = {
         /* quarter turns, the turned size, where it sits on the screen,
          * the source pixel at its centre, the aspect it is shown at */
         { 1, 224, 256, 370, 127, 112, 3.0f / 4.0f },
         { 2, 256, 224, 160, 127, 111, 4.0f / 3.0f },
         { 3, 224, 256, 370, 128, 111, 3.0f / 4.0f },
         { 0, 256, 224, 160, 128, 112, 4.0f / 3.0f }
      };
      unsigned k, pitch;
      const uint32_t *s;
      for (y = 0; y < 224; y++)
         for (x = 0; x < 256; x++)
            frame_px[y * 256 + x] = pattern_px(x, y);
      for (k = 0; k < 4; k++)
      {
         drv_aspect = t[k].a;
         exynos_apply_state_changes(v);
         exynos_set_rotation(v, t[k].rot);
         frame(v, frame_px, 256, 224, 1024, NULL, &vi);
         s = screen(&pitch);
         CHECK(pd->blit_params[4] == t[k].w && pd->blit_params[5] == t[k].h
               && pd->blit_params[0] == t[k].x0,
               "%u quarter turn(s): %ux%u at %u (%ux%u at %u)", t[k].rot,
               t[k].w, t[k].h, t[k].x0, pd->blit_params[4],
               pd->blit_params[5], pd->blit_params[0]);
         CHECK(s && (s[360 * pitch + 640] & 0xffffff)
               == pattern_px(t[k].sx, t[k].sy),
               "%u quarter turn(s): the frame turned on the screen (%06x)",
               t[k].rot, s ? s[360 * pitch + 640] & 0xffffff : 0);
      }
   }
   if (failures == before)
      printf("   ok   a core's rotation, turned as the frame is copied\n");

   before = failures;
   exynos_overlay_load(v, NULL, 0);
   exynos_free(v);
   CHECK(!fbs_alive && !rmfb_wrong_fd && !bos_alive && !fds_open,
         "the second free leaves nothing (%u fbs, %u bos, %u open)",
         fbs_alive, bos_alive, fds_open);
   for (i = 0; i < MAX_UP; i++)
      free(ups[i].seen);
   if (failures == before)
      printf("   ok   and its free, overlay buffers too\n");

   if (failures)
   {
      printf("FAIL exynos_g2d_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS exynos_g2d_test\n");
   return 0;
}
