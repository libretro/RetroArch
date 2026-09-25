/* gfx/common/wayland_wlr_output.c, the wlroots backend of the Wayland
 * display server's resolution list and mode switch (sway, Hyprland,
 * river), against a compositor in this process that speaks
 * zwlr_output_manager_v1. The protocol makes it fatal to leave a head
 * out of a configuration or to configure one twice; this compositor
 * checks every configuration the way wlroots does. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include "wlr-output-management-unstable-v1-server-protocol.h"
#include "gfx/common/wayland_wlr_output.h"

#define LOG_FN(name) \
   void name(const char *fmt, ...) \
   { va_list ap; va_start(ap, fmt); vfprintf(stdout, fmt, ap); va_end(ap); }
LOG_FN(RARCH_LOG)
LOG_FN(RARCH_WARN)
LOG_FN(RARCH_ERR)
LOG_FN(RARCH_DBG)

/* ---- the compositor: DP-2 is asked about, HDMI-A-1 is disabled ---- */

typedef struct fmode { int w, h, mhz; struct wl_resource *res; } fmode_t;
static fmode_t dp2[4]  = { { 2560, 1600, 240000, NULL }, { 1920, 1200, 60000, NULL },
                           { 2560, 1600, 60000, NULL },  { 1920, 1200, 120000, NULL } };
static fmode_t hdmi[1] = { { 1920, 1080, 60000, NULL } };

enum { ANSWER_SUCCEEDED, ANSWER_FAILED, ANSWER_CANCELLED };

static struct
{
   struct wl_display *dpy;
   const char *socket;
   pthread_mutex_t lock;
   int stop;
   int answer;
   uint32_t serial;
   int applies, config_destroyed, protocol_errors;
   uint32_t asked_serial;
   int asked_w, asked_h, asked_mhz;
   int dp2_enabled, dp2_mode_set, hdmi_disabled, hdmi_enabled;
   struct wl_resource *dp2_head, *hdmi_head;
} comp;

static void res_destroy(struct wl_client *c, struct wl_resource *r)
{ (void)c; wl_resource_destroy(r); }

/* Heads and modes are released by the client from version 3 */
static const struct zwlr_output_head_v1_interface head_impl = { res_destroy };
static const struct zwlr_output_mode_v1_interface mode_impl = { res_destroy };

static struct wl_resource *send_head(struct wl_client *c, struct wl_resource *mgr,
      const char *name, fmode_t *modes, int n, int current, int enabled)
{
   int i;
   struct wl_resource *h = wl_resource_create(c, &zwlr_output_head_v1_interface,
         wl_resource_get_version(mgr), 0);
   wl_resource_set_implementation(h, &head_impl, NULL, NULL);
   zwlr_output_manager_v1_send_head(mgr, h);
   zwlr_output_head_v1_send_name(h, name);
   for (i = 0; i < n; i++)
   {
      struct wl_resource *m = wl_resource_create(c, &zwlr_output_mode_v1_interface,
            wl_resource_get_version(mgr), 0);
      wl_resource_set_implementation(m, &mode_impl, NULL, NULL);
      modes[i].res = m;
      zwlr_output_head_v1_send_mode(h, m);
      zwlr_output_mode_v1_send_size(m, modes[i].w, modes[i].h);
      zwlr_output_mode_v1_send_refresh(m, modes[i].mhz);
   }
   zwlr_output_head_v1_send_enabled(h, enabled);
   if (enabled)
      zwlr_output_head_v1_send_current_mode(h, modes[current].res);
   return h;
}

/* A head configuration */
static void ch_set_mode(struct wl_client *c, struct wl_resource *r, struct wl_resource *mode)
{
   int i;
   (void)c; (void)r;
   pthread_mutex_lock(&comp.lock);
   for (i = 0; i < 4; i++)
      if (dp2[i].res == mode)
      {
         comp.dp2_mode_set++;
         comp.asked_w = dp2[i].w; comp.asked_h = dp2[i].h; comp.asked_mhz = dp2[i].mhz;
      }
   pthread_mutex_unlock(&comp.lock);
}
static void ch_set_custom_mode(struct wl_client *c, struct wl_resource *r, int32_t w, int32_t h, int32_t rf) { (void)c; (void)r; (void)w; (void)h; (void)rf; }
static void ch_set_position(struct wl_client *c, struct wl_resource *r, int32_t x, int32_t y) { (void)c; (void)r; (void)x; (void)y; }
static void ch_set_transform(struct wl_client *c, struct wl_resource *r, int32_t t) { (void)c; (void)r; (void)t; }
static void ch_set_scale(struct wl_client *c, struct wl_resource *r, wl_fixed_t s) { (void)c; (void)r; (void)s; }
static void ch_set_adaptive_sync(struct wl_client *c, struct wl_resource *r, uint32_t s) { (void)c; (void)r; (void)s; }
static const struct zwlr_output_configuration_head_v1_interface ch_impl = {
   ch_set_mode, ch_set_custom_mode, ch_set_position, ch_set_transform,
   ch_set_scale, ch_set_adaptive_sync
};

/* A configuration, checked as wlroots checks one */
static void cfg_enable_head(struct wl_client *c, struct wl_resource *r, uint32_t id, struct wl_resource *head)
{
   struct wl_resource *ch = wl_resource_create(c, &zwlr_output_configuration_head_v1_interface,
         wl_resource_get_version(r), id);
   wl_resource_set_implementation(ch, &ch_impl, NULL, NULL);
   pthread_mutex_lock(&comp.lock);
   if (head == comp.dp2_head)  comp.dp2_enabled++;
   if (head == comp.hdmi_head) comp.hdmi_enabled++;
   pthread_mutex_unlock(&comp.lock);
}
static void cfg_disable_head(struct wl_client *c, struct wl_resource *r, struct wl_resource *head)
{
   (void)c; (void)r;
   pthread_mutex_lock(&comp.lock);
   if (head == comp.hdmi_head) comp.hdmi_disabled++;
   if (head == comp.dp2_head)  comp.dp2_enabled += 100;   /* wrong: DP-2 is on */
   pthread_mutex_unlock(&comp.lock);
}
static void cfg_apply(struct wl_client *c, struct wl_resource *r)
{
   int dp2_times, hdmi_times;
   (void)c;
   pthread_mutex_lock(&comp.lock);
   comp.applies++;
   dp2_times  = comp.dp2_enabled;
   hdmi_times = comp.hdmi_enabled + comp.hdmi_disabled;
   pthread_mutex_unlock(&comp.lock);
   if (dp2_times == 0 || hdmi_times == 0)
   {
      pthread_mutex_lock(&comp.lock); comp.protocol_errors++; pthread_mutex_unlock(&comp.lock);
      wl_resource_post_error(r, ZWLR_OUTPUT_CONFIGURATION_V1_ERROR_UNCONFIGURED_HEAD,
            "head has not been configured");
      return;
   }
   if (dp2_times > 1 || hdmi_times > 1)
   {
      pthread_mutex_lock(&comp.lock); comp.protocol_errors++; pthread_mutex_unlock(&comp.lock);
      wl_resource_post_error(r, ZWLR_OUTPUT_CONFIGURATION_V1_ERROR_ALREADY_CONFIGURED_HEAD,
            "head has been configured twice");
      return;
   }
   if (comp.answer == ANSWER_FAILED)
      zwlr_output_configuration_v1_send_failed(r);
   else if (comp.answer == ANSWER_CANCELLED)
      zwlr_output_configuration_v1_send_cancelled(r);
   else
      zwlr_output_configuration_v1_send_succeeded(r);
}
static void cfg_test(struct wl_client *c, struct wl_resource *r) { (void)c; (void)r; }
static void cfg_destroy(struct wl_client *c, struct wl_resource *r)
{
   pthread_mutex_lock(&comp.lock);
   comp.config_destroyed++;
   pthread_mutex_unlock(&comp.lock);
   res_destroy(c, r);
}
static const struct zwlr_output_configuration_v1_interface cfg_impl = {
   cfg_enable_head, cfg_disable_head, cfg_apply, cfg_test, cfg_destroy
};

static void mgr_create_configuration(struct wl_client *c, struct wl_resource *r, uint32_t id, uint32_t serial)
{
   struct wl_resource *cfg = wl_resource_create(c, &zwlr_output_configuration_v1_interface,
         wl_resource_get_version(r), id);
   wl_resource_set_implementation(cfg, &cfg_impl, NULL, NULL);
   pthread_mutex_lock(&comp.lock);
   comp.asked_serial = serial;
   comp.dp2_enabled = comp.dp2_mode_set = comp.hdmi_disabled = comp.hdmi_enabled = 0;
   pthread_mutex_unlock(&comp.lock);
}
static void mgr_stop(struct wl_client *c, struct wl_resource *r) { (void)c; (void)r; }
static const struct zwlr_output_manager_v1_interface mgr_impl = { mgr_create_configuration, mgr_stop };

static void mgr_bind(struct wl_client *c, void *data, uint32_t v, uint32_t id)
{
   struct wl_resource *r = wl_resource_create(c, &zwlr_output_manager_v1_interface, v, id);
   (void)data;
   wl_resource_set_implementation(r, &mgr_impl, NULL, NULL);
   comp.dp2_head  = send_head(c, r, "DP-2", dp2, 4, 2, 1);
   comp.hdmi_head = send_head(c, r, "HDMI-A-1", hdmi, 1, 0, 0);
   zwlr_output_manager_v1_send_done(r, comp.serial);
}

static void *comp_thread(void *arg)
{
   struct wl_event_loop *loop = wl_display_get_event_loop(comp.dpy);
   (void)arg;
   for (;;)
   {
      int stop;
      pthread_mutex_lock(&comp.lock);
      stop = comp.stop;
      pthread_mutex_unlock(&comp.lock);
      if (stop)
         break;
      wl_event_loop_dispatch(loop, 10);
      wl_display_flush_clients(comp.dpy);
   }
   return NULL;
}

static pthread_t tid;
static void start(uint32_t version)
{
   memset(&comp.stop, 0, sizeof(comp) - offsetof(__typeof__(comp), stop));
   comp.serial = 4242;
   comp.dpy    = wl_display_create();
   comp.socket = wl_display_add_socket_auto(comp.dpy);
   wl_global_create(comp.dpy, &zwlr_output_manager_v1_interface, version, NULL, mgr_bind);
   pthread_create(&tid, NULL, comp_thread, NULL);
}
static void stop(void)
{
   pthread_mutex_lock(&comp.lock);
   comp.stop = 1;
   pthread_mutex_unlock(&comp.lock);
   pthread_join(tid, NULL);
   wl_display_destroy_clients(comp.dpy);
   wl_display_destroy(comp.dpy);
}

/* ---- the client, as dispserv_wl binds it ---- */

static wlr_outputs_t wo;
static void reg_global(void *d, struct wl_registry *r, uint32_t id, const char *i, uint32_t v)
{ (void)d; wlr_outputs_bind(&wo, r, id, i, v); }
static void reg_remove(void *d, struct wl_registry *r, uint32_t id) { (void)d; (void)r; (void)id; }
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

static int fails;
static void check(const char *what, int ok)
{
   printf("   %s %s\n", ok ? "ok  " : "FAIL", what);
   if (!ok)
      fails++;
}

static struct wl_display *cdpy;
static struct wl_registry *creg;
static void connect_client(void)
{
   memset(&wo, 0, sizeof(wo));
   cdpy = wl_display_connect(comp.socket);
   creg = wl_display_get_registry(cdpy);
   wl_registry_add_listener(creg, &reg_listener, NULL);
   wl_display_roundtrip(cdpy);
   wl_display_roundtrip(cdpy);
}
static void disconnect_client(void)
{
   wlr_outputs_destroy(&wo);
   wl_registry_destroy(creg);
   wl_display_roundtrip(cdpy);
   wl_display_disconnect(cdpy);
}

static void switch_and_settle(unsigned w, unsigned h, float hz, int *requested)
{
   *requested = wlr_outputs_set_mode(&wo, "DP-2", w, h, 0, hz);
   wl_display_roundtrip(cdpy);
   wl_display_roundtrip(cdpy);
}

int main(void)
{
   unsigned n, i, cur = 0;
   int requested;
   video_display_config_t *l;

   pthread_mutex_init(&comp.lock, NULL);
   setenv("XDG_RUNTIME_DIR", "/tmp", 0);

   for (i = 0; i < 2; i++)
   {
      uint32_t version = i ? 2 : 4;
      printf("%u. manager version %u%s\n", i + 1, version, i ? " (modes forgotten, not released)" : "");
      start(version);
      connect_client();
      check("ready", wlr_outputs_ready(&wo));
      l = wlr_outputs_resolution_list(&wo, "DP-2", &n);
      check("DP-2's four modes listed", l && n == 4);
      for (cur = 0, n = l ? n : 0; n--; )
         if (l[n].current)
         {
            cur++;
            check("the current one is 2560x1600 at 60 Hz",
                  VIDEO_SCALE_W(l[n].dims) == 2560 && l[n].refreshrate == 60);
         }
      check("one current", cur == 1);
      free(l);

      switch_and_settle(1024, 768, 60.0f, &requested);
      check("an unlisted size is refused", !requested);
      switch_and_settle(2560, 1600, 60.0f, &requested);
      pthread_mutex_lock(&comp.lock);
      check("the current mode asks nothing", requested && comp.applies == 0);
      pthread_mutex_unlock(&comp.lock);

      switch_and_settle(2560, 1600, 239.9f, &requested);
      pthread_mutex_lock(&comp.lock);
      check("asked: 2560x1600 at 240 Hz on DP-2", requested && comp.applies == 1
            && comp.asked_w == 2560 && comp.asked_h == 1600 && comp.asked_mhz == 240000);
      check("with the last done serial", comp.asked_serial == 4242);
      check("every head exactly once: DP-2 enabled with its mode, HDMI-A-1 disabled",
            comp.dp2_enabled == 1 && comp.dp2_mode_set == 1
            && comp.hdmi_disabled == 1 && comp.hdmi_enabled == 0);
      check("no protocol error", comp.protocol_errors == 0);
      check("the configuration released once it succeeded", comp.config_destroyed == 1);
      pthread_mutex_unlock(&comp.lock);

      comp.answer = ANSWER_FAILED;
      switch_and_settle(1920, 1200, 120.0f, &requested);
      comp.answer = ANSWER_CANCELLED;
      switch_and_settle(1920, 1200, 60.0f, &requested);
      pthread_mutex_lock(&comp.lock);
      check("failed and cancelled are heard, each configuration released",
            comp.applies == 3 && comp.config_destroyed == 3 && comp.protocol_errors == 0);
      pthread_mutex_unlock(&comp.lock);

      zwlr_output_mode_v1_send_finished(dp2[3].res);
      wl_display_flush_clients(comp.dpy);
      wl_display_roundtrip(cdpy);
      l = wlr_outputs_resolution_list(&wo, "DP-2", &n);
      check("a finished mode is gone from the list", l && n == 3);
      free(l);
      disconnect_client();
      stop();
   }

   pthread_mutex_destroy(&comp.lock);
   if (fails)
   {
      printf("wayland wlr output: %d check(s) failed\n", fails);
      return 1;
   }
   printf("wayland wlr output: all checks passed\n");
   return 0;
}
