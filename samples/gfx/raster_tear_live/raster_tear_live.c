/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (raster_tear_live.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Live check for the KMS beam estimate - the one dispserv_kms.c makes
 * through video_display_server_scanline_from_time() - and for async
 * page flips. It needs a display and DRM master, so run it from a text
 * console with no compositor on the card:
 *
 *   ./raster_tear_live -l                 list connectors and modes
 *   ./raster_tear_live [-d /dev/dri/cardN] [-c connector] [-m WxH@Hz]
 *                      [-o frames.csv]
 *
 * Each refresh flips to black in blanking, then to white without
 * waiting for vblank once the estimate puts the beam on the target
 * line. Red ticks at both edges mark that line; after a second, green
 * ticks mark the line the white flips had landed by (the median). With
 * a right estimate the edge sits between red and green, a little below
 * green at most (the display fetches ahead), and about as far below
 * red at every target. An edge above red, clearly below green, or an
 * offset that grows or shrinks with the target means the estimate is
 * wrong. A flickering white band at the top means black flips miss
 * blanking; the summary's count of these is only an upper bound (it
 * reads the line after the flip has already latched), so the band on
 * screen is the real sign - at 3840x2160@120 blanking is about
 * 0.33-0.37 ms, close to the async commit latency, so the count can be
 * high with no band. The target steps through 25%, 50% and 75% of the
 * height, ten seconds each, with a summary line after each and a line
 * on how steady the vblank stamps behind the estimate were. Ctrl+C
 * stops early and restores the console. -o writes one CSV row per
 * frame - the lines, the stamps each white request came from and the
 * clock around its flip - once the console is restored.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "../../../gfx/video_display_server.h"

#define STEP_SECONDS   10
#define LATE_LINES     2
#define LANDED_SAMPLES 512 /* the first second at up to 500 Hz */
#define LOG_ROWS       (3 * STEP_SECONDS * 500 + 4096)
#define COLOUR_BLACK   0x00000000u
#define COLOUR_WHITE   0x00ffffffu
#define COLOUR_TICK    0x00ff0000u
#define COLOUR_LANDED  0x0000ff00u

typedef struct
{
   uint32_t  handle;
   uint32_t  pitch;
   uint64_t  size;
   uint32_t  fb_id;
   uint32_t *pixels;
} dumb_fb_t;

/* One -o row; the clocks are absolute CLOCK_MONOTONIC ns */
typedef struct
{
   uint64_t white_req_seq;
   uint64_t white_req_line0_ns;
   uint64_t white_req_now_ns;
   uint64_t white_req_clock_ns;
   uint64_t white_event_clock_ns;
   unsigned step_target;
   unsigned frame;
   int      black_req_line;
   int      black_event_line;
   int      white_req_line;
   int      white_event_line;
   int      landed_offset;
} frame_row_t;

static volatile sig_atomic_t s_stop;
static int s_flip_pending;

static void on_signal(int sig)
{
   (void)sig;
   s_stop = 1;
}

static uint64_t now_ns(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

/* kms_display_server_get_scanline()'s estimate: the vblank time, then
 * the clock. The outs, each may be NULL, get what the line came from. */
static int beam_line(int fd, uint32_t crtc_id, uint64_t frame_ns,
      unsigned vtotal, uint64_t *seq_out, uint64_t *line0_out,
      uint64_t *now_out)
{
   uint64_t seq, line0_ns, now;
   if (drmCrtcGetSequence(fd, crtc_id, &seq, &line0_ns) != 0 || !line0_ns)
      return -1;
   now = now_ns();
   if (seq_out)
      *seq_out = seq;
   if (line0_out)
      *line0_out = line0_ns;
   if (now_out)
      *now_out = now;
   return video_display_server_scanline_from_time(
         (int64_t)(now - line0_ns), frame_ns, vtotal);
}

static int cmp_int(const void *a, const void *b)
{
   int x = *(const int*)a;
   int y = *(const int*)b;
   return (x > y) - (x < y);
}

static void on_flip(int fd, unsigned frame, unsigned sec,
      unsigned usec, void *data)
{
   (void)fd; (void)frame; (void)sec; (void)usec; (void)data;
   s_flip_pending = 0;
}

static int flip_async(int fd, uint32_t crtc_id, uint32_t fb_id)
{
   s_flip_pending = 1;
   if (drmModePageFlip(fd, crtc_id, fb_id,
         DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_PAGE_FLIP_ASYNC, NULL) == 0)
      return 0;
   s_flip_pending = 0;
   return -errno;
}

static int wait_flip(int fd)
{
   drmEventContext ev;
   struct pollfd pfd;

   memset(&ev, 0, sizeof(ev));
   ev.version           = 2;
   ev.page_flip_handler = on_flip;
   pfd.fd               = fd;
   pfd.events           = POLLIN;

   /* Not given up on s_stop: a flip completes within microseconds */
   while (s_flip_pending)
   {
      int r = poll(&pfd, 1, 1000);
      if (r < 0 && errno == EINTR)
         continue;
      if (r == 0)
      {
         fputs("No flip event within a second.\n", stderr);
         return -1;
      }
      if (     r < 0
            || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            || drmHandleEvent(fd, &ev) < 0)
      {
         fputs("Waiting for the flip event failed.\n", stderr);
         return -1;
      }
   }
   return 0;
}

static int create_fb(int fd, unsigned w, unsigned h, dumb_fb_t *fb)
{
   struct drm_mode_create_dumb create;
   struct drm_mode_map_dumb    map;
   void *p;

   memset(&create, 0, sizeof(create));
   create.width  = w;
   create.height = h;
   create.bpp    = 32;
   if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0)
      return -1;
   fb->handle = create.handle;
   fb->pitch  = create.pitch;
   fb->size   = create.size;

   if (drmModeAddFB(fd, w, h, 24, 32, fb->pitch, fb->handle, &fb->fb_id) != 0)
      return -1;

   memset(&map, 0, sizeof(map));
   map.handle = fb->handle;
   if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0)
      return -1;
   p = mmap(NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);
   if (p == MAP_FAILED)
      return -1;
   fb->pixels = (uint32_t*)p;
   return 0;
}

static void destroy_fb(int fd, dumb_fb_t *fb)
{
   struct drm_mode_destroy_dumb destroy;

   if (fb->pixels)
      munmap(fb->pixels, fb->size);
   if (fb->fb_id)
      drmModeRmFB(fd, fb->fb_id);
   if (fb->handle)
   {
      memset(&destroy, 0, sizeof(destroy));
      destroy.handle = fb->handle;
      drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
   }
}

static void fill_rect(dumb_fb_t *fb, unsigned w, unsigned y0, unsigned y1,
      unsigned x0, unsigned x1, uint32_t colour)
{
   unsigned x, y;
   for (y = y0; y < y1; y++)
   {
      uint32_t *row = fb->pixels + (size_t)y * (fb->pitch / 4);
      for (x = x0; x < x1 && x < w; x++)
         row[x] = colour;
   }
}

/* Three rows centred on line, across the outer twentieth of each side */
static void paint_ticks(dumb_fb_t *fb, unsigned w, unsigned h,
      unsigned line, uint32_t colour)
{
   unsigned y0 = line ? line - 1 : 0;
   unsigned y1 = (line + 2 < h) ? line + 2 : h;
   fill_rect(fb, w, y0, y1, 0, w / 20, colour);
   fill_rect(fb, w, y0, y1, w - w / 20, w, colour);
}

static void list_card(int fd, const char *path)
{
   drmModeRes *res = drmModeGetResources(fd);
   int i, j;

   if (!res)
      return;
   /* The cached state: listing never probes, so it is safe beside a
    * running compositor */
   for (i = 0; i < res->count_connectors; i++)
   {
      drmModeConnector *c = drmModeGetConnectorCurrent(fd, res->connectors[i]);
      if (!c)
         continue;
      if (c->connection == DRM_MODE_CONNECTED)
      {
         printf("%s connector %u:\n", path, c->connector_id);
         for (j = 0; j < c->count_modes; j++)
         {
            drmModeModeInfo *m = &c->modes[j];
            printf("  %ux%u@%u%s%s (%.4f Hz, vtotal %u)\n",
                  m->hdisplay, m->vdisplay, m->vrefresh,
                  (m->flags & DRM_MODE_FLAG_INTERLACE) ? "i" : "",
                  (m->type & DRM_MODE_TYPE_PREFERRED) ? " preferred" : "",
                  m->clock * 1000.0 / ((double)m->htotal * m->vtotal),
                  m->vtotal);
         }
      }
      drmModeFreeConnector(c);
   }
   drmModeFreeResources(res);
}

static drmModeConnector *pick_connector(int fd, drmModeRes *res,
      uint32_t want)
{
   int i;
   for (i = 0; i < res->count_connectors; i++)
   {
      drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
      if (!c)
         continue;
      if (     c->connection == DRM_MODE_CONNECTED
            && c->count_modes > 0
            && (!want || c->connector_id == want))
         return c;
      drmModeFreeConnector(c);
   }
   return NULL;
}

static uint32_t pick_crtc(int fd, drmModeRes *res, drmModeConnector *conn)
{
   drmModeEncoder *enc;
   int i, j;

   if (conn->encoder_id && (enc = drmModeGetEncoder(fd, conn->encoder_id)))
   {
      uint32_t crtc_id = enc->crtc_id;
      drmModeFreeEncoder(enc);
      if (crtc_id)
         return crtc_id;
   }
   /* Only an idle CRTC: an active one drives another monitor */
   for (i = 0; i < conn->count_encoders; i++)
   {
      if (!(enc = drmModeGetEncoder(fd, conn->encoders[i])))
         continue;
      for (j = 0; j < res->count_crtcs; j++)
      {
         drmModeCrtc *crtc;
         int idle;

         if (     !(enc->possible_crtcs & (1u << j))
               || !(crtc = drmModeGetCrtc(fd, res->crtcs[j])))
            continue;
         idle = !crtc->mode_valid;
         drmModeFreeCrtc(crtc);
         if (idle)
         {
            drmModeFreeEncoder(enc);
            return res->crtcs[j];
         }
      }
      drmModeFreeEncoder(enc);
   }
   return 0;
}

/* The modes kms_display_server_get_scanline() can time */
static int mode_timed(const drmModeModeInfo *m)
{
   return m->clock && m->htotal && m->vtotal
         && !(m->flags & (DRM_MODE_FLAG_INTERLACE | DRM_MODE_FLAG_DBLSCAN));
}

static drmModeModeInfo *pick_mode(drmModeConnector *conn,
      unsigned w, unsigned h, unsigned hz)
{
   drmModeModeInfo *first = NULL;
   int i;
   for (i = 0; i < conn->count_modes; i++)
   {
      drmModeModeInfo *m = &conn->modes[i];
      if (!mode_timed(m))
         continue;
      if (!first)
         first = m;
      if (w)
      {
         if (     m->hdisplay == w && m->vdisplay == h
               && (!hz || m->vrefresh == hz))
            return m;
      }
      else if (m->type & DRM_MODE_TYPE_PREFERRED)
         return m;
   }
   return w ? NULL : first;
}

/* VRR_ENABLED on the CRTC; absent reads as off */
static int crtc_vrr_enabled(int fd, uint32_t crtc_id)
{
   drmModeObjectProperties *props = drmModeObjectGetProperties(fd, crtc_id,
         DRM_MODE_OBJECT_CRTC);
   uint32_t i;
   int on = 0;

   if (!props)
      return 0;
   for (i = 0; i < props->count_props; i++)
   {
      drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
      if (!prop)
         continue;
      if (!strcmp(prop->name, "VRR_ENABLED"))
         on = props->prop_values[i] != 0;
      drmModeFreeProperty(prop);
   }
   drmModeFreeObjectProperties(props);
   return on;
}

static int write_log(FILE *f, const frame_row_t *rows, size_t n,
      unsigned dropped)
{
   size_t i;

   fputs("step_target,frame,black_req_line,black_event_line,"
         "white_req_line,white_req_seq,white_req_line0_ns,white_req_now_ns,"
         "white_req_clock_ns,white_event_clock_ns,white_event_line,"
         "landed_offset\n", f);
   for (i = 0; i < n; i++)
   {
      const frame_row_t *r = &rows[i];
      fprintf(f, "%u,%u,%d,%d,%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%"
            PRIu64 ",%" PRIu64 ",%d,%d\n",
            r->step_target, r->frame, r->black_req_line,
            r->black_event_line, r->white_req_line, r->white_req_seq,
            r->white_req_line0_ns, r->white_req_now_ns,
            r->white_req_clock_ns, r->white_event_clock_ns,
            r->white_event_line, r->landed_offset);
   }
   if (dropped)
      fprintf(f, "# row buffer full: the last %u frames were not "
            "recorded\n", dropped);
   return ferror(f) ? -1 : 0;
}

int main(int argc, char **argv)
{
   const char *card       = NULL;
   const char *log_path   = NULL;
   FILE *log_file         = NULL;
   frame_row_t *rows      = NULL;
   size_t rows_n          = 0;
   unsigned rows_dropped  = 0;
   uint32_t want_conn     = 0;
   unsigned want_w        = 0;
   unsigned want_h        = 0;
   unsigned want_hz       = 0;
   int list               = 0;
   int fd                 = -1;
   int ret                = 1;
   int opt, i, step;
   char path[64];
   uint64_t cap           = 0;
   uint64_t frame_ns      = 0;
   uint32_t crtc_id       = 0;
   drmModeRes *res        = NULL;
   drmModeConnector *conn = NULL;
   drmModeCrtc *saved     = NULL;
   drmModeModeInfo *mode  = NULL;
   dumb_fb_t black, white;

   memset(&black, 0, sizeof(black));
   memset(&white, 0, sizeof(white));

   while ((opt = getopt(argc, argv, "ld:c:m:o:")) != -1)
   {
      switch (opt)
      {
         case 'l':
            list = 1;
            break;
         case 'd':
            card = optarg;
            break;
         case 'c':
            want_conn = (uint32_t)strtoul(optarg, NULL, 10);
            break;
         case 'm':
            if (sscanf(optarg, "%ux%u@%u", &want_w, &want_h, &want_hz) < 2)
            {
               fputs("-m takes WxH or WxH@Hz\n", stderr);
               return 1;
            }
            break;
         case 'o':
            log_path = optarg;
            break;
         default:
            fprintf(stderr, "usage: %s [-l] [-d /dev/dri/cardN] "
                  "[-c connector] [-m WxH@Hz] [-o PATH]\n", argv[0]);
            return 1;
      }
   }

   /* Before the card is opened, so a bad path never touches the display.
    * The rows are faulted in now, not inside the timed loop, by a
    * non-zero fill: a zero one can be folded into a lazy calloc. */
   if (log_path && !list)
   {
      if (!(log_file = fopen(log_path, "w")))
      {
         fprintf(stderr, "Could not open %s: %s\n", log_path,
               strerror(errno));
         return 1;
      }
      if (!(rows = malloc(LOG_ROWS * sizeof(*rows))))
      {
         fputs("Could not allocate the log rows.\n", stderr);
         fclose(log_file);
         return 1;
      }
      memset(rows, 0xff, LOG_ROWS * sizeof(*rows));
   }

   /* The given card, or the first with a connected connector */
   for (i = 0; i < 8 && !conn; i++)
   {
      if (card)
      {
         if (i)
            break;
         snprintf(path, sizeof(path), "%s", card);
      }
      else
         snprintf(path, sizeof(path), "/dev/dri/card%d", i);

      if ((fd = open(path, O_RDWR | O_CLOEXEC)) < 0)
         continue;
      if (list)
      {
         list_card(fd, path);
         close(fd);
         fd = -1;
         continue;
      }
      /* Opening a card no one holds made us master */
      if (!drmIsMaster(fd))
      {
         fprintf(stderr, "%s is held by another process (a compositor?). "
               "Run this from a text console.\n", path);
         close(fd);
         free(rows);
         if (log_file)
            fclose(log_file);
         return 1;
      }
      if ((res = drmModeGetResources(fd)))
      {
         if ((conn = pick_connector(fd, res, want_conn)))
            break;
         drmModeFreeResources(res);
         res = NULL;
      }
      close(fd);
      fd = -1;
   }
   if (list)
      return 0;
   if (!conn)
   {
      fprintf(stderr, "No connected connector%s found.\n",
            want_conn ? " with that id" : "");
      free(rows);
      if (log_file)
         fclose(log_file);
      return 1;
   }

   if (drmGetCap(fd, DRM_CAP_ASYNC_PAGE_FLIP, &cap) != 0 || !cap)
   {
      fprintf(stderr, "%s has no async page flips.\n", path);
      goto out;
   }
   if (     !(mode = pick_mode(conn, want_w, want_h, want_hz))
         || !mode->clock)
   {
      if (want_w)
         fprintf(stderr, "Connector %u has no %ux%u@%u mode; -l lists "
               "them.\n", conn->connector_id, want_w, want_h, want_hz);
      else
         fprintf(stderr, "Connector %u has no usable mode.\n",
               conn->connector_id);
      goto out;
   }
   if (!(crtc_id = pick_crtc(fd, res, conn)))
   {
      fprintf(stderr, "No CRTC for connector %u.\n", conn->connector_id);
      goto out;
   }
   if (crtc_vrr_enabled(fd, crtc_id))
   {
      fputs("VRR is on for this CRTC, so the refresh isn't fixed and this "
            "test can't judge the estimate. Turn variable refresh "
            "(Adaptive Sync / FreeSync) off in your compositor's display "
            "settings and rerun.\n", stderr);
      goto out;
   }

   frame_ns = (uint64_t)mode->htotal * mode->vtotal * 1000000u / mode->clock;
   printf("%s connector %u, CRTC %u: %ux%u, htotal %u, vtotal %u, "
         "%u kHz: %.4f Hz, %.3f us a line\n",
         path, conn->connector_id, crtc_id, mode->hdisplay, mode->vdisplay,
         mode->htotal, mode->vtotal, mode->clock,
         1e9 / (double)frame_ns, (double)frame_ns / mode->vtotal / 1000.0);

   if (!(saved = drmModeGetCrtc(fd, crtc_id)))
   {
      fprintf(stderr, "Could not read CRTC %u to restore it after: %s\n",
            crtc_id, strerror(errno));
      goto out;
   }

   if (     create_fb(fd, mode->hdisplay, mode->vdisplay, &black) != 0
         || create_fb(fd, mode->hdisplay, mode->vdisplay, &white) != 0)
   {
      fprintf(stderr, "Could not make the buffers: %s\n", strerror(errno));
      goto out;
   }
   fill_rect(&black, mode->hdisplay, 0, mode->vdisplay, 0, mode->hdisplay,
         COLOUR_BLACK);
   fill_rect(&white, mode->hdisplay, 0, mode->vdisplay, 0, mode->hdisplay,
         COLOUR_WHITE);

   /* Before the mode set, so every way out restores the console.
    * Stopped, we would hold master with the test image up. */
   signal(SIGINT,  on_signal);
   signal(SIGTERM, on_signal);
   signal(SIGQUIT, on_signal);
   signal(SIGHUP,  on_signal);
   signal(SIGTSTP, SIG_IGN);
   signal(SIGPIPE, SIG_IGN);

   if (drmModeSetCrtc(fd, crtc_id, black.fb_id, 0, 0,
         &conn->connector_id, 1, mode) != 0)
   {
      fprintf(stderr, "Mode set failed: %s. Is a compositor holding the "
            "card? Run this from a text console.\n", strerror(errno));
      goto out;
   }

   for (step = 1; step <= 3 && !s_stop; step++)
   {
      unsigned target     = mode->vdisplay * step / 4;
      unsigned frames     = 0;
      unsigned late       = 0;
      unsigned missed     = 0;
      unsigned first_n    = 0;
      int worst           = 0;
      int missed_worst    = 0;
      int landed_min      = (int)mode->vtotal;
      int landed_max      = 0;
      int landed_line     = -1;
      int landed_offset   = 0;
      uint64_t landed_sum = 0;
      uint64_t event_ns   = 0;
      uint64_t event_max  = 0;
      unsigned stamp_n    = 0;
      unsigned stamp_gaps = 0;
      unsigned stamp_back = 0;
      unsigned stamp_same = 0;
      int64_t stamp_sum   = 0;
      int64_t stamp_min   = 0;
      int64_t stamp_max   = 0;
      uint64_t prev_seq   = 0;
      uint64_t prev_line0 = 0;
      uint64_t step_start, step_end;
      int first[LANDED_SAMPLES];

      paint_ticks(&black, mode->hdisplay, mode->vdisplay, target, COLOUR_TICK);
      paint_ticks(&white, mode->hdisplay, mode->vdisplay, target, COLOUR_TICK);
      step_start = now_ns();
      step_end   = step_start + (uint64_t)STEP_SECONDS * 1000000000u;

      while (!s_stop && now_ns() < step_end)
      {
         int line;
         int landed;
         int err;
         int black_req, black_event, event_line;
         uint64_t req_seq   = 0;
         uint64_t req_line0 = 0;
         uint64_t req_now   = 0;
         uint64_t t0, t1, dt;

         /* Black, in blanking */
         do
            line = beam_line(fd, crtc_id, frame_ns, mode->vtotal,
                  NULL, NULL, NULL);
         while (!s_stop && line >= 0 && line < (int)mode->vdisplay);
         if (line < 0)
         {
            fputs("Reading the beam failed.\n", stderr);
            goto out;
         }
         if (s_stop)
            break;
         if ((err = flip_async(fd, crtc_id, black.fb_id)) != 0)
         {
            fprintf(stderr, "Async flip refused: %s\n", strerror(-err));
            goto out;
         }
         if (wait_flip(fd) != 0)
            goto out;
         black_req = line;

         /* Asked for in blanking, so a visible line is the next frame's */
         line = beam_line(fd, crtc_id, frame_ns, mode->vtotal,
               &req_seq, &req_line0, &req_now);
         black_event = line;
         if (line >= 0 && line < (int)mode->vdisplay)
         {
            missed++;
            if (line > missed_worst)
               missed_worst = line;
         }

         /* White, from the target line of the next frame down */
         while (!s_stop && line >= 0
               && !(line >= (int)target && line < (int)mode->vdisplay))
            line = beam_line(fd, crtc_id, frame_ns, mode->vtotal,
                  &req_seq, &req_line0, &req_now);
         if (line < 0)
         {
            fputs("Reading the beam failed.\n", stderr);
            goto out;
         }
         if (s_stop)
            break;

         t0 = now_ns();
         if ((err = flip_async(fd, crtc_id, white.fb_id)) != 0)
         {
            fprintf(stderr, "Async flip refused: %s\n", strerror(-err));
            goto out;
         }
         if (wait_flip(fd) != 0)
            goto out;
         t1 = now_ns();
         dt = t1 - t0;

         /* The line by which it landed; the event trails the latch */
         if ((landed = beam_line(fd, crtc_id, frame_ns, mode->vtotal,
               NULL, NULL, NULL)) < 0)
         {
            fputs("Reading the beam failed.\n", stderr);
            goto out;
         }
         event_line = landed;
         landed -= (int)target;
         if (landed < 0)
            landed += (int)mode->vtotal;

         frames++;
         event_ns += dt;
         if (dt > event_max)
            event_max = dt;
         if (line - (int)target > worst)
            worst = line - (int)target;
         if (line - (int)target > LATE_LINES)
            late++;
         landed_sum += (unsigned)landed;
         if (landed < landed_min)
            landed_min = landed;
         if (landed > landed_max)
            landed_max = landed;
         if (landed_line < 0 && first_n < LANDED_SAMPLES)
            first[first_n++] = landed;

         /* The stamps each white request's line came from, frame on frame */
         if (frames > 1)
         {
            int64_t d_seq   = (int64_t)(req_seq - prev_seq);
            int64_t d_line0 = (int64_t)(req_line0 - prev_line0);
            if (d_seq < 0 || d_line0 < 0)
               stamp_back++;
            if (d_seq == 0)
               stamp_same++;
            else if (d_seq == 1)
            {
               if (!stamp_n || d_line0 < stamp_min)
                  stamp_min = d_line0;
               if (!stamp_n || d_line0 > stamp_max)
                  stamp_max = d_line0;
               stamp_sum += d_line0;
               stamp_n++;
            }
            else if (d_seq > 1)
               stamp_gaps++;
         }
         prev_seq   = req_seq;
         prev_line0 = req_line0;

         if (rows)
         {
            if (rows_n < LOG_ROWS)
            {
               frame_row_t *r          = &rows[rows_n++];
               r->step_target          = target;
               r->frame                = frames;
               r->black_req_line       = black_req;
               r->black_event_line     = black_event;
               r->white_req_line       = line;
               r->white_req_seq        = req_seq;
               r->white_req_line0_ns   = req_line0;
               r->white_req_now_ns     = req_now;
               r->white_req_clock_ns   = t0;
               r->white_event_clock_ns = t1;
               r->white_event_line     = event_line;
               r->landed_offset        = landed;
            }
            else
               rows_dropped++;
         }

         /* The median, so one late event can't move it */
         if (landed_line < 0 && now_ns() - step_start >= 1000000000u)
         {
            qsort(first, first_n, sizeof(*first), cmp_int);
            landed_offset = first[first_n / 2];
            landed_line = (int)target + landed_offset;
            if (landed_line >= (int)mode->vdisplay)
               landed_line = (int)mode->vdisplay - 1;
            paint_ticks(&black, mode->hdisplay, mode->vdisplay,
                  (unsigned)landed_line, COLOUR_LANDED);
            paint_ticks(&white, mode->hdisplay, mode->vdisplay,
                  (unsigned)landed_line, COLOUR_LANDED);
         }
      }

      paint_ticks(&black, mode->hdisplay, mode->vdisplay, target, COLOUR_BLACK);
      paint_ticks(&white, mode->hdisplay, mode->vdisplay, target, COLOUR_WHITE);
      if (landed_line >= 0)
      {
         paint_ticks(&black, mode->hdisplay, mode->vdisplay,
               (unsigned)landed_line, COLOUR_BLACK);
         paint_ticks(&white, mode->hdisplay, mode->vdisplay,
               (unsigned)landed_line, COLOUR_WHITE);
      }

      if (frames)
      {
         printf("target line %u: %u frames; requested up to +%d lines "
               "late (%u over %d); flip landed by +%.1f lines on average "
               "(+%d..+%d), its event %.1f us after the request on "
               "average, %.1f us at most; black seen past blanking %u "
               "times (upper bound: read after the flip latches - a "
               "white band on screen is the real sign)",
               target, frames, worst, late, LATE_LINES,
               (double)landed_sum / frames, landed_min, landed_max,
               (double)event_ns / frames / 1000.0,
               (double)event_max / 1000.0, missed);
         if (missed)
            printf(" (by line %d at worst)", missed_worst);
         if (landed_line >= 0)
            printf("; green tick at +%d (median of the first second)",
                  landed_offset);
         else
            fputs("; green tick not placed", stdout);
         putchar('\n');

         /* The largest deviation is at one of the extremes */
         if (stamp_n)
         {
            double mean = (double)stamp_sum / stamp_n;
            double dev  = (double)stamp_max - mean;
            if (mean - (double)stamp_min > dev)
               dev = mean - (double)stamp_min;
            printf("  vblank stamps: %u deltas, mean %.3f us (mode says "
                  "%.3f us), max deviation from the mean %.1f us (%.1f "
                  "lines); %u skipped seq gaps",
                  stamp_n, mean / 1000.0, (double)frame_ns / 1000.0,
                  dev / 1000.0, dev / ((double)frame_ns / mode->vtotal),
                  stamp_gaps);
         }
         else
            printf("  vblank stamps: 0 deltas; %u skipped seq gaps",
                  stamp_gaps);
         if (stamp_back || stamp_same)
            printf("; %u went backwards, %u repeated the previous seq",
                  stamp_back, stamp_same);
         putchar('\n');
      }
   }
   ret = 0;

out:
   if (saved)
   {
      if (saved->mode_valid)
         drmModeSetCrtc(fd, saved->crtc_id, saved->buffer_id, saved->x,
               saved->y, &conn->connector_id, 1, &saved->mode);
      drmModeFreeCrtc(saved);
   }
   destroy_fb(fd, &white);
   destroy_fb(fd, &black);
   if (conn)
      drmModeFreeConnector(conn);
   if (res)
      drmModeFreeResources(res);
   if (fd >= 0)
   {
      drmDropMaster(fd);
      close(fd);
   }
   /* With the console back, so a slow write can't hold the display */
   if (log_file)
   {
      int bad = write_log(log_file, rows, rows_n, rows_dropped) != 0;
      if (fclose(log_file) != 0)
         bad = 1;
      if (bad)
      {
         fprintf(stderr, "Writing %s failed: %s\n", log_path,
               strerror(errno));
         ret = 1;
      }
      else
         printf("Wrote %zu frames to %s%s\n", rows_n, log_path,
               rows_dropped ? " (the row buffer filled; see its end)" : "");
   }
   free(rows);
   return ret;
}
