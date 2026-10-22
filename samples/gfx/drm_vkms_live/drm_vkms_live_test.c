/* The DRM driver against a real kernel: vkms, the kernel's virtual
 * display, with its writeback connector, which hands back what the CRTC
 * composed. drm_surface_lifetime holds the driver to a fake kernel; this
 * holds it to the one it runs on - what the kernel accepts and what
 * reaches the screen.
 *
 * Every check reads the screen back through writeback and compares it
 * with what the frame should look like there: a frame as it is, turned
 * a quarter, a half and three quarters, from the threaded wrapper's
 * frame memory (where a frame starts in it, too, but vkms reads such a
 * framebuffer from the wrong place, so only that it is taken),
 * flipped without vsync, with the input overlay on its
 * own plane, at a CRT modeline; and the screenshot readback against
 * the screen. A card that is not vkms is never touched: the run skips
 * (77). run_vm.sh boots a kernel with vkms in qemu to run it. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <dirent.h>
#include <poll.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../../gfx/drivers/drm_gfx.c"
#include "../../../gfx/common/drm_common.c"

/* ---- frontend pieces the driver reaches ------------------------------ */

static int verbose;
static void say(const char *tag, const char *fmt, va_list ap)
{
   if (!verbose)
      return;
   fputs(tag, stdout);
   vprintf(fmt, ap);
}
void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); say("[INFO] ", fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); say("[WARN] ", fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); say("[ERROR] ", fmt, ap); va_end(ap); }
struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];
static float drv_aspect = 4.0f / 3.0f;
float video_driver_get_aspect_ratio(void) { return drv_aspect; }
static video_driver_state_t video_st;
video_driver_state_t *video_state_get_ptr(void) { return &video_st; }
void video_driver_cached_frame_retire(void) { }
void video_driver_set_output_dims(unsigned dims) { (void)dims; }
void video_driver_display_type_set(enum rarch_display_type t) { (void)t; }
static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }
int font_renderer_create_default(const font_renderer_driver_t **drv,
      void **handle, const char *path, unsigned size,
      enum font_atlas_format fmt)
{
   (void)drv; (void)handle; (void)path; (void)size; (void)fmt;
   return 0;
}

/* /dev/dri as it is */
struct string_list *dir_list_new(const char *dir, const char *ext,
      bool a, bool b, bool c, bool d)
{
   DIR *dp;
   struct dirent *e;
   struct string_list *l = (struct string_list*)calloc(1, sizeof(*l));
   (void)ext; (void)a; (void)b; (void)c; (void)d;
   if (!l || !(dp = opendir(dir)))
   {
      free(l);
      return NULL;
   }
   l->elems = (struct string_list_elem*)calloc(64, sizeof(*l->elems));
   while ((e = readdir(dp)) && l->size < 64)
   {
      char p[300];
      if (e->d_name[0] == '.')
         continue;
      snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
      l->elems[l->size++].data = strdup(p);
   }
   closedir(dp);
   l->cap = 64;
   return l;
}
void dir_list_free(struct string_list *l)
{
   unsigned i;
   if (!l)
      return;
   for (i = 0; i < l->size; i++)
      free(l->elems[i].data);
   free(l->elems);
   free(l);
}
const char *path_basename_nocompression(const char *path)
{
   const char *s = strrchr(path, '/');
   return s ? s + 1 : path;
}

/* ---- the screen, read back through writeback ------------------------- */

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("   FAIL "); \
   printf(__VA_ARGS__); printf("\n"); } } while (0)

static struct
{
   uint32_t conn, crtc_prop, fb_prop, fence_prop;
} wb;

static uint32_t conn_prop(uint32_t conn, const char *name)
{
   uint32_t id = 0, i;
   drmModeObjectProperties *p = drmModeObjectGetProperties(drm.fd, conn,
         DRM_MODE_OBJECT_CONNECTOR);
   for (i = 0; p && i < p->count_props && !id; i++)
   {
      drmModePropertyRes *r = drmModeGetProperty(drm.fd, p->props[i]);
      if (r && !strcmp(r->name, name))
         id = r->prop_id;
      drmModeFreeProperty(r);
   }
   drmModeFreeObjectProperties(p);
   return id;
}

static bool wb_find(void)
{
   int i;
   drmModeRes *res;
   memset(&wb, 0, sizeof(wb));
   if (     drmSetClientCap(drm.fd, DRM_CLIENT_CAP_ATOMIC, 1)
         || drmSetClientCap(drm.fd, DRM_CLIENT_CAP_WRITEBACK_CONNECTORS, 1)
         || !(res = drmModeGetResources(drm.fd)))
      return false;
   for (i = 0; i < res->count_connectors && !wb.conn; i++)
   {
      drmModeConnector *c = drmModeGetConnector(drm.fd, res->connectors[i]);
      if (c && c->connector_type == DRM_MODE_CONNECTOR_WRITEBACK)
         wb.conn = c->connector_id;
      drmModeFreeConnector(c);
   }
   drmModeFreeResources(res);
   wb.crtc_prop  = conn_prop(wb.conn, "CRTC_ID");
   wb.fb_prop    = conn_prop(wb.conn, "WRITEBACK_FB_ID");
   wb.fence_prop = conn_prop(wb.conn, "WRITEBACK_OUT_FENCE_PTR");
   return wb.conn && wb.crtc_prop && wb.fb_prop && wb.fence_prop;
}

/* What the CRTC composes now, XRGB8888 rows of the mode's width; NULL
 * when writeback gave nothing back */
static uint32_t *capture(unsigned *w, unsigned *h)
{
   struct modeset_buf buf;
   drmModeAtomicReq *req;
   int32_t fence = -1;
   uint32_t *out = NULL;
   unsigned y;

   *w = drm.mode.hdisplay;
   *h = drm.mode.vdisplay;
   memset(&buf, 0, sizeof(buf));
   buf.dims = VIDEO_SCALE_PACK(*w, *h);
   if (modeset_create_dumbfb(drm.fd, &buf, 4, DRM_FORMAT_XRGB8888))
      return NULL;
   /* What a flip on its way shows is taken up first */
   drm_flip_wait();
   req = drmModeAtomicAlloc();
   drmModeAtomicAddProperty(req, wb.conn, wb.crtc_prop, drm.crtc_id);
   drmModeAtomicAddProperty(req, wb.conn, wb.fb_prop, buf.fb_id);
   drmModeAtomicAddProperty(req, wb.conn, wb.fence_prop,
         (uint64_t)(uintptr_t)&fence);
   if (!drmModeAtomicCommit(drm.fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET,
            NULL) && fence >= 0)
   {
      struct pollfd p;
      p.fd     = fence;
      p.events = POLLIN;
      if (poll(&p, 1, 5000) == 1 && (out = (uint32_t*)malloc(
                  (size_t)*w * *h * 4)))
         for (y = 0; y < *h; y++)
            memcpy(out + (size_t)y * *w, buf.map + (size_t)y * buf.stride,
                  (size_t)*w * 4);
   }
   if (fence >= 0)
      close(fence);
   drmModeAtomicFree(req);
   modeset_destroy_dumbfb(&buf);
   return out;
}

/* ---- what the screen should show ------------------------------------- */

static uint32_t pattern_px(unsigned x, unsigned y, unsigned seed)
{
   return ((x * 3 + seed) & 0xff) << 16 | ((y * 5 + seed) & 0xff) << 8
      | ((x ^ y ^ seed) & 0xff);
}

/* A @w x @h XRGB8888 frame at @pitch bytes a row */
static void pattern(uint8_t *f, unsigned w, unsigned h, size_t pitch,
      unsigned seed)
{
   unsigned x, y;
   for (y = 0; y < h; y++)
      for (x = 0; x < w; x++)
         ((uint32_t*)(f + y * pitch))[x] = pattern_px(x, y, seed);
}

/* The frame pixel seen at (ox, oy) with the frame turned @rot quarter
 * turns counter-clockwise */
static uint32_t turned(unsigned rot, unsigned w, unsigned h, unsigned ox,
      unsigned oy, unsigned seed)
{
   unsigned x = ox, y = oy;
   switch (rot & 3)
   {
      case 1: x = w - 1 - oy; y = ox;         break;
      case 2: x = w - 1 - ox; y = h - 1 - oy; break;
      case 3: x = oy;         y = h - 1 - ox; break;
   }
   return pattern_px(x, y, seed);
}

/* The screen shows the @w x @h pattern turned @rot, filling it; and the
 * screenshot readback shows the same */
static void expect_screen(struct drm_video *v, const char *what,
      unsigned rot, unsigned w, unsigned h, unsigned seed)
{
   unsigned sw, sh, x, y, bad = 0, rbad = 0;
   uint32_t *s = capture(&sw, &sh);
   struct video_viewport vp;
   uint8_t *rb;

   CHECK(s, "%s: writeback gave nothing back", what);
   if (!s)
      return;
   CHECK(sw == ((rot & 1) ? h : w) && sh == ((rot & 1) ? w : h),
         "%s: the screen is %ux%u", what, sw, sh);
   for (y = 0; y < sh && sw == ((rot & 1) ? h : w); y++)
      for (x = 0; x < sw; x++)
         if ((s[(size_t)y * sw + x] & 0xffffff) != turned(rot, w, h, x, y,
                  seed))
         {
            if (!bad)
               printf("   %s: (%u, %u) is %06x, want %06x\n", what, x, y,
                     s[(size_t)y * sw + x] & 0xffffff,
                     turned(rot, w, h, x, y, seed));
            bad++;
         }
   CHECK(!bad, "%s: %u pixels on the screen are not the frame's", what,
         bad);

   memset(&vp, 0, sizeof(vp));
   drm_viewport_info(v, &vp);
   rb = (uint8_t*)malloc((size_t)VIDEO_SCALE_AREA(vp.dims) * 3);
   CHECK(rb && vp.dims == VIDEO_SCALE_PACK(sw, sh)
         && drm_read_viewport(v, rb, false),
         "%s: no screenshot of the screen's size", what);
   if (rb && vp.dims == VIDEO_SCALE_PACK(sw, sh))
   {
      for (y = 0; y < sh; y++)
         for (x = 0; x < sw; x++)
         {
            const uint8_t *q = rb + ((size_t)(sh - 1 - y) * sw + x) * 3;
            uint32_t p = s[(size_t)y * sw + x];
            if (     q[0] != (p & 0xff) || q[1] != ((p >> 8) & 0xff)
                  || q[2] != ((p >> 16) & 0xff))
               rbad++;
         }
      CHECK(!rbad, "%s: the screenshot differs from the screen in %u pixels",
            what, rbad);
   }
   free(rb);
   free(s);
}

int main(int argc, char **argv)
{
   static video_info_t info;
   struct drm_video *v;
   drmVersionPtr ver;
   unsigned W, H, before, r;
   uint8_t *frame;

   verbose = argc > 1 && !strcmp(argv[1], "-v");
   setvbuf(stdout, NULL, _IONBF, 0);
   info.rgb32 = true;
   v = (struct drm_video*)drm_init(&info);
   if (!v)
   {
      printf("[skip] drm_vkms_live_test: no card the driver takes\n");
      return 77;
   }
   ver = drmGetVersion(drm.fd);
   if (!ver || strcmp(ver->name, "vkms"))
   {
      printf("[skip] drm_vkms_live_test: %s is not vkms\n",
            ver ? ver->name : "the card");
      drmFreeVersion(ver);
      drm_gfx_free(v);
      return 77;
   }
   drmFreeVersion(ver);
   CHECK(drm.atomic, "vkms takes atomic commits, the driver does not use them");
   if (!wb_find())
   {
      printf("FAIL drm_vkms_live_test: vkms has no writeback connector"
            " (load it with enable_writeback=1)\n");
      drm_gfx_free(v);
      return 1;
   }
   W = drm.mode.hdisplay;
   H = drm.mode.vdisplay;
   printf("   vkms at %ux%u, frames on plane %u\n", W, H, drm.plane_id);
   /* A frame the screen's size, at the screen's aspect: the plane shows
    * it pixel for pixel */
   drv_aspect = v->current_aspect = (float)W / H;
   frame      = (uint8_t*)malloc((size_t)W * H * 4);

   /* A frame as it is */
   before = failures;
   pattern(frame, W, H, W * 4, 1);
   CHECK(drm_frame(v, frame, VIDEO_SCALE_PACK(W, H), 0, W * 4, NULL, NULL),
         "the frame was refused");
   expect_screen(v, "a frame", 0, W, H, 1);
   if (failures == before)
      printf("   ok   a frame reaches the screen as it is, and is read back\n");

   /* Turned: by the plane where it turns that far, else by the copy -
    * either way the turn the core asked for */
   for (r = 1; r < 4; r++)
   {
      unsigned fw = (r & 1) ? H : W, fh = (r & 1) ? W : H;
      char what[64];
      before = failures;
      snprintf(what, sizeof(what), "%u quarter turns", r);
      drm_set_rotation(v, r);
      pattern(frame, fw, fh, fw * 4, 10 + r);
      CHECK(drm_frame(v, frame, VIDEO_SCALE_PACK(fw, fh), 0, fw * 4, NULL,
               NULL), "%s: the frame was refused", what);
      expect_screen(v, what, r, fw, fh, 10 + r);
      if (failures == before)
         printf("   ok   %u quarter turns, by the %s\n", r,
               v->main_surface && v->main_surface->hw_rot ? "plane" : "copy");
   }
   drm_set_rotation(v, 0);
   pattern(frame, W, H, W * 4, 2);
   drm_frame(v, frame, VIDEO_SCALE_PACK(W, H), 0, W * 4, NULL, NULL);
   expect_screen(v, "unturned again", 0, W, H, 2);

   /* The threaded wrapper's frame memory: scanned out where it lies,
    * each buffer written only while ready, as the wrapper does */
   {
      uintptr_t id[2];
      unsigned align = 0, k;
      size_t pitch = 0;
      uint8_t *mem[2];
      before = failures;
      for (k = 0; k < 2; k++)
         mem[k] = (uint8_t*)drm_frame_mem_alloc(v,
               (size_t)W * (H + 2) * 4 + 65536, &id[k], NULL, &align);
      CHECK(mem[0] && mem[1] && align, "no frame memory");
      if (mem[0] && mem[1])
      {
         pitch = ((size_t)W * 4 + align - 1) / align * align;
         CHECK(drm_frame_mem_ready(v, id[0]), "a new buffer is not ready");
         pattern(mem[0], W, H, pitch, 3);
         CHECK(drm_frame(v, mem[0], VIDEO_SCALE_PACK(W, H), 0,
                  (unsigned)pitch, NULL, NULL),
               "a frame in frame memory was refused");
         CHECK(v->main_surface->current_page
               && v->main_surface->current_page->buf.map == mem[0],
               "the kernel took no framebuffer at a pitch of %u",
               (unsigned)pitch);
         CHECK(!drm_frame_mem_ready(v, id[0]), "the buffer shown is ready");
         expect_screen(v, "frame memory", 0, W, H, 3);
         /* A frame inside the other buffer: the kernel takes a
          * framebuffer from where it starts. What reaches the screen is
          * not read back: vkms adds the framebuffer's offset twice (the
          * mapping it composes from has it added already), which a
          * display that scans out does not. */
         CHECK(drm_frame_mem_ready(v, id[1]), "the buffer not shown is not ready");
         pattern(mem[1] + pitch * 2, W, H, pitch, 4);
         drm_frame(v, mem[1] + pitch * 2, VIDEO_SCALE_PACK(W, H), 0,
               (unsigned)pitch, NULL, NULL);
         CHECK(v->main_surface->current_page
               && v->main_surface->current_page->buf.map == mem[1] + pitch * 2,
               "a frame inside frame memory was copied");
         /* The first again, now off the screen */
         CHECK(drm_frame_mem_ready(v, id[0]), "a buffer off the screen is not ready");
         pattern(mem[0], W, H, pitch, 5);
         drm_frame(v, mem[0], VIDEO_SCALE_PACK(W, H), 0, (unsigned)pitch,
               NULL, NULL);
         expect_screen(v, "frame memory again", 0, W, H, 5);
         /* Freed while shown: kept until a copied frame replaces it */
         drm_frame_mem_free(v, id[1]);
         drm_frame_mem_free(v, id[0]);
         pattern(frame, W, H, W * 4, 9);
         drm_frame(v, frame, VIDEO_SCALE_PACK(W, H), 0, W * 4, NULL, NULL);
         expect_screen(v, "after frame memory", 0, W, H, 9);
      }
      if (failures == before)
         printf("   ok   the threaded wrapper's frame memory scanned out where it lies\n");
   }

   /* Without vsync: commits that do not wait, their events read back;
    * the last frame shown once vsync is on again */
   {
      unsigned f, shown = 0;
      before = failures;
      drm_set_nonblock_state(v, true, false, 0);
      CHECK(drm.nonblock, "vsync stays on");
      for (f = 0; f < 120; f++)
      {
         pattern(frame, W, H, W * 4, 20 + (f & 7));
         drm_frame(v, frame, VIDEO_SCALE_PACK(W, H), 0, W * 4, NULL, NULL);
         if (drm.pending)
            shown++;
      }
      CHECK(shown, "no flip went out without waiting");
      drm_set_nonblock_state(v, false, false, 0);
      pattern(frame, W, H, W * 4, 6);
      drm_frame(v, frame, VIDEO_SCALE_PACK(W, H), 0, W * 4, NULL, NULL);
      CHECK(!drm.pending, "a flip is still on its way with vsync on");
      expect_screen(v, "vsync back on", 0, W, H, 6);
      if (failures == before)
         printf("   ok   flips without waiting, and the last frame with vsync back\n");
   }

#ifdef HAVE_OVERLAY
   /* The input overlay on a plane of its own, over the frame */
   {
      static uint32_t red[16 * 16];
      struct texture_image img;
      const video_overlay_interface_t *ovi = NULL;
      unsigned sw, sh, i, x0, y0;
      uint32_t *s;
      before = failures;
      for (i = 0; i < 16 * 16; i++)
         red[i] = 0xffff0000u;
      memset(&img, 0, sizeof(img));
      img.pixels = red;
      img.width  = 16;
      img.height = 16;
      drm_get_overlay_interface(v, &ovi);
      ovi->load(v, &img, 1);
      ovi->full_screen(v, true);
      ovi->vertex_geom(v, 0, 0.25f, 0.25f, 0.25f, 0.25f);
      ovi->enable(v, true);
      memset(frame, 0, (size_t)W * H * 4);
      drm_frame(v, frame, VIDEO_SCALE_PACK(W, H), 0, W * 4, NULL, NULL);
      x0 = W / 4;
      y0 = H / 4;
      s  = capture(&sw, &sh);
      CHECK(s, "overlay: writeback gave nothing back");
      if (s)
      {
         printf("   the overlay %s\n", v->ov.mode == DRM_OVERLAY_PLANE
               ? "on a plane of its own" : "blended into the frame");
         CHECK((s[(size_t)(y0 + 4) * sw + x0 + 4] & 0xffffff) == 0xff0000,
               "overlay: %06x where it is drawn",
               s[(size_t)(y0 + 4) * sw + x0 + 4] & 0xffffff);
         CHECK(!(s[(size_t)(y0 + H / 4 + 4) * sw + x0 + W / 4 + 4]
                  & 0xffffff), "overlay: drawn past its place");
         CHECK(!(s[4 * sw + 4] & 0xffffff), "overlay: the frame not black");
         free(s);
      }
      ovi->enable(v, false);
      pattern(frame, W, H, W * 4, 7);
      drm_frame(v, frame, VIDEO_SCALE_PACK(W, H), 0, W * 4, NULL, NULL);
      expect_screen(v, "overlay off", 0, W, H, 7);
      if (failures == before)
         printf("   ok   the input overlay over the frame, and gone again\n");
   }
#endif

#ifdef HAVE_MODELINE
   /* A CRT modeline: 640x480 at a clock no connector lists */
   {
      videocrt_switch_t *sw = &video_st.crt_switch_st;
      drmModeCrtc *c;
      before = failures;
      memset(sw, 0, sizeof(*sw));
      sw->clock = 25000; sw->hdisplay = 640; sw->hsync_start = 664;
      sw->hsync_end = 760; sw->htotal = 800; sw->vdisplay = 480;
      sw->vsync_start = 491; sw->vsync_end = 493; sw->vtotal = 525;
      sw->vrefresh = 60;
      drm_set_video_mode(v, VIDEO_SCALE_PACK(640, 480), true);
      c = drmModeGetCrtc(drm.fd, drm.crtc_id);
      CHECK(c && c->mode_valid && c->mode.hdisplay == 640
            && c->mode.htotal == 800 && c->mode.clock == 25000,
            "the modeline is not what the CRTC runs");
      drmModeFreeCrtc(c);
      drv_aspect = 4.0f / 3.0f;
      drm_apply_state_changes(v);
      pattern(frame, 640, 480, 640 * 4, 8);
      drm_frame(v, frame, VIDEO_SCALE_PACK(640, 480), 0, 640 * 4, NULL, NULL);
      expect_screen(v, "a CRT modeline", 0, 640, 480, 8);
      if (failures == before)
         printf("   ok   a CRT modeline set, and the frame shown at it\n");
   }
#endif

   drm_gfx_free(v);

   /* RGB565 frames, the format a 16-bit core hands over */
   info.rgb32 = false;
   drv_aspect = 4.0f / 3.0f;
   v = (struct drm_video*)drm_init(&info);
   CHECK(v, "init for RGB565 frames failed");
   if (v && wb_find())
   {
      unsigned sw, sh, x, y, bad = 0;
      uint32_t *scr;
      uint16_t *f16 = (uint16_t*)frame;
      before = failures;
      W = drm.mode.hdisplay;
      H = drm.mode.vdisplay;
      drv_aspect = v->current_aspect = (float)W / H;
      for (y = 0; y < H; y++)
         for (x = 0; x < W; x++)
            f16[(size_t)y * W + x] = (uint16_t)(((x & 31) << 11)
                  | ((y & 63) << 5) | ((x ^ y) & 31));
      CHECK(drm_frame(v, frame, VIDEO_SCALE_PACK(W, H), 0, W * 2, NULL,
               NULL), "an RGB565 frame was refused");
      scr = capture(&sw, &sh);
      CHECK(scr, "RGB565: writeback gave nothing back");
      for (y = 0; scr && y < sh; y++)
         for (x = 0; x < sw; x++)
         {
            uint32_t p = scr[(size_t)y * sw + x];
            int r = (int)((x & 31) * 255 / 31), g = (int)((y & 63) * 255 / 63);
            int b = (int)(((x ^ y) & 31) * 255 / 31);
            if (     abs((int)((p >> 16) & 0xff) - r) > 1
                  || abs((int)((p >> 8) & 0xff) - g) > 1
                  || abs((int)(p & 0xff) - b) > 1)
               bad++;
         }
      CHECK(!bad, "RGB565: %u pixels on the screen are not the frame's", bad);
      free(scr);
      if (failures == before)
         printf("   ok   an RGB565 frame reaches the screen\n");
   }
   if (v)
      drm_gfx_free(v);
   free(frame);
   if (failures)
   {
      printf("FAIL drm_vkms_live_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS drm_vkms_live_test\n");
   return 0;
}
