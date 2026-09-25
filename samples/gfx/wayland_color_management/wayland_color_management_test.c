/* gfx/common/wayland_color.c, the client side of wp_color_manager_v1
 * as RetroArch's GL context uses it for HDR, against a compositor in
 * this process whose answers each case chooses. The protocol makes
 * several mistakes fatal to the client - asking for Windows-scRGB
 * without the feature, setting a description before it is ready, an
 * intent the compositor did not offer, a second colour-management
 * object on one surface - and the compositor here records every one. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include "color-management-v1-server-protocol.h"
#include "gfx/common/wayland_color.h"

/* ---- the log RetroArch's code writes to ---- */

#define LOG_FN(name) \
   void name(const char *fmt, ...) \
   { va_list ap; va_start(ap, fmt); vfprintf(stdout, fmt, ap); va_end(ap); }
LOG_FN(RARCH_LOG)
LOG_FN(RARCH_WARN)
LOG_FN(RARCH_ERR)
LOG_FN(RARCH_DBG)

/* ---- the compositor ---- */

enum answer { ANSWER_READY, ANSWER_FAILED };

static struct
{
   struct wl_display *dpy;
   const char *socket;
   pthread_mutex_t lock;
   int stop;
   /* what this compositor offers */
   int offer_scrgb, offer_perceptual, offer_parametric;
   /* what the parametric creator was told, and the rules it broke */
   int params_created, params_tf, params_primaries;
   uint32_t params_min, params_max, params_reference;
   int params_incomplete, params_already_set;
   enum answer answer;
   /* what the client did */
   int get_surface, create_scrgb, set_desc, set_intent;
   int violations;
   char violation[256];
   int desc_ready;
   int surface_destroyed_before_cm;
   int cm_surface_alive;
   /* the output's description: its target peak, 0 to send none */
   unsigned output_peak;
   int send_icc;
   int info_asked;
} comp;

static void violate(const char *what)
{
   comp.violations++;
   snprintf(comp.violation, sizeof(comp.violation), "%s", what);
}

static void res_destroy(struct wl_client *c, struct wl_resource *r)
{
   (void)c;
   wl_resource_destroy(r);
}

/* wl_surface: only destroy matters here */
static void surface_destroy(struct wl_client *c, struct wl_resource *r)
{
   pthread_mutex_lock(&comp.lock);
   if (comp.cm_surface_alive)
      comp.surface_destroyed_before_cm = 1;
   pthread_mutex_unlock(&comp.lock);
   res_destroy(c, r);
}
static const struct wl_surface_interface surface_impl = { surface_destroy };

static void compositor_create_surface(struct wl_client *c,
      struct wl_resource *r, uint32_t id)
{
   struct wl_resource *s = wl_resource_create(c, &wl_surface_interface, 1, id);
   (void)r;
   wl_resource_set_implementation(s, &surface_impl, NULL, NULL);
}
static const struct wl_compositor_interface compositor_impl =
   { compositor_create_surface, NULL };

static void output_bind(struct wl_client *c, void *data, uint32_t v,
      uint32_t id)
{
   (void)data; (void)v;
   wl_resource_create(c, &wl_output_interface, 1, id);
}

static void compositor_bind(struct wl_client *c, void *data, uint32_t v,
      uint32_t id)
{
   struct wl_resource *r = wl_resource_create(c, &wl_compositor_interface, 1, id);
   (void)data; (void)v;
   wl_resource_set_implementation(r, &compositor_impl, NULL, NULL);
}

/* wp_image_description_v1 */
static void desc_get_information(struct wl_client *c, struct wl_resource *r,
      uint32_t id)
{
   (void)c; (void)r; (void)id;
   pthread_mutex_lock(&comp.lock);
   violate("get_information on Windows-scRGB");
   pthread_mutex_unlock(&comp.lock);
}
static const struct wp_image_description_v1_interface desc_impl =
   { res_destroy, desc_get_information };

/* wp_image_description_creator_params_v1: the protocol makes it fatal
 * to leave the transfer function or primaries unset at create, or to
 * set either twice. */
static void params_set_tf_named(struct wl_client *c, struct wl_resource *r, uint32_t tf)
{
   (void)c; (void)r;
   pthread_mutex_lock(&comp.lock);
   if (comp.params_tf++)
      comp.params_already_set++;
   if (tf != WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR)
      violate("a transfer function that was not offered");
   pthread_mutex_unlock(&comp.lock);
}
static void params_set_primaries_named(struct wl_client *c, struct wl_resource *r, uint32_t p)
{
   (void)c; (void)r;
   pthread_mutex_lock(&comp.lock);
   if (comp.params_primaries++)
      comp.params_already_set++;
   if (p != WP_COLOR_MANAGER_V1_PRIMARIES_SRGB)
      violate("primaries that were not offered");
   pthread_mutex_unlock(&comp.lock);
}
static void params_set_luminances(struct wl_client *c, struct wl_resource *r,
      uint32_t min_lum, uint32_t max_lum, uint32_t reference_lum)
{
   (void)c; (void)r;
   pthread_mutex_lock(&comp.lock);
   comp.params_min       = min_lum;
   comp.params_max       = max_lum;
   comp.params_reference = reference_lum;
   pthread_mutex_unlock(&comp.lock);
}
static void params_create(struct wl_client *c, struct wl_resource *r, uint32_t id)
{
   struct wl_resource *d;
   pthread_mutex_lock(&comp.lock);
   comp.params_created++;
   if (!comp.params_tf || !comp.params_primaries)
      comp.params_incomplete++;
   pthread_mutex_unlock(&comp.lock);
   d = wl_resource_create(c, &wp_image_description_v1_interface, 1, id);
   wl_resource_set_implementation(d, &desc_impl, NULL, NULL);
   /* create destroys the creator */
   wl_resource_destroy(r);
   if (comp.answer == ANSWER_READY)
   {
      pthread_mutex_lock(&comp.lock);
      comp.desc_ready = 1;
      pthread_mutex_unlock(&comp.lock);
      wp_image_description_v1_send_ready(d, 3);
   }
   else
      wp_image_description_v1_send_failed(d,
            WP_IMAGE_DESCRIPTION_V1_CAUSE_UNSUPPORTED, "not today");
}
static void params_ignore_8int(struct wl_client *c, struct wl_resource *r,
      int32_t a, int32_t b, int32_t d, int32_t e, int32_t f, int32_t g,
      int32_t h, int32_t i)
{ (void)c; (void)r; (void)a; (void)b; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; }
static void params_ignore_uint(struct wl_client *c, struct wl_resource *r, uint32_t v)
{ (void)c; (void)r; (void)v; }
static void params_ignore_2uint(struct wl_client *c, struct wl_resource *r, uint32_t a, uint32_t b)
{ (void)c; (void)r; (void)a; (void)b; }
static const struct wp_image_description_creator_params_v1_interface params_impl = {
   params_create, params_set_tf_named, params_ignore_uint,
   params_set_primaries_named, params_ignore_8int, params_set_luminances,
   params_ignore_8int, params_ignore_2uint, params_ignore_uint,
   params_ignore_uint
};

/* wp_color_management_surface_v1 */
static void cms_destroy(struct wl_client *c, struct wl_resource *r)
{
   pthread_mutex_lock(&comp.lock);
   comp.cm_surface_alive = 0;
   pthread_mutex_unlock(&comp.lock);
   res_destroy(c, r);
}
static void cms_set_image_description(struct wl_client *c,
      struct wl_resource *r, struct wl_resource *desc, uint32_t intent)
{
   (void)c; (void)r; (void)desc;
   pthread_mutex_lock(&comp.lock);
   comp.set_desc++;
   comp.set_intent = (int)intent;
   if (!comp.desc_ready)
      violate("set_image_description with a description not ready");
   if (     intent != WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL
         || !comp.offer_perceptual)
      violate("render intent not offered");
   pthread_mutex_unlock(&comp.lock);
}
static void cms_unset(struct wl_client *c, struct wl_resource *r)
{
   (void)c; (void)r;
}
static const struct wp_color_management_surface_v1_interface cms_impl =
   { cms_destroy, cms_set_image_description, cms_unset };

/* The output's image description and its information */
static void odesc_get_information(struct wl_client *c,
      struct wl_resource *r, uint32_t id)
{
   struct wl_resource *info = wl_resource_create(c,
         &wp_image_description_info_v1_interface, 1, id);
   (void)r;
   pthread_mutex_lock(&comp.lock);
   comp.info_asked++;
   pthread_mutex_unlock(&comp.lock);
   if (comp.send_icc)
   {
      int fd = open("/dev/null", O_RDONLY);
      wp_image_description_info_v1_send_icc_file(info, fd, 0);
      close(fd);
   }
   /* the transfer function's range: PQ's 10,000 nits, not the panel's */
   wp_image_description_info_v1_send_luminances(info, 50, 10000, 203);
   if (comp.output_peak)
      wp_image_description_info_v1_send_target_luminance(info, 1,
            comp.output_peak);
   wp_image_description_info_v1_send_done(info);
   wl_resource_destroy(info);
}
static const struct wp_image_description_v1_interface odesc_impl =
   { res_destroy, odesc_get_information };

static void cmo_get_image_description(struct wl_client *c,
      struct wl_resource *r, uint32_t id)
{
   struct wl_resource *d = wl_resource_create(c,
         &wp_image_description_v1_interface, 1, id);
   (void)r;
   wl_resource_set_implementation(d, &odesc_impl, NULL, NULL);
   wp_image_description_v1_send_ready(d, 2);
}
static const struct wp_color_management_output_v1_interface cmo_impl =
   { res_destroy, cmo_get_image_description };

/* wp_color_manager_v1 */
static void cm_get_output(struct wl_client *c, struct wl_resource *r,
      uint32_t id, struct wl_resource *o)
{
   struct wl_resource *out = wl_resource_create(c,
         &wp_color_management_output_v1_interface, 1, id);
   (void)r; (void)o;
   wl_resource_set_implementation(out, &cmo_impl, NULL, NULL);
}
static void cm_get_surface(struct wl_client *c, struct wl_resource *r,
      uint32_t id, struct wl_resource *surface)
{
   struct wl_resource *s;
   (void)r; (void)surface;
   pthread_mutex_lock(&comp.lock);
   comp.get_surface++;
   if (comp.cm_surface_alive)
      violate("second colour-management object on the surface");
   comp.cm_surface_alive = 1;
   pthread_mutex_unlock(&comp.lock);
   s = wl_resource_create(c, &wp_color_management_surface_v1_interface, 1, id);
   wl_resource_set_implementation(s, &cms_impl, NULL, NULL);
}
static void cm_get_surface_feedback(struct wl_client *c,
      struct wl_resource *r, uint32_t id, struct wl_resource *s)
{ (void)c; (void)r; (void)id; (void)s; }
static void cm_create_icc(struct wl_client *c, struct wl_resource *r,
      uint32_t id)
{ (void)c; (void)r; (void)id; }
static void cm_create_params(struct wl_client *c, struct wl_resource *r,
      uint32_t id)
{
   struct wl_resource *p = wl_resource_create(c,
         &wp_image_description_creator_params_v1_interface, 1, id);
   (void)r;
   wl_resource_set_implementation(p, &params_impl, NULL, NULL);
   pthread_mutex_lock(&comp.lock);
   if (!comp.offer_parametric)
      violate("a parametric creator without the feature");
   comp.params_tf = comp.params_primaries = 0;
   pthread_mutex_unlock(&comp.lock);
}
static void cm_create_windows_scrgb(struct wl_client *c,
      struct wl_resource *r, uint32_t id)
{
   struct wl_resource *d = wl_resource_create(c,
         &wp_image_description_v1_interface, 1, id);
   (void)r;
   wl_resource_set_implementation(d, &desc_impl, NULL, NULL);
   pthread_mutex_lock(&comp.lock);
   comp.create_scrgb++;
   if (!comp.offer_scrgb)
      violate("create_windows_scrgb without the feature");
   pthread_mutex_unlock(&comp.lock);
   if (comp.answer == ANSWER_READY)
   {
      pthread_mutex_lock(&comp.lock);
      comp.desc_ready = 1;
      pthread_mutex_unlock(&comp.lock);
      wp_image_description_v1_send_ready(d, 1);
   }
   else
      wp_image_description_v1_send_failed(d,
            WP_IMAGE_DESCRIPTION_V1_CAUSE_UNSUPPORTED, "not today");
}
static const struct wp_color_manager_v1_interface cm_impl = {
   res_destroy, cm_get_output, cm_get_surface, cm_get_surface_feedback,
   cm_create_icc, cm_create_params, cm_create_windows_scrgb
};

static void cm_bind(struct wl_client *c, void *data, uint32_t v, uint32_t id)
{
   struct wl_resource *r = wl_resource_create(c,
         &wp_color_manager_v1_interface, 1, id);
   (void)data; (void)v;
   wl_resource_set_implementation(r, &cm_impl, NULL, NULL);
   if (comp.offer_perceptual)
      wp_color_manager_v1_send_supported_intent(r,
            WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
   wp_color_manager_v1_send_supported_intent(r,
         WP_COLOR_MANAGER_V1_RENDER_INTENT_RELATIVE);
   if (comp.offer_parametric)
   {
      wp_color_manager_v1_send_supported_feature(r,
            WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC);
      wp_color_manager_v1_send_supported_feature(r,
            WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES);
   }
   if (comp.offer_scrgb)
      wp_color_manager_v1_send_supported_feature(r,
            WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_SCRGB);
   wp_color_manager_v1_send_supported_tf_named(r,
         WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB);
   if (comp.offer_parametric)
      wp_color_manager_v1_send_supported_tf_named(r,
            WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR);
   wp_color_manager_v1_send_supported_primaries_named(r,
         WP_COLOR_MANAGER_V1_PRIMARIES_SRGB);
   wp_color_manager_v1_send_done(r);
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

/* ---- the client, as RetroArch's context is ---- */

struct client
{
   struct wl_display *dpy;
   struct wl_registry *reg;
   struct wl_compositor *compositor;
   struct wl_output *output;
   wl_color_t color;
};

static void reg_global(void *data, struct wl_registry *reg, uint32_t id,
      const char *iface, uint32_t version)
{
   struct client *cl = (struct client*)data;
   if (!strcmp(iface, wl_compositor_interface.name))
      cl->compositor = (struct wl_compositor*)wl_registry_bind(reg, id,
            &wl_compositor_interface, 1);
   else if (!strcmp(iface, wl_output_interface.name))
      cl->output = (struct wl_output*)wl_registry_bind(reg, id,
            &wl_output_interface, 1);
   else if (!strcmp(iface, wl_color_interface_name()))
      wl_color_bind(&cl->color, reg, id, version);
}
static void reg_remove(void *d, struct wl_registry *r, uint32_t id)
{ (void)d; (void)r; (void)id; }
static const struct wl_registry_listener reg_listener = { reg_global, reg_remove };

static int fails;
static void check(const char *what, int ok)
{
   printf("   %s %s\n", ok ? "ok  " : "FAIL", what);
   if (!ok)
      fails++;
}

static pthread_t tid;

static float cb_peak;
static void peak_cb(float nits) { cb_peak = nits; }

static void start_full(int scrgb, int perceptual, int parametric,
      enum answer answer)
{
   memset(&comp.offer_scrgb, 0,
         sizeof(comp) - offsetof(__typeof__(comp), offer_scrgb));
   comp.offer_parametric = parametric;
   comp.offer_scrgb      = scrgb;
   comp.offer_perceptual = perceptual;
   comp.answer           = answer;
   comp.stop             = 0;
   comp.dpy              = wl_display_create();
   comp.socket           = wl_display_add_socket_auto(comp.dpy);
   wl_global_create(comp.dpy, &wl_compositor_interface, 1, NULL, compositor_bind);
   wl_global_create(comp.dpy, &wp_color_manager_v1_interface, 1, NULL, cm_bind);
   wl_global_create(comp.dpy, &wl_output_interface, 1, NULL, output_bind);
   pthread_create(&tid, NULL, comp_thread, NULL);
}

static void start(int scrgb, int perceptual, enum answer answer)
{
   start_full(scrgb, perceptual, 1, answer);
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

/* Connects, binds, and makes the two roundtrips RetroArch's context
 * init makes, after which the compositor's offers are known. */
static int connect_client(struct client *cl)
{
   memset(cl, 0, sizeof(*cl));
   if (!(cl->dpy = wl_display_connect(comp.socket)))
      return 0;
   cl->reg = wl_display_get_registry(cl->dpy);
   wl_registry_add_listener(cl->reg, &reg_listener, cl);
   wl_display_roundtrip(cl->dpy);
   wl_display_roundtrip(cl->dpy);
   return 1;
}

static void disconnect_client(struct client *cl)
{
   wl_color_destroy(&cl->color);
   if (cl->compositor)
      wl_compositor_destroy(cl->compositor);
   if (cl->output)
      wl_output_destroy(cl->output);
   wl_registry_destroy(cl->reg);
   wl_display_roundtrip(cl->dpy);
   wl_display_disconnect(cl->dpy);
}

static void snapshot(int *get_surface, int *create_scrgb, int *set_desc,
      int *intent, int *violations)
{
   pthread_mutex_lock(&comp.lock);
   *get_surface  = comp.get_surface;
   *create_scrgb = comp.create_scrgb;
   *set_desc     = comp.set_desc;
   *intent       = comp.set_intent;
   *violations   = comp.violations;
   pthread_mutex_unlock(&comp.lock);
}

int main(void)
{
   struct client cl;
   struct wl_surface *surface;
   int gs, cs, sd, in, v;

   pthread_mutex_init(&comp.lock, NULL);
   setenv("XDG_RUNTIME_DIR", "/tmp", 0);

   printf("1. a compositor that takes Windows-scRGB\n");
   start(1, 1, ANSWER_READY);
   check("connected", connect_client(&cl));
   check("scRGB reported supported", wl_color_scrgb_supported(&cl.color));
   surface = wl_compositor_create_surface(cl.compositor);
   check("attach asked for", wl_color_attach_scrgb(&cl.color, surface));
   check("a second attach is refused, not sent",
         !wl_color_attach_scrgb(&cl.color, surface));
   wl_display_roundtrip(cl.dpy);   /* ready arrives; the tag goes out */
   wl_display_roundtrip(cl.dpy);   /* ...and reaches the compositor */
   snapshot(&gs, &cs, &sd, &in, &v);
   check("one colour-management surface, one description", gs == 1 && cs == 1);
   check("the description was set once, after ready", sd == 1);
   check("with the perceptual intent",
         in == WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
   check("the client knows it is tagged", !!(cl.color.flags & WL_COLOR_TAGGED));
   wl_color_destroy(&cl.color);
   wl_surface_destroy(surface);
   wl_display_roundtrip(cl.dpy);
   pthread_mutex_lock(&comp.lock);
   check("its colour-management object went before the surface",
         !comp.surface_destroyed_before_cm);
   pthread_mutex_unlock(&comp.lock);
   snapshot(&gs, &cs, &sd, &in, &v);
   check("no protocol rule broken", v == 0);
   if (v)
      printf("        (%s)\n", comp.violation);
   disconnect_client(&cl);
   stop();

   printf("2. a compositor without Windows-scRGB\n");
   start(0, 1, ANSWER_READY);
   check("connected", connect_client(&cl));
   check("scRGB reported unsupported", !wl_color_scrgb_supported(&cl.color));
   surface = wl_compositor_create_surface(cl.compositor);
   check("attach refused", !wl_color_attach_scrgb(&cl.color, surface));
   wl_display_roundtrip(cl.dpy);
   snapshot(&gs, &cs, &sd, &in, &v);
   check("nothing asked of the compositor", gs == 0 && cs == 0 && sd == 0);
   check("no protocol rule broken", v == 0);
   wl_surface_destroy(surface);
   disconnect_client(&cl);
   stop();

   printf("3. a compositor without the perceptual intent\n");
   start(1, 0, ANSWER_READY);
   check("connected", connect_client(&cl));
   check("scRGB reported unsupported", !wl_color_scrgb_supported(&cl.color));
   surface = wl_compositor_create_surface(cl.compositor);
   check("attach refused", !wl_color_attach_scrgb(&cl.color, surface));
   wl_display_roundtrip(cl.dpy);
   snapshot(&gs, &cs, &sd, &in, &v);
   check("nothing asked of the compositor", gs == 0 && cs == 0 && sd == 0);
   wl_surface_destroy(surface);
   disconnect_client(&cl);
   stop();

   printf("4. the description fails\n");
   start(1, 1, ANSWER_FAILED);
   check("connected", connect_client(&cl));
   surface = wl_compositor_create_surface(cl.compositor);
   check("attach asked for", wl_color_attach_scrgb(&cl.color, surface));
   wl_display_roundtrip(cl.dpy);
   wl_display_roundtrip(cl.dpy);
   snapshot(&gs, &cs, &sd, &in, &v);
   check("a failed description is never set", sd == 0);
   check("the client knows it failed", !!(cl.color.flags & WL_COLOR_FAILED)
         && !(cl.color.flags & WL_COLOR_TAGGED));
   check("no protocol rule broken", v == 0);
   wl_color_destroy(&cl.color);
   wl_surface_destroy(surface);
   disconnect_client(&cl);
   stop();

   printf("6. the display's peak, as the compositor describes the output\n");
   {
      static const unsigned peaks[2] = { 650, 0 };
      int k;
      for (k = 0; k < 2; k++)
      {
         start(1, 1, ANSWER_READY);
         pthread_mutex_lock(&comp.lock);
         comp.output_peak = peaks[k];
         comp.send_icc    = 1;
         pthread_mutex_unlock(&comp.lock);
         cb_peak = 0.0f;
         check("connected", connect_client(&cl));
         cl.color.peak_cb = peak_cb;
         check("the output asked about", wl_color_query_output(&cl.color, cl.output));
         check("a second query is refused, not sent",
               !wl_color_query_output(&cl.color, cl.output));
         wl_display_roundtrip(cl.dpy);   /* ready: information asked */
         wl_display_roundtrip(cl.dpy);   /* information arrives */
         if (peaks[k])
            check("peak from target luminance, not PQ's 10,000",
                  cl.color.output_peak_nits == 650.0f && cb_peak == 650.0f
                  && (cl.color.flags & WL_COLOR_OUTPUT_PEAK));
         else
            check("no target luminance: the peak stays unknown",
                  cl.color.output_peak_nits == 0.0f && cb_peak == 0.0f
                  && !(cl.color.flags & WL_COLOR_OUTPUT_PEAK));
         snapshot(&gs, &cs, &sd, &in, &v);
         check("no protocol rule broken", v == 0);
         disconnect_client(&cl);
         stop();
      }
   }

   printf("7. the frame's own luminances, not scRGB\n");
   {
      start(1, 1, ANSWER_READY);
      check("connected", connect_client(&cl));
      check("parametric reported supported", wl_color_parametric_supported(&cl.color));
      surface = wl_compositor_create_surface(cl.compositor);
      check("attach asked for",
            wl_color_attach_luminances(&cl.color, surface, 203.0f, 1000.0f));
      wl_display_roundtrip(cl.dpy);
      wl_display_roundtrip(cl.dpy);
      snapshot(&gs, &cs, &sd, &in, &v);
      pthread_mutex_lock(&comp.lock);
      check("one creator, described once, created once",
            comp.params_created == 1 && comp.params_tf == 1
            && comp.params_primaries == 1);
      check("extended-linear sRGB with 203 nits reference and 1000 peak",
            comp.params_reference == 203 && comp.params_max == 1000);
      check("no property set twice, none left unset",
            comp.params_already_set == 0 && comp.params_incomplete == 0);
      pthread_mutex_unlock(&comp.lock);
      check("Windows-scRGB was never asked for", cs == 0);
      check("the description was set on the surface once, after ready", sd == 1);
      check("and the client knows which tag it carries",
            (cl.color.flags & WL_COLOR_TAGGED)
            && (cl.color.flags & WL_COLOR_TAGGED_PARAMETRIC));
      check("no protocol rule broken", v == 0);
      if (v)
         printf("        (%s)\n", comp.violation);
      wl_color_destroy(&cl.color);
      wl_surface_destroy(surface);
      disconnect_client(&cl);
      stop();
   }

   printf("8. a compositor without the parametric features\n");
   {
      start_full(1, 1, 0, ANSWER_READY);
      check("connected", connect_client(&cl));
      check("parametric reported unsupported", !wl_color_parametric_supported(&cl.color));
      surface = wl_compositor_create_surface(cl.compositor);
      check("attach refused", !wl_color_attach_luminances(&cl.color, surface, 203.0f, 1000.0f));
      check("scRGB is still there to fall back on",
            wl_color_attach_scrgb(&cl.color, surface));
      wl_display_roundtrip(cl.dpy);
      wl_display_roundtrip(cl.dpy);
      snapshot(&gs, &cs, &sd, &in, &v);
      pthread_mutex_lock(&comp.lock);
      check("no creator was made", comp.params_created == 0);
      pthread_mutex_unlock(&comp.lock);
      check("the surface carries the scRGB tag", cs == 1 && sd == 1
            && !(cl.color.flags & WL_COLOR_TAGGED_PARAMETRIC));
      check("no protocol rule broken", v == 0);
      wl_color_destroy(&cl.color);
      wl_surface_destroy(surface);
      disconnect_client(&cl);
      stop();
   }

   printf("5. no colour management at all\n");
   {
      wl_color_t none;
      memset(&none, 0, sizeof(none));
      check("unsupported, attach refused, destroy harmless",
            !wl_color_scrgb_supported(&none)
            && !wl_color_attach_scrgb(&none, NULL));
      wl_color_destroy(&none);
   }

   pthread_mutex_destroy(&comp.lock);
   if (fails)
   {
      printf("wayland colour management: %d check(s) failed\n", fails);
      return 1;
   }
   printf("wayland colour management: the client keeps to the protocol\n");
   return 0;
}
