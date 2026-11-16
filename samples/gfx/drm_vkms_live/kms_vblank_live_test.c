/* gfx/display_servers/dispserv_kms.c's vblank clock against a real
 * kernel's vkms: the CRTC's vblanks counted and timed, the next one
 * waited for on a description that is not the master's, and the
 * master's own events left for it. Run through run_vm.sh. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "../../../gfx/video_display_server.h"
#include "../../../gfx/video_driver.h"
#include "../../../command.h"

extern const video_display_server_t dispserv_kms;

/* what dispserv_kms.c reaches for outside itself */
int               g_drm_fd        = -1;
uint32_t          g_crtc_id       = 0;
drmModeConnector *g_drm_connector = NULL;
drmModeModeInfo  *g_drm_mode      = NULL;
static video_driver_state_t s_video_st;

float drm_calc_refresh_rate(drmModeModeInfo *mode)
{
   if (!mode || !mode->htotal || !mode->vtotal)
      return 0.0f;
   return (mode->clock * 1000.0f) / (mode->htotal * (float)mode->vtotal);
}
bool command_event(enum event_command cmd, void *data)
{ (void)cmd; (void)data; return true; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void video_monitor_set_refresh_rate(float hz) { (void)hz; }
video_driver_state_t *video_state_get_ptr(void) { return &s_video_st; }
bool video_driver_set_video_mode(unsigned dims, bool fullscreen)
{ (void)dims; (void)fullscreen; return true; }

static unsigned failures;
#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         failures++; \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
      } \
   } while (0)

static int64_t now_us(void)
{
   struct timespec now;
   clock_gettime(CLOCK_MONOTONIC, &now);
   return (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

int main(void)
{
   struct drm_mode_create_dumb dumb;
   struct pollfd pfd;
   drmModeRes *res;
   drmModeConnector *conn = NULL;
   retro_time_t at        = 0;
   retro_time_t prev_at, first_at;
   uint32_t fb            = 0;
   uint32_t conn_id;
   int64_t t0, took, period;
   unsigned n, seen, first, i, late;
   int c;

   g_drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
   if (g_drm_fd < 0)
   {
      printf("FAIL no /dev/dri/card0: %s\n", strerror(errno));
      return 1;
   }
   res = drmModeGetResources(g_drm_fd);
   if (!res || !res->count_crtcs)
   {
      printf("FAIL no CRTC\n");
      return 1;
   }
   for (c = 0; c < res->count_connectors && !conn; c++)
   {
      conn = drmModeGetConnector(g_drm_fd, res->connectors[c]);
      if (conn && (conn->connection != DRM_MODE_CONNECTED || !conn->count_modes))
      {
         drmModeFreeConnector(conn);
         conn = NULL;
      }
   }
   if (!conn)
   {
      printf("FAIL no connected connector\n");
      return 1;
   }
   conn_id         = conn->connector_id;
   g_drm_connector = conn;
   g_drm_mode      = &conn->modes[0];
   g_crtc_id       = res->crtcs[0];
   period          = (int64_t)(1000000.0f / drm_calc_refresh_rate(g_drm_mode));

   memset(&dumb, 0, sizeof(dumb));
   dumb.width  = g_drm_mode->hdisplay;
   dumb.height = g_drm_mode->vdisplay;
   dumb.bpp    = 32;
   if (     drmIoctl(g_drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb) != 0
         || drmModeAddFB(g_drm_fd, dumb.width, dumb.height, 24, 32,
               dumb.pitch, dumb.handle, &fb) != 0)
   {
      printf("FAIL no framebuffer: %s\n", strerror(errno));
      return 1;
   }

   if (!dispserv_kms.wait_vblank)
   {
      printf("FAIL dispserv_kms has no wait_vblank\n");
      return 1;
   }

   if (drmModeSetCrtc(g_drm_fd, g_crtc_id, fb, 0, 0, &conn_id, 1,
            g_drm_mode) != 0)
   {
      printf("FAIL mode set: %s\n", strerror(errno));
      return 1;
   }
   /* the first vblank after the mode set stamps the count */
   for (i = 0; i < 50 && !(seen = dispserv_kms.wait_vblank(NULL, 0, 0, &at)); i++)
      usleep(20000);
   CHECK(seen, "no vblank counted on an active CRTC");
   CHECK(at > 0 && now_us() - at < 4 * period && at - now_us() < period,
         "the vblank's time is %ld us from now", (long)(now_us() - at));
   printf("   ok   the count and its time, %ld us period\n", (long)period);

   /* ---- a page flip's event is the master's ---- */
   CHECK(drmModePageFlip(g_drm_fd, g_crtc_id, fb, DRM_MODE_PAGE_FLIP_EVENT,
            NULL) == 0, "page flip: %s", strerror(errno));

   /* ---- each next vblank, counted and stamped on the CRTC's grid:
    * a machine that stalls skips some, and their stamps say so ---- */
   t0      = now_us();
   first    = seen;
   first_at = at;
   prev_at  = at;
   late    = 0;
   for (i = 0; i < 30; i++)
   {
      int64_t apart;
      n     = dispserv_kms.wait_vblank(NULL, seen, 200000, &at);
      CHECK(n && n != seen && n - seen < 1000,
            "vblank %u: count %u after %u", i, n, seen);
      apart = (int64_t)(n - seen) * period;
      if (at - prev_at < apart - period / 4 || at - prev_at > apart + period / 4)
         late++;
      seen    = n;
      prev_at = at;
   }
   took = now_us() - t0;
   CHECK(seen - first >= 30
         && at - first_at > (int64_t)(seen - first) * period - period / 4
         && at - first_at < (int64_t)(seen - first) * period + period / 4,
         "%u vblanks counted over %ld us of stamps at a %ld us period",
         seen - first, (long)(at - first_at), (long)period);
   CHECK(!late, "%u of 30 vblank stamps were off the CRTC's grid", late);
   printf("   ok   %u vblanks over 30 waits, in %ld ms\n", seen - first,
         (long)(took / 1000));

   pfd.fd      = g_drm_fd;
   pfd.events  = POLLIN;
   pfd.revents = 0;
   CHECK(poll(&pfd, 1, 0) == 1, "the page flip's event is gone from the master");
   {
      char buf[256];
      struct drm_event e;
      ssize_t len = read(g_drm_fd, buf, sizeof(buf));
      memset(&e, 0, sizeof(e));
      if (len >= (ssize_t)sizeof(e))
         memcpy(&e, buf, sizeof(e));
      CHECK(len == (ssize_t)sizeof(struct drm_event_vblank)
            && e.type == DRM_EVENT_FLIP_COMPLETE,
            "the master read %ld bytes, event %u, not its one page flip",
            (long)len, (unsigned)e.type);
   }
   printf("   ok   the master's page flip event was left to it, alone\n");

   /* ---- no vblank in time: the wait is bounded ---- */
   t0   = now_us();
   n    = dispserv_kms.wait_vblank(NULL, seen, 2000, &at);
   took = now_us() - t0;
   CHECK(took < 100000, "a 2 ms bound waited %ld us", (long)took);
   printf("   ok   a bound shorter than a period returns in %ld us\n", (long)took);

   /* ---- the display off: nothing to count, and no wait ---- */
   CHECK(drmModeSetCrtc(g_drm_fd, g_crtc_id, 0, 0, 0, NULL, 0, NULL) == 0,
         "CRTC off: %s", strerror(errno));
   t0   = now_us();
   n    = dispserv_kms.wait_vblank(NULL, seen + 8, 200000, &at);
   took = now_us() - t0;
   CHECK(!n && took < 100000, "CRTC off: count %u after %ld us", n, (long)took);
   printf("   ok   the CRTC off counts nothing\n");

   dispserv_kms.destroy(NULL);
   drmModeRmFB(g_drm_fd, fb);
   drmModeFreeConnector(conn);
   drmModeFreeResources(res);
   close(g_drm_fd);
   if (failures)
   {
      printf("FAIL kms_vblank_live_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS kms_vblank_live_test\n");
   return 0;
}
