/* The DRM driver's surfaces against a fake kernel.
 *
 * A surface is two or three pages, each a dumb buffer with a
 * framebuffer and a mapping. The main surface is made again on every
 * change of the core's resolution, and the menu's each time the menu
 * opens, so whatever a surface leaves behind when it goes accumulates.
 * And the kernel switches a plane off when the framebuffer it is
 * scanning out is removed, so a surface must not go while the plane
 * still reads from it.
 *
 * The fake kernel counts the dumb buffers, framebuffers and mappings
 * alive, and records a blank whenever the framebuffer on the plane is
 * removed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/mman.h>

#include "../../../gfx/drivers/drm_gfx.c"

/* ---- frontend pieces the driver reaches ------------------------------ */

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
drmModeModeInfo *g_drm_mode;
struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];
float video_driver_get_aspect_ratio(void) { return 4.0f / 3.0f; }

/* ---- the fake kernel ------------------------------------------------- */

#define FAKE_FD      7
#define PLANE_ID     31
#define CRTC_ID      41
#define FB_PROP_ID   51
#define TYPE_PROP_ID 52
#define MAX_OBJ      64

static int      dumb_alive[MAX_OBJ];   /* by handle */
static uint32_t dumb_size[MAX_OBJ];
static int      fb_alive[MAX_OBJ];     /* by fb id */
static unsigned maps_alive;
static uint32_t plane_fb;              /* the fb the plane scans out */
static unsigned blanks;                /* times it was removed from under the plane */
static uint32_t next_handle = 1, next_fb = 1;

static unsigned count(const int *a)
{
   unsigned i, n = 0;
   for (i = 0; i < MAX_OBJ; i++)
      n += a[i] != 0;
   return n;
}

int drmIoctl(int fd, unsigned long request, void *arg)
{
   (void)fd;
   if (request == DRM_IOCTL_MODE_CREATE_DUMB)
   {
      struct drm_mode_create_dumb *c = (struct drm_mode_create_dumb*)arg;
      if (next_handle >= MAX_OBJ)
         return -1;
      c->handle = next_handle++;
      c->pitch  = c->width * (c->bpp / 8);
      c->size   = (uint64_t)c->pitch * c->height;
      dumb_alive[c->handle] = 1;
      dumb_size[c->handle]  = (uint32_t)c->size;
      return 0;
   }
   if (request == DRM_IOCTL_MODE_ADDFB)
   {
      struct drm_mode_fb_cmd *f = (struct drm_mode_fb_cmd*)arg;
      if (next_fb >= MAX_OBJ || !dumb_alive[f->handle])
         return -1;
      f->fb_id = next_fb++;
      fb_alive[f->fb_id] = 1;
      return 0;
   }
   if (request == DRM_IOCTL_MODE_MAP_DUMB)
   {
      struct drm_mode_map_dumb *m = (struct drm_mode_map_dumb*)arg;
      if (!dumb_alive[m->handle])
         return -1;
      m->offset = (uint64_t)m->handle << 12;
      return 0;
   }
   if (request == DRM_IOCTL_MODE_DESTROY_DUMB)
   {
      struct drm_mode_destroy_dumb *d = (struct drm_mode_destroy_dumb*)arg;
      if (d->handle >= MAX_OBJ || !dumb_alive[d->handle])
         return -1;
      dumb_alive[d->handle] = 0;
      return 0;
   }
   return -1;
}

void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
   void *p;
   (void)addr; (void)prot; (void)flags; (void)off;
   if (fd != FAKE_FD || !(p = calloc(1, len)))
      return MAP_FAILED;
   maps_alive++;
   return p;
}

int __wrap_munmap(void *addr, size_t len)
{
   (void)len;
   free(addr);
   maps_alive--;
   return 0;
}

int drmModeRmFB(int fd, uint32_t fb)
{
   (void)fd;
   if (fb >= MAX_OBJ || !fb_alive[fb])
      return -1;
   if (fb == plane_fb)
   {
      blanks++;
      plane_fb = 0;
   }
   fb_alive[fb] = 0;
   return 0;
}

int drmModeSetPlane(int fd, uint32_t plane, uint32_t crtc, uint32_t fb,
      uint32_t flags, int32_t cx, int32_t cy, uint32_t cw, uint32_t ch,
      uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh)
{
   (void)fd; (void)plane; (void)crtc; (void)flags; (void)cx; (void)cy;
   (void)cw; (void)ch; (void)sx; (void)sy; (void)sw; (void)sh;
   plane_fb = fb;
   return 0;
}

/* One plane, an overlay on the CRTC, reading XRGB8888 and RGB565 */
static uint32_t       plane_ids[1] = { PLANE_ID };
static uint32_t       formats[2]   = { DRM_FORMAT_XRGB8888, DRM_FORMAT_RGB565 };
static uint32_t       prop_ids[2]  = { FB_PROP_ID, TYPE_PROP_ID };
static uint64_t       prop_vals[2] = { 0, DRM_PLANE_TYPE_OVERLAY };
static drmModePlaneRes  plane_res  = { 1, plane_ids };
static drmModePlane     plane      = { 2, formats, PLANE_ID, CRTC_ID, 0, 0, 0, 0, 0, 1, 0 };
static drmModeObjectProperties props = { 2, prop_ids, prop_vals };
static drmModePropertyRes prop_fb, prop_type;

/* Each a copy on the heap, as libdrm's are: one not handed back to
 * its free leaks, which LeakSanitizer reports. */
static void *dup_obj(const void *p, size_t n)
{
   void *c = malloc(n);
   if (c)
      memcpy(c, p, n);
   return c;
}
drmModePlaneResPtr drmModeGetPlaneResources(int fd)
{ (void)fd; return (drmModePlaneResPtr)dup_obj(&plane_res, sizeof(plane_res)); }
void drmModeFreePlaneResources(drmModePlaneResPtr p) { free(p); }
drmModePlanePtr drmModeGetPlane(int fd, uint32_t id)
{ (void)fd; (void)id; return (drmModePlanePtr)dup_obj(&plane, sizeof(plane)); }
void drmModeFreePlane(drmModePlanePtr p) { free(p); }
drmModeObjectPropertiesPtr drmModeObjectGetProperties(int fd, uint32_t id, uint32_t type)
{ (void)fd; (void)id; (void)type; return (drmModeObjectPropertiesPtr)dup_obj(&props, sizeof(props)); }
void drmModeFreeObjectProperties(drmModeObjectPropertiesPtr p) { free(p); }
drmModePropertyPtr drmModeGetProperty(int fd, uint32_t id)
{
   (void)fd;
   return (drmModePropertyPtr)dup_obj(id == FB_PROP_ID ? &prop_fb : &prop_type,
         sizeof(prop_fb));
}
void drmModeFreeProperty(drmModePropertyPtr p) { free(p); }

/* The flip: the atomic request carries the new fb for the plane */
static uint32_t pending_fb;
drmModeAtomicReqPtr drmModeAtomicAlloc(void) { return (drmModeAtomicReqPtr)calloc(1, 16); }
void drmModeAtomicFree(drmModeAtomicReqPtr r) { free(r); }
int drmModeAtomicAddProperty(drmModeAtomicReqPtr r, uint32_t obj, uint32_t prop, uint64_t v)
{ (void)r; (void)obj; (void)prop; pending_fb = (uint32_t)v; return 0; }
int drmModeAtomicCommit(int fd, drmModeAtomicReqPtr r, uint32_t f, void *u)
{
   (void)fd; (void)r; (void)f; (void)u;
   if (!pending_fb || !fb_alive[pending_fb])
      return -1;
   plane_fb = pending_fb;
   return 0;
}

/* Reached only by the init path, which this test does not take */
int drmSetClientCap(int fd, uint64_t c, uint64_t v) { (void)fd; (void)c; (void)v; return -1; }
drmModeResPtr drmModeGetResources(int fd) { (void)fd; return NULL; }
drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t id) { (void)fd; (void)id; return NULL; }
void drmModeFreeConnector(drmModeConnectorPtr p) { (void)p; }
drmModeEncoderPtr drmModeGetEncoder(int fd, uint32_t id) { (void)fd; (void)id; return NULL; }
void drmModeFreeEncoder(drmModeEncoderPtr p) { (void)p; }
drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t id) { (void)fd; (void)id; return NULL; }
int drmModeSetCrtc(int fd, uint32_t c, uint32_t b, uint32_t x, uint32_t y,
      uint32_t *conn, int n, drmModeModeInfoPtr m)
{ (void)fd; (void)c; (void)b; (void)x; (void)y; (void)conn; (void)n; (void)m; return 0; }

/* ---- the test -------------------------------------------------------- */

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("   FAIL "); \
   printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t frame_px[640 * 480];

static void core_frame(struct drm_video *v, unsigned w, unsigned h)
{
   drm_frame(v, frame_px, VIDEO_SCALE_PACK(w, h), 0, w * 4, NULL, NULL);
}

static void menu_open_close(struct drm_video *v)
{
   drm_set_texture_enable(v, true, false);
   drm_set_texture_frame(v, frame_px, true, VIDEO_SCALE_PACK(320, 240), 1.0f);
   drm_set_texture_enable(v, false, false);
}

int main(void)
{
   static drmModeModeInfo mode;
   static uint32_t        crtcs[1] = { CRTC_ID };
   static drmModeRes      res;
   struct drm_video      *v;
   unsigned               i, before;

   setvbuf(stdout, NULL, _IONBF, 0);
   strlcpy(prop_fb.name, "FB_ID", sizeof(prop_fb.name));
   prop_fb.prop_id = FB_PROP_ID;
   strlcpy(prop_type.name, "type", sizeof(prop_type.name));
   prop_type.prop_id = TYPE_PROP_ID;
   mode.hdisplay = 1920;
   mode.vdisplay = 1080;
   res.count_crtcs = 1;
   res.crtcs       = crtcs;
   drm.fd           = FAKE_FD;
   drm.crtc_id      = CRTC_ID;
   drm.current_mode = &mode;
   drm.resources    = &res;

   v = (struct drm_video*)calloc(1, sizeof(*v));
   v->rgb32          = true;
   v->current_aspect = 4.0f / 3.0f;
   v->pending_mutex    = slock_new();
   v->vsync_cond_mutex = slock_new();
   v->vsync_condition  = scond_new();

   core_frame(v, 320, 240);
   CHECK(count(dumb_alive) == 3 && count(fb_alive) == 3 && maps_alive == 3,
         "a 3-page surface: %u buffers, %u fbs, %u maps",
         count(dumb_alive), count(fb_alive), maps_alive);
   CHECK(plane_fb && fb_alive[plane_fb], "the plane scans out no live fb");
   before = failures;

   /* the core changes resolution, again and again */
   for (i = 0; i < 6; i++)
      core_frame(v, (i & 1) ? 320 : 640, (i & 1) ? 240 : 480);
   CHECK(count(dumb_alive) == 3 && count(fb_alive) == 3 && maps_alive == 3,
         "after 6 resolution changes: %u buffers, %u fbs, %u maps, want 3 each",
         count(dumb_alive), count(fb_alive), maps_alive);
   CHECK(blanks == 0, "the plane's fb was removed from under it %u time(s)", blanks);
   if (failures == before)
      printf("   ok   a resolution change leaves only the new surface, and the plane never blanks\n");
   before = failures;

   /* the menu opens and closes */
   for (i = 0; i < 5; i++)
   {
      menu_open_close(v);
      core_frame(v, 320, 240);
   }
   CHECK(count(dumb_alive) == 3 && count(fb_alive) == 3 && maps_alive == 3,
         "after 5 menu sessions: %u buffers, %u fbs, %u maps, want 3 each",
         count(dumb_alive), count(fb_alive), maps_alive);
   CHECK(blanks == 0, "the plane's fb was removed from under it %u time(s)", blanks);
   if (failures == before)
      printf("   ok   closing the menu takes its surface down with it\n");
   before = failures;

   drm_gfx_free(v);
   CHECK(count(dumb_alive) == 0 && count(fb_alive) == 0 && maps_alive == 0,
         "after the driver's free: %u buffers, %u fbs, %u maps",
         count(dumb_alive), count(fb_alive), maps_alive);
   if (failures == before)
      printf("   ok   the driver's free leaves nothing behind\n");

   if (failures)
   {
      printf("FAIL drm_surface_lifetime_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS drm_surface_lifetime_test\n");
   return 0;
}
