/* gfx/common/wayland_resize.h, what the Wayland context drivers do once
 * their buffers are resized, against a compositor in this process: the
 * compositor's configures count again whatever the scaling, and the
 * surface takes the buffer scale only when it is the whole-scale path
 * and the surface is new enough to take one. */

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <wayland-client.h>
#include <wayland-server.h>

#include "gfx/common/wayland_resize.h"

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (what)); \
         failures++; \
      } \
   } while (0)

/* ---- the compositor ------------------------------------------------ */

static struct
{
   struct wl_display *dpy;
   pthread_mutex_t lock;
   int scale_requests;
   int last_scale;
} comp;

static void srf_destroy(struct wl_client *c, struct wl_resource *r)
{ (void)c; wl_resource_destroy(r); }
static void srf_attach(struct wl_client *c, struct wl_resource *r,
      struct wl_resource *b, int32_t x, int32_t y)
{ (void)c; (void)r; (void)b; (void)x; (void)y; }
static void srf_damage(struct wl_client *c, struct wl_resource *r,
      int32_t x, int32_t y, int32_t w, int32_t h)
{ (void)c; (void)r; (void)x; (void)y; (void)w; (void)h; }
static void srf_frame(struct wl_client *c, struct wl_resource *r, uint32_t id)
{ (void)c; (void)r; (void)id; }
static void srf_region(struct wl_client *c, struct wl_resource *r,
      struct wl_resource *g)
{ (void)c; (void)r; (void)g; }
static void srf_commit(struct wl_client *c, struct wl_resource *r)
{ (void)c; (void)r; }
static void srf_transform(struct wl_client *c, struct wl_resource *r,
      int32_t t)
{ (void)c; (void)r; (void)t; }
static void srf_scale(struct wl_client *c, struct wl_resource *r,
      int32_t scale)
{
   (void)c; (void)r;
   pthread_mutex_lock(&comp.lock);
   comp.scale_requests++;
   comp.last_scale = scale;
   pthread_mutex_unlock(&comp.lock);
}
static void srf_damage_buffer(struct wl_client *c, struct wl_resource *r,
      int32_t x, int32_t y, int32_t w, int32_t h)
{ (void)c; (void)r; (void)x; (void)y; (void)w; (void)h; }
static void srf_offset(struct wl_client *c, struct wl_resource *r,
      int32_t x, int32_t y)
{ (void)c; (void)r; (void)x; (void)y; }

static const struct wl_surface_interface surface_impl = {
   srf_destroy, srf_attach, srf_damage, srf_frame, srf_region,
   srf_region, srf_commit, srf_transform, srf_scale, srf_damage_buffer,
   srf_offset
};

static void cmp_create_surface(struct wl_client *c, struct wl_resource *r,
      uint32_t id)
{
   struct wl_resource *s = wl_resource_create(c, &wl_surface_interface,
         wl_resource_get_version(r), id);
   wl_resource_set_implementation(s, &surface_impl, NULL, NULL);
}
static void cmp_create_region(struct wl_client *c, struct wl_resource *r,
      uint32_t id)
{ (void)c; (void)r; (void)id; }

static const struct wl_compositor_interface compositor_impl = {
   cmp_create_surface, cmp_create_region
};

static void bind_compositor(struct wl_client *c, void *data,
      uint32_t version, uint32_t id)
{
   struct wl_resource *r = wl_resource_create(c, &wl_compositor_interface,
         (int)version, id);
   (void)data;
   wl_resource_set_implementation(r, &compositor_impl, NULL, NULL);
}

static void *compositor_run(void *arg)
{
   (void)arg;
   wl_display_run(comp.dpy);
   return NULL;
}

/* ---- the client ---------------------------------------------------- */

static struct wl_compositor *compositor;
static uint32_t compositor_version;

static void reg_global(void *data, struct wl_registry *reg, uint32_t name,
      const char *interface, uint32_t version)
{
   (void)data;
   if (!strcmp(interface, wl_compositor_interface.name))
   {
      compositor_version = version;
      compositor = (struct wl_compositor*)wl_registry_bind(reg, name,
            &wl_compositor_interface, version);
   }
}
static void reg_remove(void *data, struct wl_registry *reg, uint32_t name)
{ (void)data; (void)reg; (void)name; }
static const struct wl_registry_listener registry_listener = {
   reg_global, reg_remove
};

static void scale_seen(int *requests, int *last)
{
   pthread_mutex_lock(&comp.lock);
   *requests = comp.scale_requests;
   *last     = comp.last_scale;
   comp.scale_requests = 0;
   comp.last_scale     = 0;
   pthread_mutex_unlock(&comp.lock);
}

/* One compositor advertising wl_compositor at @version; a surface of
 * that version resized with and without fractional scaling. */
static void run(uint32_t version)
{
   struct wl_display *dpy;
   struct wl_registry *reg;
   struct wl_surface *surface;
   pthread_t thread;
   const char *socket;
   bool ignore;
   int requests, last;
   char what[128];
   bool takes_scale = version >= WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION;

   comp.dpy = wl_display_create();
   socket   = comp.dpy ? wl_display_add_socket_auto(comp.dpy) : NULL;
   /* The in-process compositor's socket lives in XDG_RUNTIME_DIR */
   CHECK(socket != NULL, "no compositor socket (is XDG_RUNTIME_DIR set?)");
   if (!socket)
   {
      if (comp.dpy)
         wl_display_destroy(comp.dpy);
      return;
   }
   wl_global_create(comp.dpy, &wl_compositor_interface, (int)version,
         NULL, bind_compositor);
   pthread_create(&thread, NULL, compositor_run, NULL);

   dpy        = wl_display_connect(socket);
   compositor = NULL;
   reg        = wl_display_get_registry(dpy);
   wl_registry_add_listener(reg, &registry_listener, NULL);
   wl_display_roundtrip(dpy);
   CHECK(compositor && compositor_version == version, "no wl_compositor");
   if (!compositor)
      return;
   surface = wl_compositor_create_surface(compositor);

   /* Whole scale: the surface takes it, the configures count again. */
   ignore = true;
   wl_surface_resized(surface, false, 2, &ignore);
   wl_display_roundtrip(dpy);
   scale_seen(&requests, &last);
   snprintf(what, sizeof(what),
         "v%u, whole scale: configures still ignored", version);
   CHECK(!ignore, what);
   snprintf(what, sizeof(what),
         "v%u, whole scale: %d buffer scale requests (last %d)",
         version, requests, last);
   CHECK(takes_scale ? (requests == 1 && last == 2) : requests == 0, what);

   /* Fractional: the viewport carries the scale, the configures count
    * again all the same. */
   ignore = true;
   wl_surface_resized(surface, true, 2, &ignore);
   wl_display_roundtrip(dpy);
   scale_seen(&requests, &last);
   snprintf(what, sizeof(what),
         "v%u, fractional: configures still ignored", version);
   CHECK(!ignore, what);
   snprintf(what, sizeof(what),
         "v%u, fractional: %d buffer scale requests", version, requests);
   CHECK(requests == 0, what);

   CHECK(wl_display_get_error(dpy) == 0, "protocol error");

   wl_surface_destroy(surface);
   wl_compositor_destroy(compositor);
   wl_registry_destroy(reg);
   wl_display_roundtrip(dpy);
   wl_display_disconnect(dpy);
   wl_display_terminate(comp.dpy);
   pthread_join(thread, NULL);
   wl_display_destroy(comp.dpy);
}

int main(void)
{
   pthread_mutex_init(&comp.lock, NULL);
   run(4);   /* what the context binds today */
   run(2);   /* older than wl_surface.set_buffer_scale */
   pthread_mutex_destroy(&comp.lock);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] wayland_resize_test\n");
   return 0;
}
