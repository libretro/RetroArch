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
#include <unistd.h>

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
   else if (feature == WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC)
      color->flags |= WL_COLOR_FEATURE_PARAMETRIC;
   else if (feature == WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES)
      color->flags |= WL_COLOR_FEATURE_LUMINANCES;
}

static void wl_color_supported_tf_named(void *data,
      struct wp_color_manager_v1 *manager, uint32_t tf)
{
   (void)manager;
   if (tf == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR)
      ((wl_color_t*)data)->flags |= WL_COLOR_TF_EXT_LINEAR;
}

static void wl_color_supported_primaries_named(void *data,
      struct wp_color_manager_v1 *manager, uint32_t primaries)
{
   (void)manager;
   if (primaries == WP_COLOR_MANAGER_V1_PRIMARIES_SRGB)
      ((wl_color_t*)data)->flags |= WL_COLOR_PRIMARIES_SRGB;
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
   if (color->flags & WL_COLOR_TAGGED_PARAMETRIC)
      RARCH_LOG("[Wayland] Surface tagged with the frame's own luminances for HDR output.\n");
   else
      RARCH_LOG("[Wayland] Surface tagged as scRGB for HDR output.\n");
}

static const struct wp_image_description_v1_listener
   wl_color_description_listener =
{
   wl_color_description_failed,
   wl_color_description_ready
};

/* The output's description, read for the display's peak luminance.
 * target_luminance describes the display; the plain luminances event
 * describes the transfer function's range - 10,000 nits for PQ - and
 * says nothing about the panel. */
static void wl_color_info_done(void *data,
      struct wp_image_description_info_v1 *info)
{
   wl_color_t *color = (wl_color_t*)data;
   float peak        = color->info_target_max > 0.0f
      ? color->info_target_max : color->info_target_cll;
   /* A destructor event: the object is the client's to free now */
   wp_image_description_info_v1_destroy(info);
   if (peak <= 0.0f)
      return;
   color->output_peak_nits = peak;
   color->flags           |= WL_COLOR_OUTPUT_PEAK;
   RARCH_LOG("[Wayland] Display peak luminance: %.0f nits (from the compositor).\n",
         peak);
   if (color->peak_cb)
      color->peak_cb(peak);
}

static void wl_color_info_icc_file(void *data,
      struct wp_image_description_info_v1 *info, int32_t icc,
      uint32_t icc_size)
{
   (void)data; (void)info; (void)icc_size;
   /* A descriptor the client owns once it arrives */
   if (icc >= 0)
      close(icc);
}

static void wl_color_info_primaries(void *data,
      struct wp_image_description_info_v1 *info, int32_t r_x, int32_t r_y,
      int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y, int32_t w_x,
      int32_t w_y)
{
   (void)data; (void)info; (void)r_x; (void)r_y; (void)g_x; (void)g_y;
   (void)b_x; (void)b_y; (void)w_x; (void)w_y;
}

static void wl_color_info_uint(void *data,
      struct wp_image_description_info_v1 *info, uint32_t v)
{
   (void)data; (void)info; (void)v;
}

static void wl_color_info_luminances(void *data,
      struct wp_image_description_info_v1 *info, uint32_t min_lum,
      uint32_t max_lum, uint32_t reference_lum)
{
   (void)data; (void)info; (void)min_lum; (void)max_lum; (void)reference_lum;
}

static void wl_color_info_target_luminance(void *data,
      struct wp_image_description_info_v1 *info, uint32_t min_lum,
      uint32_t max_lum)
{
   (void)info; (void)min_lum;
   ((wl_color_t*)data)->info_target_max = (float)max_lum;
}

static void wl_color_info_target_max_cll(void *data,
      struct wp_image_description_info_v1 *info, uint32_t max_cll)
{
   (void)info;
   ((wl_color_t*)data)->info_target_cll = (float)max_cll;
}

static const struct wp_image_description_info_v1_listener
   wl_color_info_listener =
{
   wl_color_info_done,
   wl_color_info_icc_file,
   wl_color_info_primaries,
   wl_color_info_uint,
   wl_color_info_uint,
   wl_color_info_uint,
   wl_color_info_luminances,
   wl_color_info_primaries,
   wl_color_info_target_luminance,
   wl_color_info_target_max_cll,
   wl_color_info_uint
};

static void wl_color_output_desc_failed(void *data,
      struct wp_image_description_v1 *desc, uint32_t cause,
      const char *msg)
{
   (void)data; (void)desc; (void)cause; (void)msg;
}

/* Ready: only now may its information be asked for */
static void wl_color_output_desc_ready(void *data,
      struct wp_image_description_v1 *desc, uint32_t identity)
{
   struct wp_image_description_info_v1 *info;
   wl_color_t *color = (wl_color_t*)data;
   (void)identity;
   color->info_target_max = 0.0f;
   color->info_target_cll = 0.0f;
   if ((info = wp_image_description_v1_get_information(desc)))
      wp_image_description_info_v1_add_listener(info,
            &wl_color_info_listener, data);
}

static const struct wp_image_description_v1_listener
   wl_color_output_desc_listener =
{
   wl_color_output_desc_failed,
   wl_color_output_desc_ready
};

bool wl_color_query_output(wl_color_t *color, struct wl_output *output)
{
   if (!color || !color->manager || !output || color->output)
      return false;
   if (!(color->output = wp_color_manager_v1_get_output(color->manager,
               output)))
      return false;
   if (!(color->output_desc =
            wp_color_management_output_v1_get_image_description(
               color->output)))
      return false;
   wp_image_description_v1_add_listener(color->output_desc,
         &wl_color_output_desc_listener, color);
   return true;
}

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

bool wl_color_parametric_supported(const wl_color_t *color)
{
   const uint32_t need = WL_COLOR_DONE | WL_COLOR_FEATURE_PARAMETRIC
      | WL_COLOR_FEATURE_LUMINANCES | WL_COLOR_INTENT_PERCEPTUAL;
   return color && color->manager && (color->flags & need) == need
      && (color->flags & WL_COLOR_TF_EXT_LINEAR)
      && (color->flags & WL_COLOR_PRIMARIES_SRGB);
}

bool wl_color_attach_luminances(wl_color_t *color,
      struct wl_surface *surface, float paper_white_nits, float peak_nits)
{
   struct wp_image_description_creator_params_v1 *params;
   uint32_t reference, peak;

   if (     !surface || color->surface
         || !wl_color_parametric_supported(color))
      return false;
   if (paper_white_nits <= 0.0f || peak_nits <= 0.0f)
      return false;
   reference = (uint32_t)(paper_white_nits + 0.5f);
   peak      = (uint32_t)(peak_nits + 0.5f);
   /* The reference white is inside the range the frame can reach */
   if (peak < reference)
      peak = reference;

   if (!(color->surface = wp_color_manager_v1_get_surface(color->manager,
               surface)))
      return false;
   if (!(params = wp_color_manager_v1_create_parametric_creator(
               color->manager)))
      return false;

   /* The same frame Windows-scRGB describes - extended-linear sRGB at
    * BT.709 primaries, 1.0 being the reference white - said with the
    * luminances it actually carries, so the compositor maps it to the
    * display from what is in it rather than from an assumption. */
   wp_image_description_creator_params_v1_set_tf_named(params,
         WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR);
   wp_image_description_creator_params_v1_set_primaries_named(params,
         WP_COLOR_MANAGER_V1_PRIMARIES_SRGB);
   /* min: the darkest the frame asks for; 0.0001 cd/m² units */
   wp_image_description_creator_params_v1_set_luminances(params,
         50, peak, reference);
   if (!(color->scrgb = wp_image_description_creator_params_v1_create(
               params)))
      return false;
   color->flags |= WL_COLOR_TAGGED_PARAMETRIC;
   wp_image_description_v1_add_listener(color->scrgb,
         &wl_color_description_listener, color);
   return true;
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
   if (color->output_desc)
      wp_image_description_v1_destroy(color->output_desc);
   if (color->output)
      wp_color_management_output_v1_destroy(color->output);
   if (color->scrgb)
      wp_image_description_v1_destroy(color->scrgb);
   if (color->surface)
      wp_color_management_surface_v1_destroy(color->surface);
   if (color->manager)
      wp_color_manager_v1_destroy(color->manager);
   memset(color, 0, sizeof(*color));
}
