/* gfx/common/drm_vblank.h as a compositor's client uses it, on a real
 * kernel (vkms; run_vm.sh): a second opener of the card, not its
 * master, finds the CRTC behind a connector by the connector's name
 * and reads that CRTC's vblanks while the master drives it. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#define DRM_VBLANK_CONNECTOR
#include "../../../gfx/common/drm_vblank.h"

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
   drm_vblank_t vb        = { -1, 0, false };
   drm_vblank_t none      = { -1, 0, false };
   drmModeRes *res;
   drmModeConnector *conn = NULL;
   drmModeModeInfo *mode;
   retro_time_t at        = 0;
   retro_time_t prev_at, first_at;
   uint32_t fb            = 0;
   uint32_t conn_id, crtc_id;
   int64_t period, t0, took;
   unsigned code, n, seen, first, i, late;
   int master, c;

   master = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
   if (master < 0)
   {
      printf("FAIL no /dev/dri/card0: %s\n", strerror(errno));
      return 1;
   }
   res = drmModeGetResources(master);
   if (!res || !res->count_crtcs)
   {
      printf("FAIL no CRTC\n");
      return 1;
   }
   for (c = 0; c < res->count_connectors && !conn; c++)
   {
      conn = drmModeGetConnector(master, res->connectors[c]);
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
   conn_id = conn->connector_id;
   crtc_id = res->crtcs[0];
   mode    = &conn->modes[0];
   period  = (int64_t)mode->htotal * mode->vtotal * 1000 / mode->clock;
   code    = DRM_VBLANK_CONNECTOR_CODE(conn->connector_type,
         conn->connector_type_id);

   memset(&dumb, 0, sizeof(dumb));
   dumb.width  = mode->hdisplay;
   dumb.height = mode->vdisplay;
   dumb.bpp    = 32;
   if (     drmIoctl(master, DRM_IOCTL_MODE_CREATE_DUMB, &dumb) != 0
         || drmModeAddFB(master, dumb.width, dumb.height, 24, 32,
               dumb.pitch, dumb.handle, &fb) != 0
         || drmModeSetCrtc(master, crtc_id, fb, 0, 0, &conn_id, 1, mode) != 0)
   {
      printf("FAIL mode set: %s\n", strerror(errno));
      return 1;
   }

   /* ---- found by its name's two halves, by one who is not master ---- */
   CHECK(drm_vblank_open_connector(&vb, code),
         "no clock for connector type %u number %u",
         (unsigned)conn->connector_type, (unsigned)conn->connector_type_id);
   if (vb.fd < 0)
      return 1;
   CHECK(vb.crtc == crtc_id, "CRTC %u found, %u drives it",
         (unsigned)vb.crtc, (unsigned)crtc_id);
   CHECK(vb.fd != master && !drmIsMaster(vb.fd),
         "the clock's file description is the master's");
   CHECK(!drm_vblank_open_connector(&none,
            DRM_VBLANK_CONNECTOR_CODE(conn->connector_type,
               conn->connector_type_id + 7)) && none.fd < 0,
         "a clock for a connector that is not there");
   CHECK(!drm_vblank_open_connector(&none,
            DRM_VBLANK_CONNECTOR_CODE(DRM_VBLANK_CONNECTOR_HDMI, 1))
         && none.fd < 0, "a clock for an HDMI connector vkms has not got");
   printf("   ok   connector %u-%u is CRTC %u, to a client\n",
         (unsigned)conn->connector_type, (unsigned)conn->connector_type_id,
         (unsigned)vb.crtc);

   for (i = 0; i < 50 && !(seen = drm_vblank_wait(&vb, vb.fd, 0, 0, &at)); i++)
      usleep(20000);
   CHECK(seen, "no vblank counted on an active CRTC");
   CHECK(at > 0 && now_us() - at < 4 * period && at - now_us() < period,
         "the vblank's time is %ld us from now", (long)(now_us() - at));

   /* ---- each next vblank, counted and stamped on the CRTC's grid ---- */
   first    = seen;
   first_at = at;
   prev_at  = at;
   late     = 0;
   for (i = 0; i < 30; i++)
   {
      int64_t apart;
      n     = drm_vblank_wait(&vb, vb.fd, seen, 200000, &at);
      CHECK(n && n != seen && n - seen < 1000,
            "vblank %u: count %u after %u", i, n, seen);
      apart = (int64_t)(n - seen) * period;
      if (at - prev_at < apart - period / 4 || at - prev_at > apart + period / 4)
         late++;
      seen    = n;
      prev_at = at;
   }
   CHECK(seen - first >= 30
         && at - first_at > (int64_t)(seen - first) * period - period / 4
         && at - first_at < (int64_t)(seen - first) * period + period / 4,
         "%u vblanks counted over %ld us of stamps at a %ld us period",
         seen - first, (long)(at - first_at), (long)period);
   CHECK(!late, "%u of 30 vblank stamps were off the CRTC's grid", late);
   printf("   ok   %u vblanks over 30 waits\n", seen - first);

   /* ---- bounded, and nothing once the master turns the CRTC off ---- */
   t0   = now_us();
   n    = drm_vblank_wait(&vb, vb.fd, seen, 2000, &at);
   took = now_us() - t0;
   CHECK(took < 100000, "a 2 ms bound waited %ld us", (long)took);
   CHECK(drmModeSetCrtc(master, crtc_id, 0, 0, 0, NULL, 0, NULL) == 0,
         "CRTC off: %s", strerror(errno));
   t0   = now_us();
   n    = drm_vblank_wait(&vb, vb.fd, seen + 8, 200000, &at);
   took = now_us() - t0;
   CHECK(!n && took < 100000, "CRTC off: count %u after %ld us", n, (long)took);
   printf("   ok   bounded, and the CRTC off counts nothing\n");

   close(vb.fd);
   drmModeRmFB(master, fb);
   drmModeFreeConnector(conn);
   drmModeFreeResources(res);
   close(master);
   if (failures)
   {
      printf("FAIL drm_client_vblank_live_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("PASS drm_client_vblank_live_test\n");
   return 0;
}
