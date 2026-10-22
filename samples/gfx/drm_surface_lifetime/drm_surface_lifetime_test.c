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
#include <fcntl.h>
#include <unistd.h>

#include "../../../gfx/drivers/drm_gfx.c"
#include "../../../gfx/common/drm_common.c"

/* ---- frontend pieces the driver reaches ------------------------------ */

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END];
float video_driver_get_aspect_ratio(void) { return 4.0f / 3.0f; }
static unsigned retires;
void video_driver_cached_frame_retire(void) { retires++; }
static unsigned output_dims;
void video_driver_set_output_dims(unsigned dims) { output_dims = dims; }
static enum rarch_display_type display_type;
void video_driver_display_type_set(enum rarch_display_type t) { display_type = t; }
static settings_t settings;
settings_t *config_get_ptr(void) { return &settings; }

/* /dev/dri: a card with no display, a render node, a card with one */
static const char *dri_names[3] = { "/dev/dri/card0", "/dev/dri/renderD128",
   "/dev/dri/card1" };
struct string_list *dir_list_new(const char *dir, const char *ext,
      bool a, bool b, bool c, bool d)
{
   unsigned i;
   struct string_list *l = (struct string_list*)calloc(1, sizeof(*l));
   (void)dir; (void)ext; (void)a; (void)b; (void)c; (void)d;
   l->elems = (struct string_list_elem*)calloc(3, sizeof(*l->elems));
   for (i = 0; i < 3; i++)
      l->elems[i].data = strdup(dri_names[i]);
   l->size = l->cap = 3;
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

/* ---- the fake kernel ------------------------------------------------- */

#define FAKE_FD      7
#define BARE_FD      6   /* the card with no display */
#define CONSOLE      1   /* the blank frame under the planes */
#define PLANE_ID     31
#define CRTC_ID      41
#define FB_PROP_ID   51
#define TYPE_PROP_ID 52
#define MAX_OBJ      512

static int      dumb_alive[MAX_OBJ];   /* by handle */
static uint32_t dumb_size[MAX_OBJ];
static int      fb_alive[MAX_OBJ];     /* by fb id */
static uint32_t fb_format[MAX_OBJ];    /* the format each was made with */
static unsigned maps_alive;
static uint32_t plane_fb;              /* the fb the plane scans out */
static unsigned blanks;                /* times it was removed from under the plane */
static uint32_t next_handle = 1, next_fb = 1;

/* Row alignment the kernel gives dumb buffers */
static uint32_t pitch_align = 1;

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
      c->pitch  = (c->width * (c->bpp / 8) + pitch_align - 1)
         / pitch_align * pitch_align;
      c->size   = (uint64_t)c->pitch * c->height;
      dumb_alive[c->handle] = 1;
      dumb_size[c->handle]  = (uint32_t)c->size;
      return 0;
   }
   if (request == DRM_IOCTL_MODE_ADDFB2)
   {
      struct drm_mode_fb_cmd2 *f = (struct drm_mode_fb_cmd2*)arg;
      if (next_fb >= MAX_OBJ || !dumb_alive[f->handles[0]])
         return -1;
      f->fb_id = next_fb++;
      fb_alive[f->fb_id]  = 1;
      fb_format[f->fb_id] = f->pixel_format;
      return 0;
   }
   /* Legacy: a depth and bpp, which name XRGB8888 or RGB565 at best */
   if (request == DRM_IOCTL_MODE_ADDFB)
   {
      struct drm_mode_fb_cmd *f = (struct drm_mode_fb_cmd*)arg;
      if (next_fb >= MAX_OBJ || !dumb_alive[f->handle])
         return -1;
      f->fb_id = next_fb++;
      fb_alive[f->fb_id]  = 1;
      fb_format[f->fb_id] = f->bpp == 16 ? DRM_FORMAT_RGB565 : DRM_FORMAT_XRGB8888;
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

/* The overlay plane, beside the frame's */
#define PLANE2_ID   33
static uint32_t ov_fb;                 /* the fb the overlay plane scans out */
static int32_t  ov_x, ov_y;
static uint32_t ov_w, ov_h;
static unsigned ov_blanks, ov_setplanes;

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
   if (fb == ov_fb)
   {
      ov_blanks++;
      ov_fb = 0;
   }
   fb_alive[fb] = 0;
   return 0;
}

int drmModeSetPlane(int fd, uint32_t plane, uint32_t crtc, uint32_t fb,
      uint32_t flags, int32_t cx, int32_t cy, uint32_t cw, uint32_t ch,
      uint32_t sx, uint32_t sy, uint32_t sw, uint32_t sh)
{
   (void)fd; (void)crtc; (void)flags; (void)sx; (void)sy; (void)sw; (void)sh;
   if (plane == PLANE2_ID)
   {
      ov_setplanes++;
      ov_fb = fb;
      ov_x  = cx; ov_y = cy; ov_w = cw; ov_h = ch;
      return 0;
   }
   plane_fb = fb;
   return 0;
}

/* The frame's plane, an overlay on the CRTC, reading XRGB8888 and
 * RGB565, and RGBX4444 when count_formats says so; and a second overlay
 * plane, there when planes is 2, reading ARGB8888 when count_formats
 * says so, with a zpos and a pixel blend mode when it has them */
static uint32_t       plane_ids[2] = { PLANE_ID, PLANE2_ID };
static uint32_t       formats[3]   = { DRM_FORMAT_XRGB8888, DRM_FORMAT_RGB565,
   DRM_FORMAT_RGBX4444 };
static uint32_t       formats2[1]  = { DRM_FORMAT_ARGB8888 };
static drmModePlaneRes  plane_res  = { 1, plane_ids };
static drmModePlane     plane      = { 2, formats, PLANE_ID, CRTC_ID, 0, 0, 0, 0, 0, 1, 0 };
static drmModePlane     plane2     = { 1, formats2, PLANE2_ID, 0, 0, 0, 0, 0, 0, 1, 0 };

/* Per plane: FB_ID, type, zpos, pixel blend mode; the frame's from 51,
 * the overlay's from 71 */
#define PROP_FB    0
#define PROP_TYPE  1
#define PROP_ZPOS  2
#define PROP_BLEND 3
typedef struct
{
   uint64_t zpos, zpos_max, blend;
   int has_zpos, zpos_fixed, has_blend;
} fake_plane_props_t;
static fake_plane_props_t pp[2];
#define PROP_BASE(i) ((i) ? 71u : 51u)

static int plane_index(uint32_t id) { return id == PLANE2_ID ? 1 : 0; }

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
static unsigned plane_queries;
drmModePlanePtr drmModeGetPlane(int fd, uint32_t id)
{
   (void)fd;
   plane_queries++;
   return (drmModePlanePtr)dup_obj(id == PLANE2_ID ? &plane2 : &plane,
         sizeof(plane));
}
void drmModeFreePlane(drmModePlanePtr p) { free(p); }

/* One allocation each, freed whole, as the arrays live inside it */
drmModeObjectPropertiesPtr drmModeObjectGetProperties(int fd, uint32_t id, uint32_t type)
{
   int i = plane_index(id);
   uint32_t n = 2;
   drmModeObjectPropertiesPtr o = (drmModeObjectPropertiesPtr)calloc(1,
         sizeof(*o) + 4 * (sizeof(uint32_t) + sizeof(uint64_t)));
   (void)fd; (void)type;
   o->prop_values = (uint64_t*)(o + 1);
   o->props       = (uint32_t*)(o->prop_values + 4);
   o->props[0] = PROP_BASE(i) + PROP_FB;   o->prop_values[0] = 0;
   o->props[1] = PROP_BASE(i) + PROP_TYPE; o->prop_values[1] = DRM_PLANE_TYPE_OVERLAY;
   if (pp[i].has_zpos)
   {
      o->props[n] = PROP_BASE(i) + PROP_ZPOS; o->prop_values[n++] = pp[i].zpos;
   }
   if (pp[i].has_blend)
   {
      o->props[n] = PROP_BASE(i) + PROP_BLEND; o->prop_values[n++] = pp[i].blend;
   }
   o->count_props = n;
   return o;
}
void drmModeFreeObjectProperties(drmModeObjectPropertiesPtr p) { free(p); }
drmModePropertyPtr drmModeGetProperty(int fd, uint32_t id)
{
   static const char *blend_names[3] = { "None", "Pre-multiplied", "Coverage" };
   int i     = (id >= 71) ? 1 : 0;
   int which = (int)(id - PROP_BASE(i));
   drmModePropertyPtr r = (drmModePropertyPtr)calloc(1, sizeof(*r)
         + 2 * sizeof(uint64_t) + 3 * sizeof(struct drm_mode_property_enum));
   (void)fd;
   r->prop_id = id;
   switch (which)
   {
      case PROP_FB:   strlcpy(r->name, "FB_ID", sizeof(r->name)); break;
      case PROP_TYPE: strlcpy(r->name, "type", sizeof(r->name)); break;
      case PROP_ZPOS:
         strlcpy(r->name, "zpos", sizeof(r->name));
         r->flags        = DRM_MODE_PROP_RANGE
            | (pp[i].zpos_fixed ? DRM_MODE_PROP_IMMUTABLE : 0);
         r->count_values = 2;
         r->values       = (uint64_t*)(r + 1);
         r->values[0]    = 0;
         r->values[1]    = pp[i].zpos_max;
         break;
      case PROP_BLEND:
      {
         int k;
         strlcpy(r->name, "pixel blend mode", sizeof(r->name));
         r->flags       = DRM_MODE_PROP_ENUM;
         r->count_enums = 3;
         r->enums       = (struct drm_mode_property_enum*)
            ((uint64_t*)(r + 1) + 2);
         for (k = 0; k < 3; k++)
         {
            r->enums[k].value = (uint64_t)k;
            strlcpy(r->enums[k].name, blend_names[k], sizeof(r->enums[k].name));
         }
         break;
      }
   }
   return r;
}
void drmModeFreeProperty(drmModePropertyPtr p) { free(p); }
int drmModeObjectSetProperty(int fd, uint32_t obj, uint32_t type,
      uint32_t prop, uint64_t v)
{
   int i = plane_index(obj);
   (void)fd; (void)type;
   if (prop == PROP_BASE(i) + PROP_ZPOS)
   {
      if (pp[i].zpos_fixed || v > pp[i].zpos_max)
         return -1;
      pp[i].zpos = v;
      return 0;
   }
   if (prop == PROP_BASE(i) + PROP_BLEND)
   {
      pp[i].blend = v;
      return 0;
   }
   return -1;
}

/* The flip: the atomic request carries the new fb for each plane */
static uint32_t pending_fb, pending_ov_fb;
static unsigned atomic_allocs, ov_commits;
drmModeAtomicReqPtr drmModeAtomicAlloc(void)
{ atomic_allocs++; return (drmModeAtomicReqPtr)calloc(1, 16); }
void drmModeAtomicSetCursor(drmModeAtomicReqPtr r, int c) { (void)r; (void)c; }
void drmModeAtomicFree(drmModeAtomicReqPtr r) { free(r); }
int drmModeAtomicAddProperty(drmModeAtomicReqPtr r, uint32_t obj, uint32_t prop, uint64_t v)
{
   (void)r;
   if (obj == PLANE2_ID && prop == PROP_BASE(1) + PROP_FB)
      pending_ov_fb = (uint32_t)v;
   else
      pending_fb = (uint32_t)v;
   return 0;
}
static unsigned commits;
int drmModeAtomicCommit(int fd, drmModeAtomicReqPtr r, uint32_t f, void *u)
{
   (void)fd; (void)r; (void)f; (void)u;
   commits++;
   if (     !pending_fb || !fb_alive[pending_fb]
         || (pending_ov_fb && !fb_alive[pending_ov_fb]))
      return -1;
   plane_fb = pending_fb;
   if (pending_ov_fb)
   {
      ov_fb = pending_ov_fb;
      ov_commits++;
   }
   pending_ov_fb = 0;
   return 0;
}

/* The device: one CRTC lit at 1920x1080 by the console, one encoder on
 * it, one connector with three modes; BARE_FD's connector is not
 * connected. Opens and closes of the fake cards are counted. */
#define CONN_ID 90
#define ENC_ID  61
static drmModeModeInfo modes[3];
static int      atomic_ok = 1, fds_open;
static uint32_t crtc_fb = 9, crtc_conns;
static drmModeModeInfo crtc_mode;
static int      crtc_mode_valid = 1;

int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...)
{
   if (!strcmp(path, "/dev/dri/card1")) { fds_open++; return FAKE_FD; }
   if (!strcmp(path, "/dev/dri/card0")) { fds_open++; return BARE_FD; }
   if (!strncmp(path, "/dev/dri/", 9))  return -1;
   return __real_open(path, flags, 0644);
}
int __real_close(int fd);
int __wrap_close(int fd)
{
   if (fd == FAKE_FD || fd == BARE_FD) { fds_open--; return 0; }
   return __real_close(fd);
}

int drmSetClientCap(int fd, uint64_t c, uint64_t v)
{ (void)fd; (void)v; return (c == DRM_CLIENT_CAP_ATOMIC && !atomic_ok) ? -1 : 0; }
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
   c->connector_id   = id;
   c->connection     = fd == BARE_FD ? DRM_MODE_DISCONNECTED : DRM_MODE_CONNECTED;
   c->encoder_id     = ENC_ID;
   c->count_modes    = fd == BARE_FD ? 0 : 3;
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
   c->mode_valid = crtc_mode_valid;
   c->mode       = modes[0];
   return c;
}
void drmModeFreeCrtc(drmModeCrtcPtr p) { free(p); }
int drmModeSetCrtc(int fd, uint32_t c, uint32_t b, uint32_t x, uint32_t y,
      uint32_t *conn, int n, drmModeModeInfoPtr m)
{
   (void)fd; (void)c; (void)x; (void)y; (void)conn;
   crtc_fb    = b;
   crtc_conns = (uint32_t)n;
   if (m)
      crtc_mode = *m;
   return 0;
}
int drmHandleEvent(int fd, drmEventContextPtr ev) { (void)fd; (void)ev; return 0; }

static void set_mode(drmModeModeInfo *m, unsigned w, unsigned h, unsigned hz)
{
   m->hdisplay = (uint16_t)w;
   m->vdisplay = (uint16_t)h;
   m->htotal   = (uint16_t)w;
   m->vtotal   = (uint16_t)h;
   m->clock    = w * h * hz / 1000;
   m->vrefresh = hz;
}

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
   static video_info_t info;
   struct drm_video      *v;
   unsigned               i, before;

   setvbuf(stdout, NULL, _IONBF, 0);
   set_mode(&modes[0], 1920, 1080, 60);
   set_mode(&modes[1], 1280, 720, 60);
   set_mode(&modes[2], 640, 480, 60);
   settings.floats.video_refresh_rate = 60.0f;

   /* Init: the card with no display passed over, the render node never
    * opened, the console's mode kept and a blank frame under the planes */
   info.rgb32 = true;
   v = (struct drm_video*)drm_init(&info);
   CHECK(v && drm.fd == FAKE_FD && fds_open == 1,
         "the card with a display is the one kept (%d open)", fds_open);
   CHECK(v && drm.mode.hdisplay == 1920 && crtc_mode.hdisplay == 1920
         && crtc_fb && fb_alive[crtc_fb] && crtc_fb == drm.console.fb_id,
         "the mode the console ran, with a blank frame under the planes");
   CHECK(display_type == RARCH_DISPLAY_KMS && g_drm_mode == &drm.mode
         && output_dims == VIDEO_SCALE_PACK(1920, 1080),
         "the KMS display server is given the mode");
   if (!v)
      return 1;
   v->current_aspect = 4.0f / 3.0f;

   core_frame(v, 320, 240);
   CHECK(count(dumb_alive) == 3 + CONSOLE && count(fb_alive) == 3 + CONSOLE && maps_alive == 3 + CONSOLE,
         "a 3-page surface: %u buffers, %u fbs, %u maps",
         count(dumb_alive), count(fb_alive), maps_alive);
   CHECK(v->main_surface
         && fb_format[v->main_surface->pages[0].buf.fb_id] == DRM_FORMAT_XRGB8888,
         "the frame's framebuffers are XRGB8888");
   {
      /* A 16-bit core's frames are RGB565 framebuffers */
      struct drm_surface *s16 = NULL;
      CHECK(drm_surface_setup(v, VIDEO_SCALE_PACK(64, 32), 128, 2,
               DRM_FORMAT_RGB565, 255, 1.0f, 2, 0, &s16)
            && fb_format[s16->pages[0].buf.fb_id] == DRM_FORMAT_RGB565,
            "an RGB565 surface's framebuffers are RGB565");
      drm_surface_free(v, &s16);
   }
   CHECK(plane_fb && fb_alive[plane_fb], "the plane scans out no live fb");
   before = failures;

   /* the core changes resolution, again and again */
   for (i = 0; i < 6; i++)
      core_frame(v, (i & 1) ? 320 : 640, (i & 1) ? 240 : 480);
   CHECK(count(dumb_alive) == 3 + CONSOLE && count(fb_alive) == 3 + CONSOLE && maps_alive == 3 + CONSOLE,
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
   CHECK(count(dumb_alive) == 3 + CONSOLE && count(fb_alive) == 3 + CONSOLE && maps_alive == 3 + CONSOLE,
         "after 5 menu sessions: %u buffers, %u fbs, %u maps, want 3 each",
         count(dumb_alive), count(fb_alive), maps_alive);
   CHECK(blanks == 0, "the plane's fb was removed from under it %u time(s)", blanks);
   if (failures == before)
      printf("   ok   closing the menu takes its surface down with it\n");
   before = failures;

   /* a core's views: composed into a surface of their own, which the
    * plane scans out, and given back to the frame */
   {
      static video_frame_info_t info;
      struct retro_video_view vv[2];
      video_views_layout_params_t lp;
      struct drm_page *page;
      uint32_t px;
      unsigned x, y, off;
      bool same = true;

      memset(vv, 0, sizeof(vv));
      vv[0].width = vv[1].width = 256;
      vv[0].height = vv[1].height = 192;
      vv[1].y = 192;
      vv[1].screen = 1;
      video_views_validate(vv, 2, &info.views);
      memset(&lp, 0, sizeof(lp));
      lp.map         = &info.views;
      lp.dims        = VIDEO_SCALE_PACK(1920, 1080);
      lp.stereo_mode = VIDEO_STEREO_MODE_2D;
      video_views_layout(&lp, &info.views_layout);
      info.dims      = lp.dims;
      for (i = 0; i < 256 * 384; i++)
         frame_px[i] = 0x10000u * (i / 256) + (i % 256);

      drm_frame(v, frame_px, VIDEO_SCALE_PACK(256, 384), 0, 256 * 4, NULL,
            &info);
      CHECK(v->views_surface && plane_fb
            == v->views_surface->pages[!v->views_surface->flip_page].buf.fb_id,
            "the plane does not scan out the views' surface");
      /* The screens stacked, 384 rows of the surface's: each view's
       * pixels as they are, the plane enlarging them */
      if (v->views_surface)
      {
         CHECK(VIDEO_SCALE_H(v->views_surface->src_dims) == 384,
               "the views composed at %u rows, want 384",
               VIDEO_SCALE_H(v->views_surface->src_dims));
         page = &v->views_surface->pages[!v->views_surface->flip_page];
         off  = (VIDEO_SCALE_W(v->views_surface->src_dims) - 256) / 2;
         for (y = 0; y < 384; y += 37)
            for (x = 0; x < 256; x += 29)
            {
               px = ((const uint32_t*)page->buf.map)[(size_t)y
                  * (page->buf.stride / 4) + off + x];
               if ((px & 0xffffff) != (frame_px[y * 256 + x] & 0xffffff))
                  same = false;
            }
         CHECK(same, "the composed views differ from the frame's");
      }
      CHECK(count(dumb_alive) == 6 + CONSOLE && count(fb_alive) == 6 + CONSOLE,
            "the frame's surface and the views': %u buffers, %u fbs",
            count(dumb_alive), count(fb_alive));

      /* the map goes: the frame is shown whole again */
      memset(&info.views, 0, sizeof(info.views));
      drm_frame(v, frame_px, VIDEO_SCALE_PACK(256, 384), 0, 256 * 4, NULL,
            &info);
      CHECK(!v->views_surface && count(dumb_alive) == 3 + CONSOLE,
            "the views' surface outlives the map: %u buffers",
            count(dumb_alive));
      CHECK(blanks == 0, "the plane's fb was removed from under it %u time(s)", blanks);
      if (failures == before)
         printf("   ok   views composed for the plane, and given back to the frame\n");
      before = failures;
   }

   /* Rows the kernel pads; frames handed over, lent, and duped */
   {
      struct retro_framebuffer fb;
      struct drm_surface *s;
      const uint8_t *row;
      uint32_t lent_fb;
      unsigned allocs, w = 349, h = 200, x, y;
      bool same = true;

      pitch_align = 64;
      for (i = 0; i < w * h; i++)
         frame_px[i] = 0x00010203u * (i % 61) + i / w;
      core_frame(v, w, h);
      s = v->main_surface;
      CHECK(s && s->pages[0].buf.stride == 1408,
            "the fake kernel pads 349-pixel rows to 1408 bytes");
      /* The page just shown holds the frame, row by row at its stride */
      if (s)
      {
         row = s->pages[!s->flip_page].buf.map;
         for (y = 0; y < h; y += 7, row += 7 * s->pages[0].buf.stride)
            for (x = 0; x < w; x += 13)
               if (((const uint32_t*)row)[x] != frame_px[y * w + x])
                  same = false;
         CHECK(same, "a padded row is copied at the buffer's stride");
      }

      /* A frame from a buffer with a pitch of its own */
      {
         static uint32_t wide[360 * 200];
         same = true;
         for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
               wide[y * 360 + x] = frame_px[y * w + x] ^ 0xffffffu;
         drm_frame(v, wide, VIDEO_SCALE_PACK(w, h), 0, 360 * 4, NULL, NULL);
         row = s->pages[!s->flip_page].buf.map;
         for (y = 0; y < h; y += 7, row += 7 * s->pages[0].buf.stride)
            for (x = 0; x < w; x += 13)
               if (((const uint32_t*)row)[x] != wide[y * 360 + x])
                  same = false;
         CHECK(same, "each frame is read at the pitch it comes with");
      }

      /* Lent: the core renders into the next page, which is flipped as is */
      memset(&fb, 0, sizeof(fb));
      fb.width        = w;
      fb.height       = h;
      fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
      CHECK(drm_get_current_software_framebuffer(v, &fb),
            "the next page is lent");
      CHECK(fb.data == s->pages[s->flip_page].buf.map
            && fb.pitch == s->pages[s->flip_page].buf.stride
            && fb.format == RETRO_PIXEL_FORMAT_XRGB8888,
            "the lent page is the next one, at its stride and format");
      lent_fb = s->pages[s->flip_page].buf.fb_id;
      for (y = 0; y < h; y++)
         ((uint32_t*)((uint8_t*)fb.data + y * fb.pitch))[0] = 0xabcdefu + y;
      drm_frame(v, fb.data, VIDEO_SCALE_PACK(w, h), 0, (unsigned)fb.pitch,
            NULL, NULL);
      CHECK(plane_fb == lent_fb, "the lent page goes to the plane");
      CHECK(((uint32_t*)((uint8_t*)fb.data + 5 * fb.pitch))[0] == 0xabcdefu + 5,
            "what the core rendered is what is shown");

      /* Refused: read access, another size */
      fb.access_flags = RETRO_MEMORY_ACCESS_WRITE | RETRO_MEMORY_ACCESS_READ;
      CHECK(!drm_get_current_software_framebuffer(v, &fb),
            "a core reading its frame back is not given uncached memory");
      fb.access_flags = RETRO_MEMORY_ACCESS_WRITE;
      fb.width        = w + 1;
      CHECK(!drm_get_current_software_framebuffer(v, &fb),
            "a size the surface is not is refused");
      fb.width        = w;

      /* A dupe changes nothing and does not crash */
      lent_fb = plane_fb;
      drm_frame(v, NULL, VIDEO_SCALE_PACK(w, h), 0, 0, NULL, NULL);
      CHECK(plane_fb == lent_fb && v->main_surface == s,
            "a duped frame leaves the plane on the frame it shows");

      /* No allocation per flip */
      allocs = atomic_allocs;
      for (i = 0; i < 30; i++)
         core_frame(v, w, h);
      CHECK(atomic_allocs == allocs,
            "%u atomic requests made over 30 flips, want none",
            atomic_allocs - allocs);

      /* A resize after lending retires the cached frame first */
      retires = 0;
      pitch_align = 1;
      core_frame(v, 320, 240);
      CHECK(retires == 1, "the cache was retired %u times before the lent pages went",
            retires);
      retires = 0;
      core_frame(v, 640, 480);
      CHECK(retires == 0, "a surface never lent retires nothing (%u)", retires);
      CHECK(blanks == 0, "the plane's fb was removed from under it %u time(s)", blanks);
      if (failures == before)
         printf("   ok   padded rows, lending, dupes, and no allocation per flip\n");
      before = failures;
      core_frame(v, 320, 240);
   }

   /* The menu: RGUI's RGBA4444 converted, or scanned out as it is where
    * the plane takes it, and rendered straight into the next page */
   {
      static uint16_t rgui[640 * 480];
      struct drm_surface *s;
      const uint8_t *row;
      size_t pitch = 0;
      uint16_t *loan;
      uint32_t loan_fb;
      unsigned x, y, queries;
      bool same = true;

      for (i = 0; i < 640 * 480; i++)
         rgui[i] = (uint16_t)(0x1230u + i * 0x10u) | 0x5;

      CHECK(!drm_get_menu_framebuffer(v, false, VIDEO_SCALE_PACK(320, 240), &pitch),
            "nothing is lent while the menu is closed");

      /* A plane without RGBX4444: converted, and nothing lent */
      plane.count_formats = 2;
      drm_set_texture_enable(v, true, false);
      CHECK(!drm_get_menu_framebuffer(v, false, VIDEO_SCALE_PACK(320, 240), &pitch),
            "RGBA4444 is not lent into an XRGB8888 page");
      drm_set_texture_frame(v, rgui, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
      s = v->menu_surface;
      CHECK(s && s->pixformat == DRM_FORMAT_XRGB8888, "the menu converts to XRGB8888");
      if (s)
      {
         uint32_t px = ((const uint32_t*)s->pages[!s->flip_page].buf.map)[7];
         uint16_t in = rgui[7];
         CHECK(px == ((((in >> 12) & 0xF) * 0x11u) << 16
                  | (((in >> 8) & 0xF) * 0x11u) << 8 | ((in >> 4) & 0xF) * 0x11u),
               "a converted pixel is %08x", px);
      }
      drm_set_texture_enable(v, false, false);

      /* A plane that takes it: copied as is, and lent */
      plane.count_formats = 3;
      v->menu_probed      = false;
      drm_set_texture_enable(v, true, false);
      loan = (uint16_t*)drm_get_menu_framebuffer(v, false,
            VIDEO_SCALE_PACK(320, 240), &pitch);
      s = v->menu_surface;
      CHECK(loan && s && s->pixformat == DRM_FORMAT_RGBX4444
            && pitch == s->pages[s->flip_page].buf.stride,
            "the next RGBX4444 page is lent, at its stride");
      CHECK(s && fb_format[s->pages[0].buf.fb_id] == DRM_FORMAT_RGBX4444
            && fb_format[s->pages[1].buf.fb_id] == DRM_FORMAT_RGBX4444,
            "the menu's framebuffers are RGBX4444, as their pixels are");
      if (loan && s)
      {
         loan_fb = s->pages[s->flip_page].buf.fb_id;
         for (y = 0; y < 240; y++)
            for (x = 0; x < 320; x++)
               ((uint16_t*)((uint8_t*)loan + y * pitch))[x] = rgui[y * 320 + x];
         drm_set_texture_frame(v, loan, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
         CHECK(plane_fb == loan_fb, "the lent page goes to the plane");
         CHECK(((uint16_t*)((uint8_t*)loan + 9 * pitch))[11] == rgui[9 * 320 + 11],
               "what RGUI rendered is what is shown");

         /* From RGUI's own buffer: rows as they are */
         drm_set_texture_frame(v, rgui, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
         row = s->pages[!s->flip_page].buf.map;
         for (y = 0; y < 240; y += 11, row += 11 * s->pages[0].buf.stride)
            for (x = 0; x < 320; x += 17)
               if (((const uint16_t*)row)[x] != rgui[y * 320 + x])
                  same = false;
         CHECK(same, "RGBA4444 rows reach the page unconverted");
      }

      /* The plane is asked once, not every menu frame */
      queries = plane_queries;
      for (i = 0; i < 20; i++)
         drm_set_texture_frame(v, rgui, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
      CHECK(plane_queries == queries, "%u plane queries over 20 menu frames, want none",
            plane_queries - queries);

      /* Another size in the same session: a surface of that size */
      drm_set_texture_frame(v, rgui, false, VIDEO_SCALE_PACK(640, 480), 1.0f);
      CHECK(v->menu_surface
            && v->menu_surface->src_dims == VIDEO_SCALE_PACK(640, 480),
            "the menu surface follows the frame's size");
      CHECK(count(dumb_alive) == 5 + CONSOLE && count(fb_alive) == 5 + CONSOLE,
            "the frame's surface and the menu's: %u buffers, %u fbs",
            count(dumb_alive), count(fb_alive));
      drm_set_texture_enable(v, false, false);
      CHECK(!v->menu_surface && count(dumb_alive) == 3 + CONSOLE,
            "closing the menu leaves %u buffers", count(dumb_alive));
      CHECK(blanks == 0, "the plane's fb was removed from under it %u time(s)", blanks);
      if (failures == before)
         printf("   ok   the menu in RGUI's own format, lent, and resized\n");
      before = failures;
   }

   /* The input overlay: on a plane of its own above the frame's,
    * composed only when it changes and flipped with the frame; blended
    * into the frame where no plane can take it */
   {
      static uint32_t red[32 * 16], green[16 * 16];
      struct texture_image img[2];
      const video_overlay_interface_t *ovi = NULL;
      const uint32_t *row;
      unsigned setplanes, commits;
      uint32_t px, first_fb;

      for (i = 0; i < 32 * 16; i++)
         red[i] = 0xffff0000u;
      for (i = 0; i < 16 * 16; i++)
         green[i] = 0x8000ff00u;
      memset(img, 0, sizeof(img));
      img[0].pixels = red;   img[0].width = 32; img[0].height = 16;
      img[1].pixels = green; img[1].width = 16; img[1].height = 16;
      v->kms_dims = VIDEO_SCALE_PACK(1920, 1080);
      drm_get_overlay_interface(v, &ovi);

      /* Two planes; the frame's at a fixed zpos 1, the other below it
       * but free to be raised, and blending as coverage */
      plane_res.count_planes = 2;
      plane2.count_formats   = 1;
      pp[0].has_zpos   = 1; pp[0].zpos = 1; pp[0].zpos_fixed = 1; pp[0].zpos_max = 3;
      pp[1].has_zpos   = 1; pp[1].zpos = 0; pp[1].zpos_max = 3;
      pp[1].has_blend  = 1; pp[1].blend = 2;

      ovi->load(v, img, 2);
      ovi->full_screen(v, true);
      ovi->vertex_geom(v, 0, 0.1f, 0.2f, 0.05f, 0.05f);
      ovi->vertex_geom(v, 1, 0.5f, 0.5f, 0.1f, 0.1f);
      ovi->enable(v, true);
      core_frame(v, 320, 240);
      CHECK(v->ov.mode == DRM_OVERLAY_PLANE, "the overlay is on a plane of its own");
      CHECK(pp[1].zpos == 2, "the plane is raised above the frame's (zpos %u)",
            (unsigned)pp[1].zpos);
      CHECK(pp[1].blend == 1, "the plane blends premultiplied");
      CHECK(ov_fb && fb_alive[ov_fb] && fb_format[ov_fb] == DRM_FORMAT_ARGB8888,
            "the plane scans out an ARGB8888 page");
      CHECK(ov_x == 192 && ov_y == 216 && ov_w == 960 && ov_h == 432,
            "the plane covers the images only: %d,%d %ux%u", ov_x, ov_y, ov_w, ov_h);
      {
         struct modeset_buf *pg = v->ov.page[0].fb_id == ov_fb
            ? &v->ov.page[0] : &v->ov.page[1];
         row = (const uint32_t*)(pg->map + (size_t)27 * pg->stride);
         px  = row[48];
         CHECK(px == 0xffff0000u, "an opaque image as it is (%08x)", px);
         row = (const uint32_t*)(pg->map + (size_t)378 * pg->stride);
         px  = row[864];
         CHECK((px >> 24) >= 0x7f && (px >> 24) <= 0x81 && ((px >> 8) & 0xff) == (px >> 24),
               "a half-transparent one premultiplied (%08x)", px);
         row = (const uint32_t*)(pg->map + (size_t)200 * pg->stride);
         CHECK(row[400] == 0, "and nothing between them");
      }
      CHECK(plane_fb && fb_alive[plane_fb], "the frame stays on its plane");

      /* A press: the layer again, flipped with the frame, not placed */
      setplanes = ov_setplanes;
      commits   = ov_commits;
      first_fb  = ov_fb;
      ovi->set_alpha(v, 1, 0.5f);
      core_frame(v, 320, 240);
      CHECK(ov_commits == commits + 1 && ov_setplanes == setplanes
            && ov_fb != first_fb && fb_alive[ov_fb],
            "an alpha change goes up in the frame's own flip");
      ovi->set_alpha(v, 1, 0.5f);
      for (i = 0; i < 3; i++)
         core_frame(v, 320, 240);
      CHECK(ov_commits == commits + 1, "nothing changed, nothing composed or flipped");

      /* Off and on */
      ovi->enable(v, false);
      CHECK(!ov_fb, "turned off, the plane shows nothing");
      ovi->enable(v, true);
      core_frame(v, 320, 240);
      CHECK(ov_fb && fb_alive[ov_fb], "turned on, it is back");

      /* Moved: placed again, on pages of the new size */
      ovi->vertex_geom(v, 1, 0.2f, 0.25f, 0.1f, 0.1f);
      core_frame(v, 320, 240);
      CHECK(ov_x == 192 && ov_y == 216 && ov_w == 384 && ov_h == 162,
            "moved, the plane covers the images: %d,%d %ux%u", ov_x, ov_y, ov_w, ov_h);
      CHECK(ov_blanks == 0, "no page went while the plane read it");

      /* A plane fixed below the frame's: blended into the frame */
      drm_overlay_free(&v->ov);
      memset(&v->ov, 0, sizeof(v->ov));
      pp[1].zpos_fixed = 1; pp[1].zpos = 0;
      ovi->load(v, img, 2);
      ovi->full_screen(v, false);
      ovi->vertex_geom(v, 0, 0.0f, 0.0f, 0.5f, 0.5f);
      ovi->vertex_geom(v, 1, 0.0f, 0.0f, 0.0f, 0.0f);
      ovi->enable(v, true);
      memset(frame_px, 0, sizeof(frame_px));
      core_frame(v, 320, 240);
      CHECK(v->ov.mode == DRM_OVERLAY_BLEND, "a plane fixed below is not used");
      {
         struct modeset_buf *pg = &v->main_surface->pages[!v->main_surface->flip_page].buf;
         px = ((const uint32_t*)(pg->map + (size_t)10 * pg->stride))[10];
         CHECK((px & 0xffffff) == 0xff0000, "blended into the frame (%08x)", px);
         px = ((const uint32_t*)(pg->map + (size_t)200 * pg->stride))[200];
         CHECK((px & 0xffffff) == 0, "the frame elsewhere as it was (%08x)", px);
      }

      /* No zpos at all: a later plane is taken as drawn over */
      drm_overlay_free(&v->ov);
      memset(&v->ov, 0, sizeof(v->ov));
      pp[0].has_zpos = pp[1].has_zpos = 0;
      ovi->load(v, img, 2);
      ovi->enable(v, true);
      core_frame(v, 320, 240);
      CHECK(v->ov.mode == DRM_OVERLAY_PLANE, "without zpos, the later plane is used");

      /* No plane takes ARGB8888 */
      drm_overlay_free(&v->ov);
      memset(&v->ov, 0, sizeof(v->ov));
      plane2.count_formats = 0;
      ovi->load(v, img, 2);
      ovi->enable(v, true);
      core_frame(v, 320, 240);
      CHECK(v->ov.mode == DRM_OVERLAY_BLEND, "no ARGB8888 plane: blended");
      plane2.count_formats = 1;
      drm_overlay_free(&v->ov);
      memset(&v->ov, 0, sizeof(v->ov));
      pp[1].zpos_fixed = 0; pp[1].has_zpos = 1; pp[0].has_zpos = 1;
      ovi->load(v, img, 2);
      ovi->enable(v, true);
      core_frame(v, 320, 240);
      CHECK(v->ov.mode == DRM_OVERLAY_PLANE && ov_fb,
            "on a plane again for the free (mode %d, fb %u, zpos %u/%u)",
            (int)v->ov.mode, ov_fb, (unsigned)pp[0].zpos, (unsigned)pp[1].zpos);

      if (failures == before)
         printf("   ok   the overlay on a plane of its own, or blended into the frame\n");
      before = failures;
   }

   drm_gfx_free(v);
   CHECK(count(dumb_alive) == 0 && count(fb_alive) == 0 && maps_alive == 0,
         "after the driver's free: %u buffers, %u fbs, %u maps",
         count(dumb_alive), count(fb_alive), maps_alive);
   CHECK(!ov_fb && !ov_blanks, "the overlay plane let go before its pages went");
   if (failures == before)
      printf("   ok   the driver's free leaves nothing behind\n");
   before = failures;
   CHECK(crtc_fb == 9 && crtc_mode.hdisplay == 1920 && crtc_conns == 1,
         "the CRTC is given back the console's framebuffer and mode");
   CHECK(!plane_fb && fds_open == 0 && drm.fd == -1,
         "the plane is off and the card closed (%d open)", fds_open);
   if (failures == before)
      printf("   ok   init keeps the card with a display; free puts the console back\n");
   before = failures;

   /* A fullscreen size picks its mode; a switch takes the planes along */
   info.fullscreen = true;
   info.dims       = VIDEO_SCALE_PACK(1280, 720);
   v = (struct drm_video*)drm_init(&info);
   CHECK(v && drm.mode.hdisplay == 1280 && crtc_mode.hdisplay == 1280,
         "fullscreen at 1280x720 sets that mode");
   if (v)
   {
      v->current_aspect = 4.0f / 3.0f;
      core_frame(v, 320, 240);
      drm_poke_interface.set_video_mode(v, VIDEO_SCALE_PACK(640, 480), true);
      CHECK(crtc_mode.hdisplay == 640 && v->kms_dims == VIDEO_SCALE_PACK(640, 480)
            && output_dims == VIDEO_SCALE_PACK(640, 480),
            "switched to 640x480, and told so");
      CHECK(plane_fb && fb_alive[plane_fb] && drm.vp_h == 480,
            "the frame's plane is back, laid out on the new mode");
      CHECK(count(dumb_alive) == 3 + CONSOLE, "the old blank frame went: %u buffers",
            count(dumb_alive));
      drm_gfx_free(v);
   }
   CHECK(crtc_fb == 9 && crtc_mode.hdisplay == 1920 && !count(dumb_alive)
         && fds_open == 0, "and the console is back as it was");
   if (failures == before)
      printf("   ok   a fullscreen size and a resolution switch set the mode\n");
   before = failures;

   /* No atomic commits: the plane is flipped on its own */
   atomic_ok       = 0;
   info.fullscreen = false;
   v = (struct drm_video*)drm_init(&info);
   CHECK(v && !drm.atomic, "a card without atomic commits is taken");
   if (v)
   {
      uint32_t fb0;
      unsigned c0 = commits;
      v->current_aspect = 4.0f / 3.0f;
      core_frame(v, 320, 240);
      fb0 = plane_fb;
      core_frame(v, 320, 240);
      CHECK(commits == c0 && plane_fb && plane_fb != fb0 && fb_alive[plane_fb],
            "frames flip by plane updates, no commit");
      drm_gfx_free(v);
   }
   atomic_ok = 1;
   CHECK(fds_open == 0 && !count(dumb_alive), "and leave nothing");
   if (failures == before)
      printf("   ok   without atomic commits the plane is flipped on its own\n");
   before = failures;

   /* No plane shows frames: init fails, without exiting and leaving nothing */
   plane.count_formats = 0;
   v = (struct drm_video*)drm_init(&info);
   CHECK(!v && fds_open == 0 && !count(dumb_alive) && !count(fb_alive),
         "no plane for frames: init fails and leaves nothing open");
   plane.count_formats = 2;

   /* The menu with no content, opened and closed */
   v = (struct drm_video*)drm_init(&info);
   if (v)
   {
      static uint16_t rgui[320 * 240];
      drm_set_texture_enable(v, true, false);
      drm_set_texture_frame(v, rgui, false, VIDEO_SCALE_PACK(320, 240), 1.0f);
      drm_set_texture_enable(v, false, false);
      CHECK(!plane_fb && !v->menu_surface && !blanks,
            "closing the menu with nothing behind it turns the plane off first"
            " (fb %u, menu %p, blanks %u, plane %u)", plane_fb,
            (void*)v->menu_surface, blanks, drm.plane_id);
      drm_gfx_free(v);
   }
   CHECK(fds_open == 0 && !count(dumb_alive), "and leaves nothing");
   if (failures == before)
      printf("   ok   init without a plane fails cleanly; the menu alone closes cleanly\n");


   if (failures)
   {
      printf("FAIL drm_surface_lifetime_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS drm_surface_lifetime_test\n");
   return 0;
}
