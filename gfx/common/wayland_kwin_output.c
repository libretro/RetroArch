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

#include "wayland_kwin_output.h"
#include "wayland/kde-output-device-v2.h"
#include "wayland/kde-output-management-v2.h"

#include "../../verbosity.h"

/* The newest versions whose events this file handles */
#define KWIN_DEVICE_VERSION     26
#define KWIN_MANAGEMENT_VERSION 22

/* A mode's user data is the device it belongs to */
static kwin_output_mode_t *kwin_mode_of(kwin_output_device_t *dev,
      struct kde_output_device_mode_v2 *mode)
{
   unsigned i;
   for (i = 0; i < dev->nmodes; i++)
      if (dev->modes[i].mode == mode)
         return &dev->modes[i];
   return NULL;
}

static const struct kde_output_device_mode_v2_listener
   kwin_output_device_mode_listener_real;

/* ---- modes ---- */

static void kwin_output_device_mode_size(void *data,
      struct kde_output_device_mode_v2 *obj, int32_t width, int32_t height)
{
   kwin_output_mode_t *m = kwin_mode_of((kwin_output_device_t*)data, obj);
   if (m)
   {
      m->width  = width;
      m->height = height;
   }
}

static void kwin_output_device_mode_refresh(void *data,
      struct kde_output_device_mode_v2 *obj, int32_t refresh)
{
   kwin_output_mode_t *m = kwin_mode_of((kwin_output_device_t*)data, obj);
   if (m)
      m->refresh_mhz = refresh;
}

static void kwin_output_device_mode_preferred(void *data,
      struct kde_output_device_mode_v2 *obj)
{
   (void)data; (void)obj;
}

/* The mode is gone: forget it and let go of its proxy */
static void kwin_output_device_mode_removed(void *data,
      struct kde_output_device_mode_v2 *obj)
{
   unsigned i;
   kwin_output_device_t *dev = (kwin_output_device_t*)data;
   for (i = 0; i < dev->nmodes; i++)
      if (dev->modes[i].mode == obj)
      {
         memmove(&dev->modes[i], &dev->modes[i + 1],
               (dev->nmodes - i - 1) * sizeof(dev->modes[0]));
         dev->nmodes--;
         break;
      }
   if (dev->current == obj)
      dev->current = NULL;
   kde_output_device_mode_v2_destroy(obj);
}

/* ---- devices ---- */

static void kwin_output_device_current_mode(void *data,
      struct kde_output_device_v2 *obj, struct kde_output_device_mode_v2 *mode)
{
   kwin_output_device_t *dev = (kwin_output_device_t*)data;
   (void)obj;
   dev->current = mode;
}

static void kwin_output_device_mode(void *data,
      struct kde_output_device_v2 *obj, struct kde_output_device_mode_v2 *mode)
{
   kwin_output_device_t *dev = (kwin_output_device_t*)data;
   (void)obj;
   if (dev->nmodes == dev->capacity)
   {
      unsigned cap           = dev->capacity ? dev->capacity * 2 : 16;
      kwin_output_mode_t *nm = (kwin_output_mode_t*)realloc(dev->modes,
            cap * sizeof(*nm));
      if (!nm)
      {
         kde_output_device_mode_v2_destroy(mode);
         return;
      }
      dev->modes    = nm;
      dev->capacity = cap;
   }
   memset(&dev->modes[dev->nmodes], 0, sizeof(dev->modes[0]));
   dev->modes[dev->nmodes++].mode = mode;
   kde_output_device_mode_v2_add_listener(mode,
         &kwin_output_device_mode_listener_real, dev);
}

static void kwin_output_device_done(void *data,
      struct kde_output_device_v2 *obj)
{
   (void)obj;
   ((kwin_output_device_t*)data)->flags |= KWIN_DEVICE_DONE;
}

static void kwin_output_device_enabled(void *data,
      struct kde_output_device_v2 *obj, int32_t enabled)
{
   kwin_output_device_t *dev = (kwin_output_device_t*)data;
   (void)obj;
   if (enabled)
      dev->flags |=  KWIN_DEVICE_ENABLED;
   else
      dev->flags &= ~KWIN_DEVICE_ENABLED;
}

static void kwin_output_device_name(void *data,
      struct kde_output_device_v2 *obj, const char *name)
{
   (void)obj;
   strlcpy(((kwin_output_device_t*)data)->name, name ? name : "",
         sizeof(((kwin_output_device_t*)data)->name));
}

static void kwin_device_free(kwin_output_device_t *dev)
{
   unsigned i;
   for (i = 0; i < dev->nmodes; i++)
      kde_output_device_mode_v2_destroy(dev->modes[i].mode);
   free(dev->modes);
   if (dev->device)
      kde_output_device_v2_destroy(dev->device);
   memset(dev, 0, sizeof(*dev));
}

/* The output is gone (a destructor event from version 21) */
static void kwin_output_device_removed(void *data,
      struct kde_output_device_v2 *obj)
{
   kwin_output_device_t *dev = (kwin_output_device_t*)data;
   (void)obj;
   /* Its entry stays in place, emptied; the device table only grows */
   kwin_device_free(dev);
}

/* ---- the registry (version 21 on): outputs arrive through it ---- */

static void kwin_output_device_registry_output(void *data,
      struct kde_output_device_registry_v2 *obj,
      struct kde_output_device_v2 *device);

static void kwin_output_device_registry_finished(void *data,
      struct kde_output_device_registry_v2 *obj)
{
   kwin_outputs_t *kw = (kwin_outputs_t*)data;
   kde_output_device_registry_v2_destroy(obj);
   kw->registry = NULL;
}

/* ---- configurations: the outcome of a switch ---- */

static void kwin_output_configuration_applied(void *data,
      struct kde_output_configuration_v2 *obj)
{
   (void)data;
   RARCH_LOG("[Wayland] KWin applied the mode switch.\n");
   kde_output_configuration_v2_destroy(obj);
}

static void kwin_output_configuration_failed(void *data,
      struct kde_output_configuration_v2 *obj)
{
   (void)data;
   RARCH_WARN("[Wayland] KWin refused the mode switch.\n");
   kde_output_configuration_v2_destroy(obj);
}

static void kwin_output_configuration_failure_reason(void *data,
      struct kde_output_configuration_v2 *obj, const char *reason)
{
   (void)data; (void)obj;
   RARCH_WARN("[Wayland] KWin: %s.\n", reason ? reason : "no reason given");
}

/* ---- the listener tables, in the protocol's event order; events this
 * file does not use are taken and left ---- */
static const struct kde_output_device_registry_v2_listener kwin_output_device_registry_listener = {
   kwin_output_device_registry_finished,
   kwin_output_device_registry_output
};

static void kwin_output_device_ignore_geometry(void *data, struct kde_output_device_v2 *obj, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform)
{ (void)data; (void)obj; (void)x; (void)y; (void)physical_width; (void)physical_height; (void)subpixel; (void)make; (void)model; (void)transform; }

static void kwin_output_device_ignore_scale(void *data, struct kde_output_device_v2 *obj, wl_fixed_t factor)
{ (void)data; (void)obj; (void)factor; }

static void kwin_output_device_ignore_edid(void *data, struct kde_output_device_v2 *obj, const char *raw)
{ (void)data; (void)obj; (void)raw; }

static void kwin_output_device_ignore_uuid(void *data, struct kde_output_device_v2 *obj, const char *uuid)
{ (void)data; (void)obj; (void)uuid; }

static void kwin_output_device_ignore_serial_number(void *data, struct kde_output_device_v2 *obj, const char *serialNumber)
{ (void)data; (void)obj; (void)serialNumber; }

static void kwin_output_device_ignore_eisa_id(void *data, struct kde_output_device_v2 *obj, const char *eisaId)
{ (void)data; (void)obj; (void)eisaId; }

static void kwin_output_device_ignore_capabilities(void *data, struct kde_output_device_v2 *obj, uint32_t flags)
{ (void)data; (void)obj; (void)flags; }

static void kwin_output_device_ignore_overscan(void *data, struct kde_output_device_v2 *obj, uint32_t overscan)
{ (void)data; (void)obj; (void)overscan; }

static void kwin_output_device_ignore_vrr_policy(void *data, struct kde_output_device_v2 *obj, uint32_t vrr_policy)
{ (void)data; (void)obj; (void)vrr_policy; }

static void kwin_output_device_ignore_rgb_range(void *data, struct kde_output_device_v2 *obj, uint32_t rgb_range)
{ (void)data; (void)obj; (void)rgb_range; }

static void kwin_output_device_ignore_high_dynamic_range(void *data, struct kde_output_device_v2 *obj, uint32_t hdr_enabled)
{ (void)data; (void)obj; (void)hdr_enabled; }

static void kwin_output_device_ignore_sdr_brightness(void *data, struct kde_output_device_v2 *obj, uint32_t sdr_brightness)
{ (void)data; (void)obj; (void)sdr_brightness; }

static void kwin_output_device_ignore_wide_color_gamut(void *data, struct kde_output_device_v2 *obj, uint32_t wcg_enabled)
{ (void)data; (void)obj; (void)wcg_enabled; }

static void kwin_output_device_ignore_auto_rotate_policy(void *data, struct kde_output_device_v2 *obj, uint32_t policy)
{ (void)data; (void)obj; (void)policy; }

static void kwin_output_device_ignore_icc_profile_path(void *data, struct kde_output_device_v2 *obj, const char *profile_path)
{ (void)data; (void)obj; (void)profile_path; }

static void kwin_output_device_ignore_brightness_metadata(void *data, struct kde_output_device_v2 *obj, uint32_t max_peak_brightness, uint32_t max_frame_average_brightness, uint32_t min_brightness)
{ (void)data; (void)obj; (void)max_peak_brightness; (void)max_frame_average_brightness; (void)min_brightness; }

static void kwin_output_device_ignore_brightness_overrides(void *data, struct kde_output_device_v2 *obj, int32_t max_peak_brightness, int32_t max_average_brightness, int32_t min_brightness)
{ (void)data; (void)obj; (void)max_peak_brightness; (void)max_average_brightness; (void)min_brightness; }

static void kwin_output_device_ignore_sdr_gamut_wideness(void *data, struct kde_output_device_v2 *obj, uint32_t gamut_wideness)
{ (void)data; (void)obj; (void)gamut_wideness; }

static void kwin_output_device_ignore_color_profile_source(void *data, struct kde_output_device_v2 *obj, uint32_t source)
{ (void)data; (void)obj; (void)source; }

static void kwin_output_device_ignore_brightness(void *data, struct kde_output_device_v2 *obj, uint32_t brightness)
{ (void)data; (void)obj; (void)brightness; }

static void kwin_output_device_ignore_color_power_tradeoff(void *data, struct kde_output_device_v2 *obj, uint32_t preference)
{ (void)data; (void)obj; (void)preference; }

static void kwin_output_device_ignore_dimming(void *data, struct kde_output_device_v2 *obj, uint32_t multiplier)
{ (void)data; (void)obj; (void)multiplier; }

static void kwin_output_device_ignore_replication_source(void *data, struct kde_output_device_v2 *obj, const char *source)
{ (void)data; (void)obj; (void)source; }

static void kwin_output_device_ignore_ddc_ci_allowed(void *data, struct kde_output_device_v2 *obj, uint32_t allowed)
{ (void)data; (void)obj; (void)allowed; }

static void kwin_output_device_ignore_max_bits_per_color(void *data, struct kde_output_device_v2 *obj, uint32_t max_bpc)
{ (void)data; (void)obj; (void)max_bpc; }

static void kwin_output_device_ignore_max_bits_per_color_range(void *data, struct kde_output_device_v2 *obj, uint32_t min_value, uint32_t max_value)
{ (void)data; (void)obj; (void)min_value; (void)max_value; }

static void kwin_output_device_ignore_automatic_max_bits_per_color_limit(void *data, struct kde_output_device_v2 *obj, uint32_t max_bpc_limit)
{ (void)data; (void)obj; (void)max_bpc_limit; }

static void kwin_output_device_ignore_edr_policy(void *data, struct kde_output_device_v2 *obj, uint32_t policy)
{ (void)data; (void)obj; (void)policy; }

static void kwin_output_device_ignore_sharpness(void *data, struct kde_output_device_v2 *obj, uint32_t sharpness)
{ (void)data; (void)obj; (void)sharpness; }

static void kwin_output_device_ignore_priority(void *data, struct kde_output_device_v2 *obj, uint32_t priority)
{ (void)data; (void)obj; (void)priority; }

static void kwin_output_device_ignore_auto_brightness(void *data, struct kde_output_device_v2 *obj, uint32_t enabled)
{ (void)data; (void)obj; (void)enabled; }

static void kwin_output_device_ignore_hdr_icc_profile_path(void *data, struct kde_output_device_v2 *obj, const char *profile_path)
{ (void)data; (void)obj; (void)profile_path; }

static void kwin_output_device_ignore_hdr_color_profile_source(void *data, struct kde_output_device_v2 *obj, uint32_t source)
{ (void)data; (void)obj; (void)source; }

static void kwin_output_device_ignore_abm_level(void *data, struct kde_output_device_v2 *obj, uint32_t level)
{ (void)data; (void)obj; (void)level; }

static const struct kde_output_device_v2_listener kwin_output_device_listener = {
   kwin_output_device_ignore_geometry,
   kwin_output_device_current_mode,
   kwin_output_device_mode,
   kwin_output_device_done,
   kwin_output_device_ignore_scale,
   kwin_output_device_ignore_edid,
   kwin_output_device_enabled,
   kwin_output_device_ignore_uuid,
   kwin_output_device_ignore_serial_number,
   kwin_output_device_ignore_eisa_id,
   kwin_output_device_ignore_capabilities,
   kwin_output_device_ignore_overscan,
   kwin_output_device_ignore_vrr_policy,
   kwin_output_device_ignore_rgb_range,
   kwin_output_device_name,
   kwin_output_device_ignore_high_dynamic_range,
   kwin_output_device_ignore_sdr_brightness,
   kwin_output_device_ignore_wide_color_gamut,
   kwin_output_device_ignore_auto_rotate_policy,
   kwin_output_device_ignore_icc_profile_path,
   kwin_output_device_ignore_brightness_metadata,
   kwin_output_device_ignore_brightness_overrides,
   kwin_output_device_ignore_sdr_gamut_wideness,
   kwin_output_device_ignore_color_profile_source,
   kwin_output_device_ignore_brightness,
   kwin_output_device_ignore_color_power_tradeoff,
   kwin_output_device_ignore_dimming,
   kwin_output_device_ignore_replication_source,
   kwin_output_device_ignore_ddc_ci_allowed,
   kwin_output_device_ignore_max_bits_per_color,
   kwin_output_device_ignore_max_bits_per_color_range,
   kwin_output_device_ignore_automatic_max_bits_per_color_limit,
   kwin_output_device_ignore_edr_policy,
   kwin_output_device_ignore_sharpness,
   kwin_output_device_ignore_priority,
   kwin_output_device_ignore_auto_brightness,
   kwin_output_device_removed,
   kwin_output_device_ignore_hdr_icc_profile_path,
   kwin_output_device_ignore_hdr_color_profile_source,
   kwin_output_device_ignore_abm_level
};

static void kwin_output_device_mode_ignore_flags(void *data, struct kde_output_device_mode_v2 *obj, uint32_t flags)
{ (void)data; (void)obj; (void)flags; }

static void kwin_output_device_mode_ignore_cvt(void *data, struct kde_output_device_mode_v2 *obj, uint32_t dot_clock, uint32_t hdisplay, uint32_t hsync_start, uint32_t hsync_end, uint32_t htotal, uint32_t hskew, uint32_t vdisplay, uint32_t vsync_start, uint32_t vsync_end, uint32_t vtotal, uint32_t vscan, uint32_t flags)
{ (void)data; (void)obj; (void)dot_clock; (void)hdisplay; (void)hsync_start; (void)hsync_end; (void)htotal; (void)hskew; (void)vdisplay; (void)vsync_start; (void)vsync_end; (void)vtotal; (void)vscan; (void)flags; }

static const struct kde_output_device_mode_v2_listener kwin_output_device_mode_listener_real = {
   kwin_output_device_mode_size,
   kwin_output_device_mode_refresh,
   kwin_output_device_mode_preferred,
   kwin_output_device_mode_removed,
   kwin_output_device_mode_ignore_flags,
   kwin_output_device_mode_ignore_cvt
};

static const struct kde_output_configuration_v2_listener kwin_output_configuration_listener = {
   kwin_output_configuration_applied,
   kwin_output_configuration_failed,
   kwin_output_configuration_failure_reason
};


static void kwin_device_add(kwin_outputs_t *kw,
      struct kde_output_device_v2 *device)
{
   kwin_output_device_t *dev = NULL;
   unsigned i;
   for (i = 0; i < kw->ndevices && !dev; i++)
      if (!kw->devices[i].device)
         dev = &kw->devices[i];
   if (!dev && kw->ndevices < KWIN_OUTPUT_MAX_DEVICES)
      dev = &kw->devices[kw->ndevices++];
   if (!dev)
   {
      kde_output_device_v2_destroy(device);
      return;
   }
   memset(dev, 0, sizeof(*dev));
   dev->device = device;
   kde_output_device_v2_add_listener(device, &kwin_output_device_listener, dev);
}

static void kwin_output_device_registry_output(void *data,
      struct kde_output_device_registry_v2 *obj,
      struct kde_output_device_v2 *device)
{
   (void)obj;
   kwin_device_add((kwin_outputs_t*)data, device);
}

bool kwin_outputs_bind(kwin_outputs_t *kw, struct wl_registry *registry,
      uint32_t name, const char *interface, uint32_t version)
{
   if (!strcmp(interface, kde_output_device_registry_v2_interface.name))
   {
      if (!kw->registry)
      {
         kw->registry = (struct kde_output_device_registry_v2*)
            wl_registry_bind(registry, name,
                  &kde_output_device_registry_v2_interface,
                  version < KWIN_DEVICE_VERSION ? version : KWIN_DEVICE_VERSION);
         if (kw->registry)
            kde_output_device_registry_v2_add_listener(kw->registry,
                  &kwin_output_device_registry_listener, kw);
      }
      return true;
   }
   /* Before the registry (version 21) each output was a global */
   if (!strcmp(interface, kde_output_device_v2_interface.name))
   {
      struct kde_output_device_v2 *device = (struct kde_output_device_v2*)
         wl_registry_bind(registry, name, &kde_output_device_v2_interface,
               version < KWIN_DEVICE_VERSION ? version : KWIN_DEVICE_VERSION);
      if (device)
         kwin_device_add(kw, device);
      return true;
   }
   if (!strcmp(interface, kde_output_management_v2_interface.name))
   {
      if (!kw->management)
         kw->management = (struct kde_output_management_v2*)
            wl_registry_bind(registry, name,
                  &kde_output_management_v2_interface,
                  version < KWIN_MANAGEMENT_VERSION
                  ? version : KWIN_MANAGEMENT_VERSION);
      return true;
   }
   return false;
}

static kwin_output_device_t *kwin_pick(kwin_outputs_t *kw,
      const char *connector)
{
   unsigned i;
   kwin_output_device_t *first = NULL;
   for (i = 0; i < kw->ndevices; i++)
   {
      kwin_output_device_t *dev = &kw->devices[i];
      if (     !dev->device
            || !(dev->flags & KWIN_DEVICE_DONE)
            || !dev->nmodes)
         continue;
      if (connector && *connector && !strcmp(dev->name, connector))
         return dev;
      if (!first && (dev->flags & KWIN_DEVICE_ENABLED))
         first = dev;
   }
   return first;
}

bool kwin_outputs_ready(const kwin_outputs_t *kw)
{
   return kw && kw->management
      && kwin_pick((kwin_outputs_t*)kw, NULL) != NULL;
}

static int kwin_mode_cmp(const void *a, const void *b)
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

video_display_config_t *kwin_outputs_resolution_list(kwin_outputs_t *kw,
      const char *connector, unsigned *len)
{
   unsigned i, n = 0;
   video_display_config_t *list = NULL;
   kwin_output_device_t *dev    = NULL;

   *len = 0;
   if (!kw || !kw->management || !(dev = kwin_pick(kw, connector)))
      return NULL;
   if (!(list = (video_display_config_t*)calloc(dev->nmodes,
               sizeof(*list))))
      return NULL;
   for (i = 0; i < dev->nmodes; i++)
   {
      const kwin_output_mode_t *m = &dev->modes[i];
      if (m->width <= 0 || m->height <= 0 || m->refresh_mhz <= 0)
         continue;
      list[n].dims              = VIDEO_SCALE_PACK(m->width, m->height);
      list[n].bpp               = 32;
      list[n].refreshrate_float = (float)m->refresh_mhz / 1000.0f;
      list[n].refreshrate       = (unsigned)((m->refresh_mhz + 500) / 1000);
      list[n].current           = (m->mode == dev->current);
      n++;
   }
   qsort(list, n, sizeof(*list), kwin_mode_cmp);
   for (i = 0; i < n; i++)
      list[i].idx = i;
   *len = n;
   return list;
}

bool kwin_outputs_set_mode(kwin_outputs_t *kw, const char *connector,
      unsigned width, unsigned height, int int_hz, float hz)
{
   unsigned i;
   float want_hz;
   struct kde_output_configuration_v2 *config;
   const kwin_output_mode_t *cur  = NULL;
   const kwin_output_mode_t *best = NULL;
   kwin_output_device_t *dev      = NULL;

   if (!kw || !kw->management || !(dev = kwin_pick(kw, connector)))
      return false;
   for (i = 0; i < dev->nmodes; i++)
      if (dev->modes[i].mode == dev->current)
         cur = &dev->modes[i];
   if (!width)
      width  = cur ? (unsigned)cur->width  : 0;
   if (!height)
      height = cur ? (unsigned)cur->height : 0;
   want_hz = hz > 0.0f ? hz : (float)int_hz;
   if (want_hz <= 0.0f && cur)
      want_hz = (float)cur->refresh_mhz / 1000.0f;

   /* The listed mode of that size nearest the rate */
   for (i = 0; i < dev->nmodes; i++)
   {
      const kwin_output_mode_t *m = &dev->modes[i];
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
      RARCH_WARN("[Wayland] KWin lists no %ux%u mode on %s.\n",
            width, height, dev->name[0] ? dev->name : "its output");
      return false;
   }
   if (best->mode == dev->current)
      return true;

   if (!(config = kde_output_management_v2_create_configuration(
               kw->management)))
      return false;
   kde_output_configuration_v2_add_listener(config,
         &kwin_output_configuration_listener, NULL);
   kde_output_configuration_v2_mode(config, dev->device, best->mode);
   kde_output_configuration_v2_apply(config);
   RARCH_LOG("[Wayland] Asked KWin for %dx%d at %.3f Hz on %s.\n",
         best->width, best->height, (float)best->refresh_mhz / 1000.0f,
         dev->name[0] ? dev->name : "its output");
   return true;
}

void kwin_outputs_destroy(kwin_outputs_t *kw)
{
   unsigned i;
   if (!kw)
      return;
   for (i = 0; i < kw->ndevices; i++)
      if (kw->devices[i].device)
         kwin_device_free(&kw->devices[i]);
   if (kw->registry)
      kde_output_device_registry_v2_destroy(kw->registry);
   if (kw->management)
      kde_output_management_v2_destroy(kw->management);
   memset(kw, 0, sizeof(*kw));
}
