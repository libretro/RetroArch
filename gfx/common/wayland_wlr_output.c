/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>

#include "wayland_wlr_output.h"
#include "wayland/wlr-output-management-unstable-v1.h"

#include "../../verbosity.h"

/* The newest version whose events this file handles */
#define WLR_MANAGER_VERSION 4

static const struct zwlr_output_head_v1_listener wlr_output_head_listener;
static const struct zwlr_output_mode_v1_listener wlr_output_mode_listener;

/* Objects the compositor has finished with: from version 3 they are
 * released with a request, before it only forgotten */
static void wlr_mode_release(struct zwlr_output_mode_v1 *mode)
{
   if (zwlr_output_mode_v1_get_version(mode) >= 3)
      zwlr_output_mode_v1_release(mode);
   else
      zwlr_output_mode_v1_destroy(mode);
}

static void wlr_head_free(wlr_output_head_info_t *h)
{
   unsigned i;
   for (i = 0; i < h->nmodes; i++)
      wlr_mode_release(h->modes[i].mode);
   free(h->modes);
   if (h->head)
   {
      if (zwlr_output_head_v1_get_version(h->head) >= 3)
         zwlr_output_head_v1_release(h->head);
      else
         zwlr_output_head_v1_destroy(h->head);
   }
   memset(h, 0, sizeof(*h));
}

static wlr_output_mode_info_t *wlr_mode_of(wlr_output_head_info_t *h,
      struct zwlr_output_mode_v1 *mode)
{
   unsigned i;
   for (i = 0; i < h->nmodes; i++)
      if (h->modes[i].mode == mode)
         return &h->modes[i];
   return NULL;
}

/* ---- modes: user data is the head they belong to ---- */

static void wlr_output_mode_size(void *data, struct zwlr_output_mode_v1 *obj,
      int32_t width, int32_t height)
{
   wlr_output_mode_info_t *m = wlr_mode_of((wlr_output_head_info_t*)data, obj);
   if (m)
   {
      m->width  = width;
      m->height = height;
   }
}

static void wlr_output_mode_refresh(void *data,
      struct zwlr_output_mode_v1 *obj, int32_t refresh)
{
   wlr_output_mode_info_t *m = wlr_mode_of((wlr_output_head_info_t*)data, obj);
   if (m)
      m->refresh_mhz = refresh;
}

/* The mode is gone: forget it and let go of it */
static void wlr_output_mode_finished(void *data,
      struct zwlr_output_mode_v1 *obj)
{
   unsigned i;
   wlr_output_head_info_t *h = (wlr_output_head_info_t*)data;
   for (i = 0; i < h->nmodes; i++)
      if (h->modes[i].mode == obj)
      {
         memmove(&h->modes[i], &h->modes[i + 1],
               (h->nmodes - i - 1) * sizeof(h->modes[0]));
         h->nmodes--;
         break;
      }
   if (h->current == obj)
      h->current = NULL;
   wlr_mode_release(obj);
}

/* ---- heads ---- */

static void wlr_output_head_name(void *data, struct zwlr_output_head_v1 *obj,
      const char *name)
{
   (void)obj;
   strlcpy(((wlr_output_head_info_t*)data)->name, name ? name : "",
         sizeof(((wlr_output_head_info_t*)data)->name));
}

static void wlr_output_head_mode(void *data, struct zwlr_output_head_v1 *obj,
      struct zwlr_output_mode_v1 *mode)
{
   wlr_output_head_info_t *h = (wlr_output_head_info_t*)data;
   (void)obj;
   if (h->nmodes == h->capacity)
   {
      unsigned cap               = h->capacity ? h->capacity * 2 : 16;
      wlr_output_mode_info_t *nm = (wlr_output_mode_info_t*)realloc(
            h->modes, cap * sizeof(*nm));
      if (!nm)
      {
         wlr_mode_release(mode);
         return;
      }
      h->modes    = nm;
      h->capacity = cap;
   }
   memset(&h->modes[h->nmodes], 0, sizeof(h->modes[0]));
   h->modes[h->nmodes++].mode = mode;
   zwlr_output_mode_v1_add_listener(mode, &wlr_output_mode_listener, h);
}

static void wlr_output_head_enabled(void *data,
      struct zwlr_output_head_v1 *obj, int32_t enabled)
{
   wlr_output_head_info_t *h = (wlr_output_head_info_t*)data;
   (void)obj;
   if (enabled)
      h->flags |=  WLR_HEAD_ENABLED;
   else
      h->flags &= ~WLR_HEAD_ENABLED;
}

static void wlr_output_head_current_mode(void *data,
      struct zwlr_output_head_v1 *obj, struct zwlr_output_mode_v1 *mode)
{
   (void)obj;
   ((wlr_output_head_info_t*)data)->current = mode;
}

/* The head is gone: its entry is emptied, the table only grows */
static void wlr_output_head_finished(void *data,
      struct zwlr_output_head_v1 *obj)
{
   (void)obj;
   wlr_head_free((wlr_output_head_info_t*)data);
}

/* ---- the manager ---- */

static void wlr_output_manager_head(void *data,
      struct zwlr_output_manager_v1 *obj, struct zwlr_output_head_v1 *head)
{
   unsigned i;
   wlr_outputs_t *wo         = (wlr_outputs_t*)data;
   wlr_output_head_info_t *h = NULL;
   (void)obj;
   for (i = 0; i < wo->nheads && !h; i++)
      if (!wo->heads[i].head)
         h = &wo->heads[i];
   if (!h && wo->nheads < WLR_OUTPUT_MAX_HEADS)
      h = &wo->heads[wo->nheads++];
   if (!h)
   {
      zwlr_output_head_v1_destroy(head);
      return;
   }
   memset(h, 0, sizeof(*h));
   h->head = head;
   zwlr_output_head_v1_add_listener(head, &wlr_output_head_listener, h);
}

static void wlr_output_manager_done(void *data,
      struct zwlr_output_manager_v1 *obj, uint32_t serial)
{
   wlr_outputs_t *wo = (wlr_outputs_t*)data;
   (void)obj;
   wo->serial      = serial;
   wo->have_serial = true;
}

static void wlr_output_manager_finished(void *data,
      struct zwlr_output_manager_v1 *obj)
{
   wlr_outputs_t *wo = (wlr_outputs_t*)data;
   zwlr_output_manager_v1_destroy(obj);
   wo->manager     = NULL;
   wo->have_serial = false;
}

/* ---- configurations: the outcome of a switch ---- */

static void wlr_output_configuration_succeeded(void *data,
      struct zwlr_output_configuration_v1 *obj)
{
   (void)data;
   RARCH_LOG("[Wayland] The compositor applied the mode switch.\n");
   zwlr_output_configuration_v1_destroy(obj);
}

static void wlr_output_configuration_failed(void *data,
      struct zwlr_output_configuration_v1 *obj)
{
   (void)data;
   RARCH_WARN("[Wayland] The compositor refused the mode switch.\n");
   zwlr_output_configuration_v1_destroy(obj);
}

static void wlr_output_configuration_cancelled(void *data,
      struct zwlr_output_configuration_v1 *obj)
{
   (void)data;
   RARCH_WARN("[Wayland] The outputs changed under the mode switch; the compositor cancelled it.\n");
   zwlr_output_configuration_v1_destroy(obj);
}

/* ---- the listener tables, in the protocol's event order; events this
 * file does not use are taken and left ---- */
static const struct zwlr_output_manager_v1_listener wlr_output_manager_listener = {
   wlr_output_manager_head,
   wlr_output_manager_done,
   wlr_output_manager_finished
};

static void wlr_output_head_ignore_description(void *data, struct zwlr_output_head_v1 *obj, const char *description)
{ (void)data; (void)obj; (void)description; }

static void wlr_output_head_ignore_physical_size(void *data, struct zwlr_output_head_v1 *obj, int32_t width, int32_t height)
{ (void)data; (void)obj; (void)width; (void)height; }

static void wlr_output_head_ignore_position(void *data, struct zwlr_output_head_v1 *obj, int32_t x, int32_t y)
{ (void)data; (void)obj; (void)x; (void)y; }

static void wlr_output_head_ignore_transform(void *data, struct zwlr_output_head_v1 *obj, int32_t transform)
{ (void)data; (void)obj; (void)transform; }

static void wlr_output_head_ignore_scale(void *data, struct zwlr_output_head_v1 *obj, wl_fixed_t scale)
{ (void)data; (void)obj; (void)scale; }

static void wlr_output_head_ignore_make(void *data, struct zwlr_output_head_v1 *obj, const char *make)
{ (void)data; (void)obj; (void)make; }

static void wlr_output_head_ignore_model(void *data, struct zwlr_output_head_v1 *obj, const char *model)
{ (void)data; (void)obj; (void)model; }

static void wlr_output_head_ignore_serial_number(void *data, struct zwlr_output_head_v1 *obj, const char *serial_number)
{ (void)data; (void)obj; (void)serial_number; }

static void wlr_output_head_ignore_adaptive_sync(void *data, struct zwlr_output_head_v1 *obj, uint32_t state)
{ (void)data; (void)obj; (void)state; }

static const struct zwlr_output_head_v1_listener wlr_output_head_listener = {
   wlr_output_head_name,
   wlr_output_head_ignore_description,
   wlr_output_head_ignore_physical_size,
   wlr_output_head_mode,
   wlr_output_head_enabled,
   wlr_output_head_current_mode,
   wlr_output_head_ignore_position,
   wlr_output_head_ignore_transform,
   wlr_output_head_ignore_scale,
   wlr_output_head_finished,
   wlr_output_head_ignore_make,
   wlr_output_head_ignore_model,
   wlr_output_head_ignore_serial_number,
   wlr_output_head_ignore_adaptive_sync
};

static void wlr_output_mode_ignore_preferred(void *data, struct zwlr_output_mode_v1 *obj)
{ (void)data; (void)obj; }

static const struct zwlr_output_mode_v1_listener wlr_output_mode_listener = {
   wlr_output_mode_size,
   wlr_output_mode_refresh,
   wlr_output_mode_ignore_preferred,
   wlr_output_mode_finished
};

static const struct zwlr_output_configuration_v1_listener wlr_output_configuration_listener = {
   wlr_output_configuration_succeeded,
   wlr_output_configuration_failed,
   wlr_output_configuration_cancelled
};


bool wlr_outputs_bind(wlr_outputs_t *wo, struct wl_registry *registry,
      uint32_t name, const char *interface, uint32_t version)
{
   if (strcmp(interface, zwlr_output_manager_v1_interface.name))
      return false;
   if (!wo->manager)
   {
      wo->manager = (struct zwlr_output_manager_v1*)wl_registry_bind(
            registry, name, &zwlr_output_manager_v1_interface,
            version < WLR_MANAGER_VERSION ? version : WLR_MANAGER_VERSION);
      if (wo->manager)
         zwlr_output_manager_v1_add_listener(wo->manager,
               &wlr_output_manager_listener, wo);
   }
   return true;
}

static wlr_output_head_info_t *wlr_pick(wlr_outputs_t *wo,
      const char *connector)
{
   unsigned i;
   wlr_output_head_info_t *first = NULL;
   for (i = 0; i < wo->nheads; i++)
   {
      wlr_output_head_info_t *h = &wo->heads[i];
      if (!h->head || !h->nmodes)
         continue;
      if (connector && *connector && !strcmp(h->name, connector))
         return h;
      if (!first && (h->flags & WLR_HEAD_ENABLED))
         first = h;
   }
   return first;
}

bool wlr_outputs_ready(const wlr_outputs_t *wo)
{
   return wo && wo->manager && wo->have_serial
      && wlr_pick((wlr_outputs_t*)wo, NULL) != NULL;
}

static int wlr_mode_cmp(const void *a, const void *b)
{
   const video_display_config_t *x = (const video_display_config_t*)a;
   const video_display_config_t *y = (const video_display_config_t*)b;
   unsigned xw = VIDEO_SCALE_W(x->dims), yw = VIDEO_SCALE_W(y->dims);
   unsigned xh = VIDEO_SCALE_H(x->dims), yh = VIDEO_SCALE_H(y->dims);
   if (xw != yw)
      return xw < yw ? -1 : 1;
   if (xh != yh)
      return xh < yh ? -1 : 1;
   if (x->refreshrate_float != y->refreshrate_float)
      return x->refreshrate_float < y->refreshrate_float ? -1 : 1;
   return 0;
}

video_display_config_t *wlr_outputs_resolution_list(wlr_outputs_t *wo,
      const char *connector, unsigned *len)
{
   unsigned i, n = 0;
   video_display_config_t *list = NULL;
   wlr_output_head_info_t *h    = NULL;

   *len = 0;
   if (!wlr_outputs_ready(wo) || !(h = wlr_pick(wo, connector)))
      return NULL;
   if (!(list = (video_display_config_t*)calloc(h->nmodes, sizeof(*list))))
      return NULL;
   for (i = 0; i < h->nmodes; i++)
   {
      const wlr_output_mode_info_t *m = &h->modes[i];
      if (m->width <= 0 || m->height <= 0 || m->refresh_mhz <= 0)
         continue;
      list[n].dims              = VIDEO_SCALE_PACK(m->width, m->height);
      list[n].bpp               = 32;
      list[n].refreshrate_float = (float)m->refresh_mhz / 1000.0f;
      list[n].refreshrate       = (unsigned)((m->refresh_mhz + 500) / 1000);
      list[n].current           = (m->mode == h->current);
      n++;
   }
   qsort(list, n, sizeof(*list), wlr_mode_cmp);
   for (i = 0; i < n; i++)
      list[i].idx = i;
   *len = n;
   return list;
}

bool wlr_outputs_set_mode(wlr_outputs_t *wo, const char *connector,
      unsigned width, unsigned height, int int_hz, float hz)
{
   unsigned i;
   float want_hz;
   struct zwlr_output_configuration_v1 *config;
   const wlr_output_mode_info_t *cur  = NULL;
   const wlr_output_mode_info_t *best = NULL;
   wlr_output_head_info_t *target     = NULL;

   if (!wlr_outputs_ready(wo) || !(target = wlr_pick(wo, connector)))
      return false;
   for (i = 0; i < target->nmodes; i++)
      if (target->modes[i].mode == target->current)
         cur = &target->modes[i];
   if (!width)
      width  = cur ? (unsigned)cur->width  : 0;
   if (!height)
      height = cur ? (unsigned)cur->height : 0;
   want_hz = hz > 0.0f ? hz : (float)int_hz;
   if (want_hz <= 0.0f && cur)
      want_hz = (float)cur->refresh_mhz / 1000.0f;

   /* The listed mode of that size nearest the rate */
   for (i = 0; i < target->nmodes; i++)
   {
      const wlr_output_mode_info_t *m = &target->modes[i];
      float d, bd;
      if ((unsigned)m->width != width || (unsigned)m->height != height)
         continue;
      if (!best)
      {
         best = m;
         continue;
      }
      d  = (float)m->refresh_mhz    / 1000.0f - want_hz;
      bd = (float)best->refresh_mhz / 1000.0f - want_hz;
      if ((d < 0 ? -d : d) < (bd < 0 ? -bd : bd))
         best = m;
   }
   if (!best)
   {
      RARCH_WARN("[Wayland] The compositor lists no %ux%u mode on %s.\n",
            width, height, target->name[0] ? target->name : "its output");
      return false;
   }
   if (best->mode == target->current)
      return true;

   if (!(config = zwlr_output_manager_v1_create_configuration(
               wo->manager, wo->serial)))
      return false;
   zwlr_output_configuration_v1_add_listener(config,
         &wlr_output_configuration_listener, NULL);
   /* Every head, exactly once: the target with its new mode, the other
    * enabled heads as they are, the disabled ones disabled. */
   for (i = 0; i < wo->nheads; i++)
   {
      wlr_output_head_info_t *h = &wo->heads[i];
      if (!h->head)
         continue;
      if (h == target || (h->flags & WLR_HEAD_ENABLED))
      {
         struct zwlr_output_configuration_head_v1 *ch =
            zwlr_output_configuration_v1_enable_head(config, h->head);
         if (ch)
         {
            if (h == target)
               zwlr_output_configuration_head_v1_set_mode(ch, best->mode);
            /* It has no destructor request; the configuration owns it */
            zwlr_output_configuration_head_v1_destroy(ch);
         }
      }
      else
         zwlr_output_configuration_v1_disable_head(config, h->head);
   }
   zwlr_output_configuration_v1_apply(config);
   RARCH_LOG("[Wayland] Asked the compositor for %dx%d at %.3f Hz on %s.\n",
         best->width, best->height, (float)best->refresh_mhz / 1000.0f,
         target->name[0] ? target->name : "its output");
   return true;
}

void wlr_outputs_destroy(wlr_outputs_t *wo)
{
   unsigned i;
   if (!wo)
      return;
   for (i = 0; i < wo->nheads; i++)
      if (wo->heads[i].head)
         wlr_head_free(&wo->heads[i]);
   if (wo->manager)
      zwlr_output_manager_v1_destroy(wo->manager);
   memset(wo, 0, sizeof(*wo));
}
