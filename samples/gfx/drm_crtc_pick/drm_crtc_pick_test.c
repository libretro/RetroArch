/* gfx/common/drm_common.c against a fake kernel: a connector nothing
 * drives yet, as with a display plugged in after boot, gets an encoder
 * and a CRTC that is off rather than failing; a CRTC that was off is
 * switched off again on restore; a size the connector lacks falls back
 * to its default mode; which hotplugs restart the video driver; and
 * the flip wait that watches the hotplug socket at no extra poll. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>

#include "../../../gfx/common/drm_common.c"

void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

/* ---- the fake kernel ------------------------------------------------- */

#define FAKE_FD 7
#define MAX_OBJ 4

typedef struct
{
   uint32_t id;
   int lit;
} fake_crtc_t;

typedef struct
{
   uint32_t id;
   uint32_t possible_crtcs;
   uint32_t crtc_id;
} fake_encoder_t;

static fake_crtc_t    crtcs[MAX_OBJ];
static int            n_crtcs;
static fake_encoder_t encoders[MAX_OBJ];
static int            n_encoders;

typedef struct
{
   uint32_t id;
   int      connected;
   int      n_modes;
} fake_connector_t;

/* The connector under test is the first; the rest only carry status */
static fake_connector_t conns[MAX_OBJ];
static int              n_conns;
static uint32_t         conn_encoder_id;
static uint32_t         conn_encoders[MAX_OBJ];
static int              conn_n_encoders;
static drmModeModeInfo  conn_modes[MAX_OBJ];
static int              conn_n_modes;

static int      setcrtc_calls;
static uint32_t setcrtc_crtc, setcrtc_fb;
static int      setcrtc_count;
static int      setcrtc_mode_null;

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { \
   printf("[FAIL] " __VA_ARGS__); printf("\n"); failures++; } } while (0)

static void fake_reset(void)
{
   memset(crtcs, 0, sizeof(crtcs));
   memset(encoders, 0, sizeof(encoders));
   memset(conn_encoders, 0, sizeof(conn_encoders));
   memset(conn_modes, 0, sizeof(conn_modes));
   memset(conns, 0, sizeof(conns));
   n_crtcs = n_encoders = conn_n_encoders = conn_n_modes = 0;
   conns[0].id        = 90;
   conns[0].connected = 1;
   n_conns            = 1;
   conn_encoder_id   = 0;
   setcrtc_calls     = 0;
   setcrtc_crtc      = setcrtc_fb = 0;
   setcrtc_count     = -1;
   setcrtc_mode_null = 0;
}

static void add_crtc(uint32_t id, int lit)
{
   crtcs[n_crtcs].id  = id;
   crtcs[n_crtcs].lit = lit;
   n_crtcs++;
}

static void add_encoder(uint32_t id, uint32_t possible, uint32_t crtc_id)
{
   encoders[n_encoders].id             = id;
   encoders[n_encoders].possible_crtcs = possible;
   encoders[n_encoders].crtc_id        = crtc_id;
   n_encoders++;
}

static void add_mode(unsigned w, unsigned h, unsigned hz)
{
   drmModeModeInfo *m = &conn_modes[conn_n_modes++];
   m->hdisplay = (uint16_t)w;
   m->vdisplay = (uint16_t)h;
   m->htotal   = (uint16_t)w;
   m->vtotal   = (uint16_t)h;
   m->clock    = (uint32_t)(w * h * hz / 1000);
}

drmModeResPtr drmModeGetResources(int fd)
{
   int i;
   drmModeResPtr r = (drmModeResPtr)calloc(1, sizeof(*r));
   (void)fd;
   r->count_crtcs      = n_crtcs;
   r->crtcs            = (uint32_t*)calloc(MAX_OBJ, sizeof(uint32_t));
   r->count_encoders   = n_encoders;
   r->encoders         = (uint32_t*)calloc(MAX_OBJ, sizeof(uint32_t));
   r->count_connectors = n_conns;
   r->connectors       = (uint32_t*)calloc(MAX_OBJ, sizeof(uint32_t));
   for (i = 0; i < n_conns; i++)
      r->connectors[i] = conns[i].id;
   for (i = 0; i < n_crtcs; i++)
      r->crtcs[i] = crtcs[i].id;
   for (i = 0; i < n_encoders; i++)
      r->encoders[i] = encoders[i].id;
   return r;
}

void drmModeFreeResources(drmModeResPtr r)
{
   if (!r)
      return;
   free(r->crtcs);
   free(r->encoders);
   free(r->connectors);
   free(r);
}

drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t id)
{
   int i;
   drmModeConnectorPtr c = NULL;
   (void)fd;
   for (i = 0; i < n_conns; i++)
   {
      if (conns[i].id != id)
         continue;
      c = (drmModeConnectorPtr)calloc(1, sizeof(*c));
      c->connector_id = id;
      c->connection   = conns[i].connected
         ? DRM_MODE_CONNECTED : DRM_MODE_DISCONNECTED;
      c->encoders     = (uint32_t*)calloc(MAX_OBJ, sizeof(uint32_t));
      c->modes        = (drmModeModeInfo*)calloc(MAX_OBJ, sizeof(drmModeModeInfo));
      if (i == 0)
      {
         c->encoder_id     = conn_encoder_id;
         c->count_encoders = conn_n_encoders;
         memcpy(c->encoders, conn_encoders, sizeof(conn_encoders));
         c->count_modes    = conn_n_modes;
         memcpy(c->modes, conn_modes, sizeof(conn_modes));
      }
      else
         c->count_modes    = conns[i].n_modes;
   }
   return c;
}

/* Probes nothing, here as in the kernel */
drmModeConnectorPtr drmModeGetConnectorCurrent(int fd, uint32_t id)
{
   return drmModeGetConnector(fd, id);
}

void drmModeFreeConnector(drmModeConnectorPtr c)
{
   if (!c)
      return;
   free(c->encoders);
   free(c->modes);
   free(c);
}

drmModeEncoderPtr drmModeGetEncoder(int fd, uint32_t id)
{
   int i;
   (void)fd;
   for (i = 0; i < n_encoders; i++)
   {
      if (encoders[i].id == id)
      {
         drmModeEncoderPtr e = (drmModeEncoderPtr)calloc(1, sizeof(*e));
         e->encoder_id     = id;
         e->crtc_id        = encoders[i].crtc_id;
         e->possible_crtcs = encoders[i].possible_crtcs;
         return e;
      }
   }
   return NULL;
}

void drmModeFreeEncoder(drmModeEncoderPtr e) { free(e); }

drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t id)
{
   int i;
   (void)fd;
   for (i = 0; i < n_crtcs; i++)
   {
      if (crtcs[i].id == id)
      {
         drmModeCrtcPtr c = (drmModeCrtcPtr)calloc(1, sizeof(*c));
         c->crtc_id    = id;
         c->mode_valid = crtcs[i].lit;
         if (crtcs[i].lit)
         {
            c->buffer_id     = 500 + id;
            c->mode.hdisplay = 1024;
            c->mode.vdisplay = 768;
         }
         return c;
      }
   }
   return NULL;
}

void drmModeFreeCrtc(drmModeCrtcPtr c) { free(c); }

int drmModeSetCrtc(int fd, uint32_t crtc_id, uint32_t buffer_id,
      uint32_t x, uint32_t y, uint32_t *connectors, int count,
      drmModeModeInfoPtr mode)
{
   (void)fd; (void)x; (void)y; (void)connectors;
   setcrtc_calls++;
   setcrtc_crtc      = crtc_id;
   setcrtc_fb        = buffer_id;
   setcrtc_count     = count;
   setcrtc_mode_null = mode == NULL;
   return 0;
}

static int flips_handled;

/* Reads the flip off the card, as the kernel's event read would */
int drmHandleEvent(int fd, drmEventContextPtr evctx)
{
   char c;
   (void)evctx;
   if (read(fd, &c, 1) == 1)
      flips_handled++;
   return 0;
}

static int polls;

int __real_poll(struct pollfd *fds, nfds_t n, int timeout);
int __wrap_poll(struct pollfd *fds, nfds_t n, int timeout)
{
   polls++;
   return __real_poll(fds, n, timeout);
}

static void *flip_later(void *fd)
{
   usleep(50 * 1000);
   if (write(*(int*)fd, "f", 1) != 1)
      return NULL;
   return NULL;
}

static void check_flip_wait(void)
{
   int card[2], mon[2];
   short revents;
   char c;
   pthread_t thr;

   if (pipe(card) != 0 || pipe(mon) != 0)
   {
      CHECK(0, "pipes");
      return;
   }
   g_drm_fd         = card[0];
   g_drm_fds.fd     = card[0];
   g_drm_fds.events = POLLIN;

   /* No monitor: as drm_wait_flip() */
   revents = 0; polls = 0; flips_handled = 0;
   CHECK(write(card[1], "f", 1) == 1, "write");
   CHECK(drm_wait_flip_hotplug(-1, -1, &revents), "a flip with no monitor is seen");
   CHECK(polls == 1 && flips_handled == 1 && !revents, "one poll, no hotplug");

   /* Nothing waiting on the card: a frame that does not block */
   revents = 0; polls = 0;
   CHECK(!drm_wait_flip_hotplug(0, mon[0], &revents), "nothing ready, no flip");
   CHECK(polls == 1 && !revents, "one poll, nothing noted");

   /* A hotplug while not blocking is noted, the flip still pending */
   revents = 0; polls = 0;
   CHECK(write(mon[1], "h", 1) == 1, "write");
   CHECK(!drm_wait_flip_hotplug(0, mon[0], &revents), "a hotplug is not a flip");
   CHECK(polls == 1 && (revents & POLLIN), "noted in the same poll");
   CHECK(read(mon[0], &c, 1) == 1, "the event is left for the caller");

   /* A hotplug while blocked: noted, and the wait goes on for the flip
    * without spinning on the unread socket */
   revents = 0; polls = 0; flips_handled = 0;
   CHECK(write(mon[1], "h", 1) == 1, "write");
   pthread_create(&thr, NULL, flip_later, &card[1]);
   CHECK(drm_wait_flip_hotplug(-1, mon[0], &revents), "the flip after a hotplug is seen");
   pthread_join(thr, NULL);
   CHECK(flips_handled == 1 && (revents & POLLIN), "flip handled, hotplug noted");
   CHECK(polls == 2, "one poll for the hotplug, one for the flip, got %d", polls);
   CHECK(read(mon[0], &c, 1) == 1, "the event is still there to read");

   /* Both at once: one poll */
   revents = 0; polls = 0; flips_handled = 0;
   CHECK(write(mon[1], "h", 1) == 1 && write(card[1], "f", 1) == 1, "write");
   CHECK(drm_wait_flip_hotplug(-1, mon[0], &revents), "flip seen");
   CHECK(polls == 1 && flips_handled == 1 && (revents & POLLIN), "both in one poll");
   CHECK(read(mon[0], &c, 1) == 1, "read");

   /* A dead socket is reported, not spun on */
   revents = 0; polls = 0; flips_handled = 0;
   close(mon[1]);
   pthread_create(&thr, NULL, flip_later, &card[1]);
   CHECK(drm_wait_flip_hotplug(-1, mon[0], &revents), "the flip after a hangup is seen");
   pthread_join(thr, NULL);
   CHECK((revents & POLLHUP) && polls == 2, "hangup reported in two polls, got %d", polls);

   close(mon[0]);
   close(card[0]);
   close(card[1]);
   memset(&g_drm_fds, 0, sizeof(g_drm_fds));
}

/* ---- the frontend's sequence ----------------------------------------- */

static bool bring_up(void)
{
   g_drm_fd = FAKE_FD;
   if (     !drm_get_resources(FAKE_FD)
         || !drm_get_connector(FAKE_FD, 0)
         || !drm_get_encoder(FAKE_FD))
   {
      drm_free();
      return false;
   }
   drm_setup(FAKE_FD);
   return true;
}

static void tear_down(void)
{
   drm_restore_crtc();
   drm_free();
}

int main(void)
{
   drmModeModeInfo *m;

   /* Plugged in after boot: no encoder, the other head lights CRTC 41 */
   fake_reset();
   add_crtc(41, 1);
   add_crtc(42, 0);
   add_encoder(61, 0x3, 41);
   add_encoder(62, 0x3, 0);
   conn_encoders[conn_n_encoders++] = 62;
   add_mode(1920, 1080, 60);
   CHECK(bring_up(), "an undriven connector must come up");
   CHECK(g_crtc_id == 42, "an undriven connector takes the CRTC that is off, got %u", g_crtc_id);
   CHECK(g_orig_crtc && !g_orig_crtc->mode_valid, "the CRTC taken was off");
   setcrtc_calls = 0;
   tear_down();
   CHECK(setcrtc_calls == 1 && setcrtc_crtc == 42, "restore touches the CRTC taken");
   CHECK(setcrtc_fb == 0 && setcrtc_count == 0 && setcrtc_mode_null,
         "a CRTC that was off is switched off again (fb %u, %d connectors)",
         setcrtc_fb, setcrtc_count);

   /* The first encoder reaches only a lit CRTC, the second a free one */
   fake_reset();
   add_crtc(41, 1);
   add_crtc(42, 0);
   add_encoder(61, 0x1, 0);
   add_encoder(62, 0x2, 0);
   conn_encoders[conn_n_encoders++] = 61;
   conn_encoders[conn_n_encoders++] = 62;
   add_mode(1920, 1080, 60);
   CHECK(bring_up(), "a connector with two encoders must come up");
   CHECK(g_crtc_id == 42, "the encoder with a free CRTC wins, got %u", g_crtc_id);
   tear_down();

   /* Every CRTC lit: take one rather than fail */
   fake_reset();
   add_crtc(41, 1);
   add_encoder(61, 0x1, 41);
   add_encoder(62, 0x1, 0);
   conn_encoders[conn_n_encoders++] = 62;
   add_mode(1920, 1080, 60);
   CHECK(bring_up(), "with no free CRTC the connector still comes up");
   CHECK(g_crtc_id == 41, "the only CRTC is taken, got %u", g_crtc_id);
   setcrtc_calls = 0;
   tear_down();
   CHECK(setcrtc_calls == 1 && setcrtc_fb == 541 && setcrtc_count == 1 && !setcrtc_mode_null,
         "a CRTC that was lit gets its framebuffer and mode back");

   /* Already driven: its own CRTC, nothing scanned */
   fake_reset();
   add_crtc(41, 0);
   add_crtc(42, 1);
   add_encoder(61, 0x3, 42);
   conn_encoder_id = 61;
   conn_encoders[conn_n_encoders++] = 61;
   add_mode(1920, 1080, 60);
   CHECK(bring_up(), "a driven connector must come up");
   CHECK(g_crtc_id == 42, "a driven connector keeps its CRTC, got %u", g_crtc_id);
   tear_down();

   /* Nothing reachable */
   fake_reset();
   add_crtc(41, 0);
   conn_encoders[conn_n_encoders++] = 99;
   add_mode(1920, 1080, 60);
   CHECK(!bring_up(), "no encoder still fails");

   /* Modes */
   fake_reset();
   add_mode(640, 480, 60);
   add_mode(1920, 1080, 50);
   add_mode(1920, 1080, 60);
   {
      drmModeConnector *c = drmModeGetConnector(FAKE_FD, 90);
      m = drm_find_mode(c, 1920, 1080, 1.0f, 59.94f);
      CHECK(m == &c->modes[2], "the rate closest to the request wins");
      m = drm_find_mode(c, 1920, 1080, 0.5f, 25.0f);
      CHECK(m == &c->modes[1], "black frame insertion scales the rate");
      m = drm_find_mode(c, 3840, 2160, 1.0f, 60.0f);
      CHECK(m == &c->modes[0], "a size the display lacks falls back to mode 0");
      c->count_modes = 0;
      CHECK(!drm_find_mode(c, 640, 480, 1.0f, 60.0f), "no modes, no mode");
      CHECK(!drm_find_mode(NULL, 640, 480, 1.0f, 60.0f), "no connector, no mode");
      drmModeFreeConnector(c);
   }

   /* Hotplug, with the display in use on connector 90 */
   {
      bool lost = false;

      fake_reset();
      add_mode(1920, 1080, 60);
      conns[1].id = 91; conns[1].connected = 0; conns[1].n_modes = 0;
      n_conns     = 2;
      g_connector_id = 90;

      conns[1].connected = 1; conns[1].n_modes = 2;
      CHECK(!drm_hotplug_needs_restart(FAKE_FD, 0, &lost),
            "a second display plugged in leaves the output where it is");
      CHECK(!drm_hotplug_needs_restart(FAKE_FD, 2, &lost),
            "even when it is the one the monitor index names");

      conns[0].connected = 0;
      CHECK(drm_hotplug_needs_restart(FAKE_FD, 0, &lost),
            "the display in use unplugged moves to the one left");
      CHECK(!lost, "a move is not a loss");

      /* Only display unplugged: nowhere to go */
      g_connector_id     = 90;
      conns[1].connected = 0;
      CHECK(!drm_hotplug_needs_restart(FAKE_FD, 0, &lost),
            "no display left, no restart");
      CHECK(lost, "the loss is remembered");
      CHECK(!drm_hotplug_needs_restart(FAKE_FD, 0, &lost),
            "nor on a second event while still gone");

      conns[0].connected = 1;
      CHECK(drm_hotplug_needs_restart(FAKE_FD, 0, &lost),
            "a display back on the port restarts for its own mode");
      CHECK(!lost, "and clears the loss");
      CHECK(!drm_hotplug_needs_restart(FAKE_FD, 0, &lost),
            "a later unrelated event changes nothing");

      /* Connected but no modes yet does not count */
      conns[0].connected = 0;
      conns[1].connected = 1; conns[1].n_modes = 0;
      CHECK(!drm_hotplug_needs_restart(FAKE_FD, 0, &lost),
            "a connector without modes is not a display to move to");

      /* The index names a display that is gone */
      lost = false;
      conns[0].connected = 0;
      conns[1].connected = 1; conns[1].n_modes = 2;
      CHECK(!drm_hotplug_needs_restart(FAKE_FD, 2, &lost),
            "an index past the displays left does not restart");
   }

   check_flip_wait();

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] drm_crtc_pick_test\n");
   return 0;
}
