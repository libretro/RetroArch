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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "spirv_shaders.h"

/* Each blob is here exactly when a driver in the build takes it: the
 * Vulkan driver, and the glcore driver with slang. */
#if defined(HAVE_VULKAN) || (defined(HAVE_OPENGL_CORE) && defined(HAVE_SLANG))
const uint32_t spirv_alpha_blend_vert[] =
#include "../drivers/vulkan_shaders/alpha_blend.vert.inc"
      ;
const size_t spirv_alpha_blend_vert_size = sizeof(spirv_alpha_blend_vert);

const uint32_t spirv_alpha_blend_frag[] =
#include "../drivers/vulkan_shaders/alpha_blend.frag.inc"
      ;
const size_t spirv_alpha_blend_frag_size = sizeof(spirv_alpha_blend_frag);

const uint32_t spirv_alpha_blend_linear_frag[] =
#include "../drivers/vulkan_shaders/alpha_blend_linear.frag.inc"
      ;
const size_t spirv_alpha_blend_linear_frag_size = sizeof(spirv_alpha_blend_linear_frag);

const uint32_t spirv_mesh_vert[] =
#include "../drivers/vulkan_shaders/mesh.vert.inc"
      ;
const size_t spirv_mesh_vert_size = sizeof(spirv_mesh_vert);

const uint32_t spirv_font_frag[] =
#include "../drivers/vulkan_shaders/font.frag.inc"
      ;
const size_t spirv_font_frag_size = sizeof(spirv_font_frag);

const uint32_t spirv_stereo_anaglyph_frag[] =
#include "../drivers/vulkan_shaders/stereo_anaglyph.frag.inc"
      ;
const size_t spirv_stereo_anaglyph_frag_size = sizeof(spirv_stereo_anaglyph_frag);

const uint32_t spirv_stereo_interlaced_frag[] =
#include "../drivers/vulkan_shaders/stereo_interlaced.frag.inc"
      ;
const size_t spirv_stereo_interlaced_frag_size = sizeof(spirv_stereo_interlaced_frag);

const uint32_t spirv_stock_rect_frag[] =
#include "../drivers/vulkan_shaders/stock_rect.frag.inc"
      ;
const size_t spirv_stock_rect_frag_size = sizeof(spirv_stock_rect_frag);

#ifdef HAVE_SHADERPIPELINE
const uint32_t spirv_pipeline_ribbon_vert[] =
#include "../drivers/vulkan_shaders/pipeline_ribbon.vert.inc"
      ;
const size_t spirv_pipeline_ribbon_vert_size = sizeof(spirv_pipeline_ribbon_vert);

const uint32_t spirv_pipeline_ribbon_frag[] =
#include "../drivers/vulkan_shaders/pipeline_ribbon.frag.inc"
      ;
const size_t spirv_pipeline_ribbon_frag_size = sizeof(spirv_pipeline_ribbon_frag);

const uint32_t spirv_pipeline_ribbon_simple_vert[] =
#include "../drivers/vulkan_shaders/pipeline_ribbon_simple.vert.inc"
      ;
const size_t spirv_pipeline_ribbon_simple_vert_size = sizeof(spirv_pipeline_ribbon_simple_vert);

const uint32_t spirv_pipeline_ribbon_simple_frag[] =
#include "../drivers/vulkan_shaders/pipeline_ribbon_simple.frag.inc"
      ;
const size_t spirv_pipeline_ribbon_simple_frag_size = sizeof(spirv_pipeline_ribbon_simple_frag);

const uint32_t spirv_pipeline_snow_simple_frag[] =
#include "../drivers/vulkan_shaders/pipeline_snow_simple.frag.inc"
      ;
const size_t spirv_pipeline_snow_simple_frag_size = sizeof(spirv_pipeline_snow_simple_frag);

const uint32_t spirv_pipeline_snow_frag[] =
#include "../drivers/vulkan_shaders/pipeline_snow.frag.inc"
      ;
const size_t spirv_pipeline_snow_frag_size = sizeof(spirv_pipeline_snow_frag);

const uint32_t spirv_pipeline_bokeh_frag[] =
#include "../drivers/vulkan_shaders/pipeline_bokeh.frag.inc"
      ;
const size_t spirv_pipeline_bokeh_frag_size = sizeof(spirv_pipeline_bokeh_frag);

const uint32_t spirv_pipeline_snowflake_frag[] =
#include "../drivers/vulkan_shaders/pipeline_snowflake.frag.inc"
      ;
const size_t spirv_pipeline_snowflake_frag_size = sizeof(spirv_pipeline_snowflake_frag);
#endif
#endif

#if defined(HAVE_SLANG) && (defined(HAVE_VULKAN) || defined(HAVE_OPENGL_CORE))
const uint32_t spirv_opaque_vert[] =
#include "../drivers/vulkan_shaders/opaque.vert.inc"
      ;
const size_t spirv_opaque_vert_size = sizeof(spirv_opaque_vert);

const uint32_t spirv_opaque_frag[] =
#include "../drivers/vulkan_shaders/opaque.frag.inc"
      ;
const size_t spirv_opaque_frag_size = sizeof(spirv_opaque_frag);
#endif

#ifdef HAVE_VULKAN
const uint32_t spirv_hdr_frag[] =
#include "../drivers/vulkan_shaders/hdr.frag.inc"
      ;
const size_t spirv_hdr_frag_size = sizeof(spirv_hdr_frag);
#endif

#if defined(HAVE_OPENGL_CORE) && defined(HAVE_SLANG)
const uint32_t spirv_hdr_scrgb_vert[] =
#include "../drivers/vulkan_shaders/hdr_scrgb.vert.inc"
      ;
const size_t spirv_hdr_scrgb_vert_size = sizeof(spirv_hdr_scrgb_vert);

const uint32_t spirv_hdr_scrgb_frag[] =
#include "../drivers/vulkan_shaders/hdr_scrgb.frag.inc"
      ;
const size_t spirv_hdr_scrgb_frag_size = sizeof(spirv_hdr_scrgb_frag);
#endif

#if defined(HAVE_OPENGL_CORE) && defined(HAVE_SLANG) && defined(HAVE_RSLC) && !defined(HAVE_SPIRV_CROSS)
static const struct
{
   const uint32_t *spirv;
   const char     *glsl;
} spirv_glsl[] = {
   { spirv_alpha_blend_vert,
#include "../drivers/vulkan_shaders/alpha_blend.vert.src.inc"
   },
   { spirv_alpha_blend_frag,
#include "../drivers/vulkan_shaders/alpha_blend.frag.src.inc"
   },
   { spirv_alpha_blend_linear_frag,
#include "../drivers/vulkan_shaders/alpha_blend_linear.frag.src.inc"
   },
   { spirv_mesh_vert,
#include "../drivers/vulkan_shaders/mesh.vert.src.inc"
   },
   { spirv_font_frag,
#include "../drivers/vulkan_shaders/font.frag.src.inc"
   },
   { spirv_stereo_anaglyph_frag,
#include "../drivers/vulkan_shaders/stereo_anaglyph.frag.src.inc"
   },
   { spirv_stereo_interlaced_frag,
#include "../drivers/vulkan_shaders/stereo_interlaced.frag.src.inc"
   },
   { spirv_stock_rect_frag,
#include "../drivers/vulkan_shaders/stock_rect.frag.src.inc"
   },
   { spirv_opaque_vert,
#include "../drivers/vulkan_shaders/opaque.vert.src.inc"
   },
   { spirv_opaque_frag,
#include "../drivers/vulkan_shaders/opaque.frag.src.inc"
   },
   { spirv_hdr_scrgb_vert,
#include "../drivers/vulkan_shaders/hdr_scrgb.vert.src.inc"
   },
   { spirv_hdr_scrgb_frag,
#include "../drivers/vulkan_shaders/hdr_scrgb.frag.src.inc"
   },
#ifdef HAVE_SHADERPIPELINE
   { spirv_pipeline_ribbon_vert,
#include "../drivers/vulkan_shaders/pipeline_ribbon.vert.src.inc"
   },
   { spirv_pipeline_ribbon_frag,
#include "../drivers/vulkan_shaders/pipeline_ribbon.frag.src.inc"
   },
   { spirv_pipeline_ribbon_simple_vert,
#include "../drivers/vulkan_shaders/pipeline_ribbon_simple.vert.src.inc"
   },
   { spirv_pipeline_ribbon_simple_frag,
#include "../drivers/vulkan_shaders/pipeline_ribbon_simple.frag.src.inc"
   },
   { spirv_pipeline_snow_simple_frag,
#include "../drivers/vulkan_shaders/pipeline_snow_simple.frag.src.inc"
   },
   { spirv_pipeline_snow_frag,
#include "../drivers/vulkan_shaders/pipeline_snow.frag.src.inc"
   },
   { spirv_pipeline_bokeh_frag,
#include "../drivers/vulkan_shaders/pipeline_bokeh.frag.src.inc"
   },
   { spirv_pipeline_snowflake_frag,
#include "../drivers/vulkan_shaders/pipeline_snowflake.frag.src.inc"
   },
#endif
   { NULL, NULL }
};

const char *spirv_shader_glsl(const uint32_t *spirv)
{
   unsigned i;
   for (i = 0; spirv_glsl[i].spirv; i++)
      if (spirv_glsl[i].spirv == spirv)
         return spirv_glsl[i].glsl;
   return NULL;
}
#endif
