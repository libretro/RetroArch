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

#include <string.h>

#include "wayland_color.h"
#include "wayland/color-management-v1.h"

#include "../../verbosity.h"

static void wl_color_supported_intent(void *data,
      struct wp_color_manager_v1 *manager, uint32_t intent)
{
   wl_color_t *color = (wl_color_t*)data;
   (void)manager;
   if (intent == WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL)
      color->flags |= WL_COLOR_INTENT_PERCEPTUAL;
}

static void wl_color_supported_feature(void *data,
      struct wp_color_manager_v1 *manager, uint32_t feature)
{
   wl_color_t *color = (wl_color_t*)data;
   (void)manager;
   if (feature == WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_SCRGB)
      color->flags |= WL_COLOR_FEATURE_SCRGB;
}

static void wl_color_supported_tf_named(void *data,
      struct wp_color_manager_v1 *manager, uint32_t tf)
{
   (void)data; (void)manager; (void)tf;
}

static void wl_color_supported_primaries_named(void *data,
      struct wp_color_manager_v1 *manager, uint32_t primaries)
{
   (void)data; (void)manager; (void)primaries;
}

static void wl_color_done(void *data, struct wp_color_manager_v1 *manager)
{
   wl_color_t *color = (wl_color_t*)data;
   (void)manager;
   color->flags |= WL_COLOR_DONE;
}

static const struct wp_color_manager_v1_listener wl_color_manager_listener =
{
   wl_color_supported_intent,
   wl_color_supported_feature,
   wl_color_supported_tf_named,
   wl_color_supported_primaries_named,
   wl_color_done
};

static void wl_color_description_failed(void *data,
      struct wp_image_description_v1 *desc, uint32_t cause,
      const char *msg)
{
   wl_color_t *color = (wl_color_t*)data;
   (void)desc;
   (void)cause;
   color->flags |= WL_COLOR_FAILED;
   RARCH_WARN("[Wayland] The compositor refused the scRGB image description: %s.\n",
         msg ? msg : "no reason given");
}

/* Ready: only now may the description be set on the surface. */
static void wl_color_description_ready(void *data,
      struct wp_image_description_v1 *desc, uint32_t identity)
{
   wl_color_t *color = (wl_color_t*)data;
   (void)identity;
   if (!color->surface)
      return;
   wp_color_management_surface_v1_set_image_description(color->surface,
         desc, WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL);
   color->flags |= WL_COLOR_TAGGED;
   RARCH_LOG("[Wayland] Surface tagged as scRGB for HDR output.\n");
}

static const struct wp_image_description_v1_listener
   wl_color_description_listener =
{
   wl_color_description_failed,
   wl_color_description_ready
};

const char *wl_color_interface_name(void)
{
   return wp_color_manager_v1_interface.name;
}

void wl_color_bind(wl_color_t *color, struct wl_registry *registry,
      uint32_t name, uint32_t version)
{
   if (color->manager)
      return;
   color->manager = (struct wp_color_manager_v1*)wl_registry_bind(
         registry, name, &wp_color_manager_v1_interface,
         version < 1 ? version : 1);
   if (color->manager)
      wp_color_manager_v1_add_listener(color->manager,
            &wl_color_manager_listener, color);
}

bool wl_color_scrgb_supported(const wl_color_t *color)
{
   const uint32_t need = WL_COLOR_DONE | WL_COLOR_FEATURE_SCRGB
      | WL_COLOR_INTENT_PERCEPTUAL;
   return color && color->manager && (color->flags & need) == need;
}

bool wl_color_attach_scrgb(wl_color_t *color, struct wl_surface *surface)
{
   if (!surface || !wl_color_scrgb_supported(color) || color->surface)
      return false;
   if (!(color->surface = wp_color_manager_v1_get_surface(color->manager,
               surface)))
      return false;
   if (!(color->scrgb = wp_color_manager_v1_create_windows_scrgb(
               color->manager)))
      return false;
   wp_image_description_v1_add_listener(color->scrgb,
         &wl_color_description_listener, color);
   return true;
}

void wl_color_destroy(wl_color_t *color)
{
   if (color->scrgb)
      wp_image_description_v1_destroy(color->scrgb);
   if (color->surface)
      wp_color_management_surface_v1_destroy(color->surface);
   if (color->manager)
      wp_color_manager_v1_destroy(color->manager);
   memset(color, 0, sizeof(*color));
}
