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

#ifndef __RARCH_WAYLAND_COLOR_H
#define __RARCH_WAYLAND_COLOR_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <wayland-client.h>

RETRO_BEGIN_DECLS

/* The compositor's colour management (wp_color_manager_v1) as a surface
 * with an HDR frame needs it: whether the compositor takes
 * Windows-scRGB, and tagging a surface with it. Nothing here waits: the
 * compositor's capabilities arrive with the globals, and a surface is
 * tagged from the image description's ready event, which the context
 * dispatches with the rest of its queue; the tag is double-buffered
 * surface state and lands with the frame that follows. */

#define WL_COLOR_INTENT_PERCEPTUAL   (1u << 0)
#define WL_COLOR_FEATURE_SCRGB       (1u << 1)
#define WL_COLOR_DONE                (1u << 2)
#define WL_COLOR_TAGGED              (1u << 3)
#define WL_COLOR_FAILED              (1u << 4)
/* Set by the GL context: its framebuffer is FP16 for scRGB */
#define WL_COLOR_FP16                (1u << 5)
/* The output's peak luminance has been read */
#define WL_COLOR_OUTPUT_PEAK         (1u << 6)
/* The compositor takes a parametric description with luminances */
#define WL_COLOR_FEATURE_PARAMETRIC  (1u << 7)
#define WL_COLOR_FEATURE_LUMINANCES  (1u << 8)
/* The surface was tagged with the frame's own luminances, not scRGB */
#define WL_COLOR_TAGGED_PARAMETRIC   (1u << 9)
/* The named transfer function and primaries the parametric path needs */
#define WL_COLOR_TF_EXT_LINEAR       (1u << 10)
#define WL_COLOR_PRIMARIES_SRGB      (1u << 11)

struct wp_color_manager_v1;
struct wp_color_management_surface_v1;
struct wp_color_management_output_v1;
struct wp_image_description_v1;

typedef struct wl_color
{
   struct wp_color_manager_v1            *manager;
   struct wp_color_management_surface_v1 *surface;
   struct wp_image_description_v1        *scrgb;
   struct wp_color_management_output_v1  *output;
   struct wp_image_description_v1        *output_desc;
   /* Told the output's peak once it is read; may be NULL */
   void                                 (*peak_cb)(float nits);
   /* The display's peak luminance as the compositor describes the
    * output (its target luminance), in nits; 0 while unknown */
   float                                  output_peak_nits;
   /* What the output's description said, while it is being read */
   float                                  info_target_max;
   float                                  info_target_cll;
   uint32_t                               flags;
} wl_color_t;

/* From the registry listener: binds the colour manager and starts
 * listening for what it supports. */
void wl_color_bind(wl_color_t *color, struct wl_registry *registry,
      uint32_t name, uint32_t version);

/* The registry interface name to match against. */
const char *wl_color_interface_name(void);

/* Whether the compositor has said it takes Windows-scRGB with a
 * perceptual rendering intent. */
bool wl_color_scrgb_supported(const wl_color_t *color);

/* Asks for 'surface' to be treated as Windows-scRGB. Returns false when
 * the compositor cannot, or the surface already has a colour-management
 * object; otherwise the tag is applied when the description is ready. */
bool wl_color_attach_scrgb(wl_color_t *color, struct wl_surface *surface);

/* Whether the compositor takes a parametric description carrying the
 * frame's luminances, which describes the same extended-linear sRGB
 * frame as Windows-scRGB but says how bright it is. */
bool wl_color_parametric_supported(const wl_color_t *color);

/* Asks for 'surface' to be treated as that frame: extended-linear sRGB
 * at BT.709 primaries, reference white at 'paper_white_nits' and a
 * peak of 'peak_nits'. Same terms as wl_color_attach_scrgb, whose
 * place it takes. */
bool wl_color_attach_luminances(wl_color_t *color,
      struct wl_surface *surface, float paper_white_nits, float peak_nits);

/* Asks the compositor how it describes 'output', for the display's
 * peak luminance; peak_cb, if set, is told when it is known. Returns
 * false where the compositor has no colour management. */
bool wl_color_query_output(wl_color_t *color, struct wl_output *output);

/* Destroys every colour-management object; call before the surface. */
void wl_color_destroy(wl_color_t *color);

RETRO_END_DECLS

#endif
