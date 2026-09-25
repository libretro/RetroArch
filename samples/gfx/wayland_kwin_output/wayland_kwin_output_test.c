/* gfx/common/wayland_kwin_output.c, the KWin backend of the Wayland
 * display server's resolution list and mode switch, against a
 * compositor in this process that speaks KWin's output protocols. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include "kde-output-device-v2-server-protocol.h"
#include "kde-output-management-v2-server-protocol.h"
#include "gfx/common/wayland_kwin_output.h"

#define LOG_FN(name) \
   void name(const char *fmt, ...) \
   { va_list ap; va_start(ap, fmt); vfprintf(stdout, fmt, ap); va_end(ap); }
LOG_FN(RARCH_LOG)
LOG_FN(RARCH_WARN)
LOG_FN(RARCH_ERR)
LOG_FN(RARCH_DBG)

/* ---- the compositor: two outputs, the second the one asked about ---- */

typedef struct fmode { int w, h, mhz; struct wl_resource *res; } fmode_t;
static fmode_t out_a[2] = { { 1920, 1080, 60000, NULL }, { 1280, 720, 60000, NULL } };
static fmode_t out_b[4] = { { 2560, 1600, 240000, NULL }, { 1920, 1200, 60000, NULL },
                            { 2560, 1600, 60000, NULL }, { 1920, 1200, 120000, NULL } };

static struct
{
   struct wl_display *dpy;
   const char *socket;
   pthread_mutex_t lock;
   int stop;
   int legacy;           /* publish outputs as globals, as before version 21 */
   int fail;             /* answer apply with failed */
   int applies, config_destroyed;
   int asked_w, asked_h, asked_mhz;
   struct wl_resource *dev_b;
} comp;

static void res_destroy(struct wl_client *c, struct wl_resource *r)
{ (void)c; wl_resource_destroy(r); }

static void send_device(struct wl_client *c, struct wl_resource *dev,
      const char *name, fmode_t *modes, int n, int current)
{
   int i;
   kde_output_device_v2_send_name(dev, name);
   for (i = 0; i < n; i++)
   {
      struct wl_resource *m = wl_resource_create(c,
            &kde_output_device_mode_v2_interface,
            wl_resource_get_version(dev), 0);
      modes[i].res = m;
      kde_output_device_v2_send_mode(dev, m);
      kde_output_device_mode_v2_send_size(m, modes[i].w, modes[i].h);
      kde_output_device_mode_v2_send_refresh(m, modes[i].mhz);
   }
   kde_output_device_v2_send_current_mode(dev, modes[current].res);
   kde_output_device_v2_send_enabled(dev, 1);
   kde_output_device_v2_send_done(dev);
}

static void registry_bind(struct wl_client *c, void *data, uint32_t v, uint32_t id)
{
   struct wl_resource *r = wl_resource_create(c,
         &kde_output_device_registry_v2_interface, v, id), *a, *b;
   (void)data;
   a = wl_resource_create(c, &kde_output_device_v2_interface, v, 0);
   kde_output_device_registry_v2_send_output(r, a);
   send_device(c, a, "eDP-1", out_a, 2, 0);
   b = wl_resource_create(c, &kde_output_device_v2_interface, v, 0);
   kde_output_device_registry_v2_send_output(r, b);
   send_device(c, b, "DP-2", out_b, 4, 2);
   comp.dev_b = b;
}

/* Before version 21: each output is a global of its own */
static void legacy_device_bind(struct wl_client *c, void *data, uint32_t v, uint32_t id)
{
   struct wl_resource *d = wl_resource_create(c,
         &kde_output_device_v2_interface, v, id);
   if (data)
   {
      send_device(c, d, "DP-2", out_b, 4, 2);
      comp.dev_b = d;
   }
   else
      send_device(c, d, "eDP-1", out_a, 2, 0);
}

static void cfg_mode(struct wl_client *c, struct wl_resource *r,
      struct wl_resource *dev, struct wl_resource *mode)
{
   int i;
   (void)c; (void)r;
   for (i = 0; i < 4; i++)
      if (out_b[i].res == mode && dev == comp.dev_b)
      {
         pthread_mutex_lock(&comp.lock);
         comp.asked_w = out_b[i].w; comp.asked_h = out_b[i].h;
         comp.asked_mhz = out_b[i].mhz;
         pthread_mutex_unlock(&comp.lock);
      }
}
static void cfg_apply(struct wl_client *c, struct wl_resource *r)
{
   (void)c;
   pthread_mutex_lock(&comp.lock);
   comp.applies++;
   pthread_mutex_unlock(&comp.lock);
   if (comp.fail)
   {
      kde_output_configuration_v2_send_failure_reason(r, "test says no");
      kde_output_configuration_v2_send_failed(r);
   }
   else
      kde_output_configuration_v2_send_applied(r);
}
static void cfg_destroy(struct wl_client *c, struct wl_resource *r)
{
   pthread_mutex_lock(&comp.lock);
   comp.config_destroyed++;
   pthread_mutex_unlock(&comp.lock);
   res_destroy(c, r);
}

static void cfg_req_enable(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, int32_t enable)
{ (void)c; (void)r; (void)outputdevice; (void)enable; }
static void cfg_req_transform(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, int32_t transform)
{ (void)c; (void)r; (void)outputdevice; (void)transform; }
static void cfg_req_position(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, int32_t x, int32_t y)
{ (void)c; (void)r; (void)outputdevice; (void)x; (void)y; }
static void cfg_req_scale(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, wl_fixed_t scale)
{ (void)c; (void)r; (void)outputdevice; (void)scale; }
static void cfg_req_overscan(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t overscan)
{ (void)c; (void)r; (void)outputdevice; (void)overscan; }
static void cfg_req_set_vrr_policy(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t policy)
{ (void)c; (void)r; (void)outputdevice; (void)policy; }
static void cfg_req_set_rgb_range(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t rgb_range)
{ (void)c; (void)r; (void)outputdevice; (void)rgb_range; }
static void cfg_req_set_primary_output(struct wl_client *c, struct wl_resource *r, struct wl_resource *output)
{ (void)c; (void)r; (void)output; }
static void cfg_req_set_priority(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t priority)
{ (void)c; (void)r; (void)outputdevice; (void)priority; }
static void cfg_req_set_high_dynamic_range(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t enable_hdr)
{ (void)c; (void)r; (void)outputdevice; (void)enable_hdr; }
static void cfg_req_set_sdr_brightness(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t sdr_brightness)
{ (void)c; (void)r; (void)outputdevice; (void)sdr_brightness; }
static void cfg_req_set_wide_color_gamut(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t enable_wcg)
{ (void)c; (void)r; (void)outputdevice; (void)enable_wcg; }
static void cfg_req_set_auto_rotate_policy(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t policy)
{ (void)c; (void)r; (void)outputdevice; (void)policy; }
static void cfg_req_set_icc_profile_path(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, const char *profile_path)
{ (void)c; (void)r; (void)outputdevice; (void)profile_path; }
static void cfg_req_set_brightness_overrides(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, int32_t max_peak_brightness, int32_t max_frame_average_brightness, int32_t min_brightness)
{ (void)c; (void)r; (void)outputdevice; (void)max_peak_brightness; (void)max_frame_average_brightness; (void)min_brightness; }
static void cfg_req_set_sdr_gamut_wideness(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t gamut_wideness)
{ (void)c; (void)r; (void)outputdevice; (void)gamut_wideness; }
static void cfg_req_set_color_profile_source(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t color_profile_source)
{ (void)c; (void)r; (void)outputdevice; (void)color_profile_source; }
static void cfg_req_set_brightness(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t brightness)
{ (void)c; (void)r; (void)outputdevice; (void)brightness; }
static void cfg_req_set_color_power_tradeoff(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t preference)
{ (void)c; (void)r; (void)outputdevice; (void)preference; }
static void cfg_req_set_dimming(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t multiplier)
{ (void)c; (void)r; (void)outputdevice; (void)multiplier; }
static void cfg_req_set_replication_source(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, const char *source)
{ (void)c; (void)r; (void)outputdevice; (void)source; }
static void cfg_req_set_ddc_ci_allowed(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t allowed)
{ (void)c; (void)r; (void)outputdevice; (void)allowed; }
static void cfg_req_set_max_bits_per_color(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t max_bpc)
{ (void)c; (void)r; (void)outputdevice; (void)max_bpc; }
static void cfg_req_set_edr_policy(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t policy)
{ (void)c; (void)r; (void)outputdevice; (void)policy; }
static void cfg_req_set_sharpness(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t sharpness)
{ (void)c; (void)r; (void)outputdevice; (void)sharpness; }
static void cfg_req_set_custom_modes(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, struct wl_resource *modes)
{ (void)c; (void)r; (void)outputdevice; (void)modes; }
static void cfg_req_set_auto_brightness(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t enabled)
{ (void)c; (void)r; (void)outputdevice; (void)enabled; }
static void cfg_req_set_hdr_icc_profile_path(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, const char *profile_path)
{ (void)c; (void)r; (void)outputdevice; (void)profile_path; }
static void cfg_req_set_hdr_color_profile_source(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t color_profile_source)
{ (void)c; (void)r; (void)outputdevice; (void)color_profile_source; }
static void cfg_req_set_abm_level(struct wl_client *c, struct wl_resource *r, struct wl_resource *outputdevice, uint32_t level)
{ (void)c; (void)r; (void)outputdevice; (void)level; }

static const struct kde_output_configuration_v2_interface cfg_impl = {
   cfg_req_enable,
   cfg_mode,
   cfg_req_transform,
   cfg_req_position,
   cfg_req_scale,
   cfg_apply,
   cfg_destroy,
   cfg_req_overscan,
   cfg_req_set_vrr_policy,
   cfg_req_set_rgb_range,
   cfg_req_set_primary_output,
   cfg_req_set_priority,
   cfg_req_set_high_dynamic_range,
   cfg_req_set_sdr_brightness,
   cfg_req_set_wide_color_gamut,
   cfg_req_set_auto_rotate_policy,
   cfg_req_set_icc_profile_path,
   cfg_req_set_brightness_overrides,
   cfg_req_set_sdr_gamut_wideness,
   cfg_req_set_color_profile_source,
   cfg_req_set_brightness,
   cfg_req_set_color_power_tradeoff,
   cfg_req_set_dimming,
   cfg_req_set_replication_source,
   cfg_req_set_ddc_ci_allowed,
   cfg_req_set_max_bits_per_color,
   cfg_req_set_edr_policy,
   cfg_req_set_sharpness,
   cfg_req_set_custom_modes,
   cfg_req_set_auto_brightness,
   cfg_req_set_hdr_icc_profile_path,
   cfg_req_set_hdr_color_profile_source,
   cfg_req_set_abm_level
};

static void mgmt_create_configuration(struct wl_client *c, struct wl_resource *r, uint32_t id)
{
   struct wl_resource *cfg = wl_resource_create(c,
         &kde_output_configuration_v2_interface, wl_resource_get_version(r), id);
   wl_resource_set_implementation(cfg, &cfg_impl, NULL, NULL);
}
static void mgmt_create_mode_list(struct wl_client *c, struct wl_resource *r, uint32_t id)
{ (void)c; (void)r; (void)id; }
static const struct kde_output_management_v2_interface mgmt_impl =
   { mgmt_create_configuration, mgmt_create_mode_list };
static void mgmt_bind(struct wl_client *c, void *data, uint32_t v, uint32_t id)
{
   struct wl_resource *r = wl_resource_create(c,
         &kde_output_management_v2_interface, v, id);
   (void)data;
   wl_resource_set_implementation(r, &mgmt_impl, NULL, NULL);
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
static void start(int legacy, int with_management)
{
   memset(&comp.stop, 0, sizeof(comp) - offsetof(__typeof__(comp), stop));
   comp.legacy = legacy;
   comp.dpy    = wl_display_create();
   comp.socket = wl_display_add_socket_auto(comp.dpy);
   if (legacy)
   {
      wl_global_create(comp.dpy, &kde_output_device_v2_interface, 20, NULL, legacy_device_bind);
      wl_global_create(comp.dpy, &kde_output_device_v2_interface, 20, (void*)1, legacy_device_bind);
   }
   else
      wl_global_create(comp.dpy, &kde_output_device_registry_v2_interface, 23, NULL, registry_bind);
   if (with_management)
      wl_global_create(comp.dpy, &kde_output_management_v2_interface, 21, NULL, mgmt_bind);
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

static kwin_outputs_t kw;
static void reg_global(void *d, struct wl_registry *r, uint32_t id, const char *i, uint32_t v)
{ (void)d; kwin_outputs_bind(&kw, r, id, i, v); }
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
   memset(&kw, 0, sizeof(kw));
   cdpy = wl_display_connect(comp.socket);
   creg = wl_display_get_registry(cdpy);
   wl_registry_add_listener(creg, &reg_listener, NULL);
   wl_display_roundtrip(cdpy);
   wl_display_roundtrip(cdpy);
   wl_display_roundtrip(cdpy);
}
static void disconnect_client(void)
{
   kwin_outputs_destroy(&kw);
   wl_registry_destroy(creg);
   wl_display_roundtrip(cdpy);
   wl_display_disconnect(cdpy);
}

static void check_list(const char *form)
{
   unsigned n = 0, i, cur = 0;
   int sorted = 1;
   video_display_config_t *l = kwin_outputs_resolution_list(&kw, "DP-2", &n);
   char what[128];
   snprintf(what, sizeof(what), "%s: DP-2's four modes listed", form);
   check(what, l && n == 4);
   for (i = 1; l && i < n; i++)
      if (VIDEO_SCALE_W(l[i].dims) < VIDEO_SCALE_W(l[i - 1].dims)
            || (VIDEO_SCALE_W(l[i].dims) == VIDEO_SCALE_W(l[i - 1].dims)
               && l[i].refreshrate_float < l[i - 1].refreshrate_float))
         sorted = 0;
   for (i = 0; l && i < n; i++)
      if (l[i].current)
      {
         cur++;
         check("the current one is 2560x1600 at 60 Hz",
               VIDEO_SCALE_W(l[i].dims) == 2560 && VIDEO_SCALE_H(l[i].dims) == 1600
               && l[i].refreshrate == 60);
      }
   check("sorted by size then rate, one current", sorted && cur == 1);
   free(l);
}

int main(void)
{
   unsigned n;
   video_display_config_t *l;

   pthread_mutex_init(&comp.lock, NULL);
   setenv("XDG_RUNTIME_DIR", "/tmp", 0);

   printf("1. outputs through the registry (KWin 6, version 21 on)\n");
   start(0, 1);
   connect_client();
   check("ready", kwin_outputs_ready(&kw));
   check_list("registry");
   l = kwin_outputs_resolution_list(&kw, "HDMI-A-9", &n);
   check("an unknown connector falls back to the first enabled output", l && n == 2);
   free(l);

   printf("2. switching\n");
   check("an unlisted size is refused", !kwin_outputs_set_mode(&kw, "DP-2", 1024, 768, 60, 60.0f));
   check("the current mode asks nothing", kwin_outputs_set_mode(&kw, "DP-2", 2560, 1600, 60, 60.0f));
   wl_display_roundtrip(cdpy);
   pthread_mutex_lock(&comp.lock);
   check("no configuration applied for either", comp.applies == 0);
   pthread_mutex_unlock(&comp.lock);
   check("2560x1600 near 239.9 Hz asked for", kwin_outputs_set_mode(&kw, "DP-2", 2560, 1600, 240, 239.9f));
   wl_display_roundtrip(cdpy);
   wl_display_roundtrip(cdpy);
   pthread_mutex_lock(&comp.lock);
   check("KWin was asked for 2560x1600 at 240 Hz on DP-2", comp.applies == 1 && comp.asked_w == 2560 && comp.asked_h == 1600 && comp.asked_mhz == 240000);
   check("the configuration is released once applied", comp.config_destroyed == 1);
   pthread_mutex_unlock(&comp.lock);
   comp.fail = 1;
   /* this compositor does not announce the new current mode, so the
    * current one is still 60 Hz: a rate-only switch to 240 Hz */
   check("a rate-only switch keeps the size", kwin_outputs_set_mode(&kw, "DP-2", 0, 0, 0, 240.0f));
   wl_display_roundtrip(cdpy);
   wl_display_roundtrip(cdpy);
   pthread_mutex_lock(&comp.lock);
   check("a refusal is heard and the configuration still released", comp.applies == 2 && comp.config_destroyed == 2 && comp.asked_w == 2560 && comp.asked_mhz == 240000);
   pthread_mutex_unlock(&comp.lock);

   printf("3. a mode KWin removes\n");
   kde_output_device_mode_v2_send_removed(out_b[3].res);
   wl_display_flush_clients(comp.dpy);
   wl_display_roundtrip(cdpy);
   l = kwin_outputs_resolution_list(&kw, "DP-2", &n);
   check("is gone from the list", l && n == 3);
   free(l);
   disconnect_client();
   stop();

   printf("4. outputs as globals (before version 21)\n");
   start(1, 1);
   connect_client();
   check("ready", kwin_outputs_ready(&kw));
   check_list("globals");
   disconnect_client();
   stop();

   printf("5. without the management global\n");
   start(0, 0);
   connect_client();
   check("not ready: nothing can be switched", !kwin_outputs_ready(&kw));
   check("and nothing listed", kwin_outputs_resolution_list(&kw, "DP-2", &n) == NULL && n == 0);
   disconnect_client();
   stop();

   pthread_mutex_destroy(&comp.lock);
   if (fails)
   {
      printf("wayland kwin output: %d check(s) failed\n", fails);
      return 1;
   }
   printf("wayland kwin output: all checks passed\n");
   return 0;
}
