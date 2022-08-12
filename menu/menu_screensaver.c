/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2019-2021 - James Leaver
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
#include <boolean.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <retro_inline.h>

#include "../verbosity.h"

#if defined(HAVE_CONFIG_H)
#include "../config.h"
#endif

#include "menu_screensaver.h"

#ifndef PI
#define PI 3.14159265359f
#endif

/* Font path defines */
#define MENU_SS_PKG_DIR "pkg"
#define MENU_SS_FONT_FILE "osd-font.ttf"

/* Determines whether current platform has
 * Unicode character support */
#if defined(HAVE_FREETYPE) || (defined(__APPLE__) && defined(HAVE_CORETEXT)) || (defined(HAVE_STB_FONT) && (defined(VITA) || defined(WIIU) || defined(ANDROID) || (defined(_WIN32) && !defined(_XBOX) && !defined(_MSC_VER) && _MSC_VER >= 1400) || (defined(_WIN32) && !defined(_XBOX) && defined(_MSC_VER)) || defined(HAVE_LIBNX) || defined(__linux__) || defined (HAVE_EMSCRIPTEN) || defined(__APPLE__) || defined(HAVE_ODROIDGO2) || defined(__PS3__)))
#define MENU_SS_UNICODE_ENABLED true
#else
#define MENU_SS_UNICODE_ENABLED false
#endif

/* 256 on-screen particles provides a good
 * balance between effect density and draw
 * performance */
#define MENU_SS_NUM_PARTICLES 256

/* Particle effect animations update at a base rate
 * of 60Hz (-> 16.666 ms update period) */
#define MENU_SS_EFFECT_PERIOD ((1.0f / 60.0f) * 1000.0f)

/* To produce a sharp image, font glyphs must
 * be oversized and scaled down. Requested font
 * size is:
 *   (smallest screen dimension) * MENU_SS_FONT_SIZE_FACTOR */
#define MENU_SS_FONT_SIZE_FACTOR 0.1f

/* On a 240p display, base particle size should
 * be 4 pixels. To achieve this, we apply a global
 * scaling factor of:
 *   ((smallest screen dimension) * MENU_SS_PARTICLE_SIZE_FACTOR) / (font size) */
#define MENU_SS_PARTICLE_SIZE_FACTOR (4.0f / 240.0f)

/* Produces a colour value in RGBA32 format
 * from the specified 'tint' adjusted by the
 * specified 'luminosity'
 * > lum must not exceed 1.0f */
#define MENU_SS_PARTICLE_COLOR(tint_r, tint_g, tint_b, lum) (((uint32_t)(tint_r * lum) << 24) | ((uint32_t)(tint_g * lum) << 16) | ((uint32_t)(tint_b * lum) << 8) | 0xFF)

/* Definition of screensaver 'particle':
 * - symbol:          string representation of a font glyph
 * - x:               centre x-coordinate of draw position
 * - y:               centre y-coordinate of draw position
 * - size:            glyph scale factor when drawn
 *                    (1.0 == default size)
 * - a,b,c,d:         general purpose effect-specific variables
 *                    (e.g. velocity, radius, theta, ...)
 * - color:           particle colour in RGBA32 format */
typedef struct
{
   uint32_t color;
   float x;
   float y;
   float size;
   float a;
   float b;
   float c;
   float d;
   const char *symbol;
} menu_ss_particle_t;

/* Holds all objects + metadata corresponding
 * to a font */
typedef struct
{
   font_data_t *font;
   video_font_raster_block_t raster_block; /* ptr alignment */
   float y_centre_offset;
} menu_ss_font_data_t;

/* Holds values used to colourise a particle */
typedef struct
{
   uint32_t color;
   uint32_t r;
   uint32_t g;
   uint32_t b;
} menu_ss_particle_tint_t;

struct menu_ss_handle
{
   float bg_color[16];
   menu_ss_font_data_t font_data;
   unsigned last_width;
   unsigned last_height;
   float font_size;
   float particle_scale;
   menu_ss_particle_tint_t particle_tint;
   menu_ss_particle_t *particles;
   enum menu_screensaver_effect effect;
   bool font_enabled;
   bool unicode_enabled;
};

/* UTF-8 Symbols */
static const char * const menu_ss_fallback_symbol = "*";

#define MENU_SS_NUM_SNOW_SYMBOLS 3
static const char * const menu_ss_snow_symbols[] = {
   "\xE2\x9D\x84", /* Snowflake, U+2744 */
   "\xE2\x9D\x85", /* Tight Trifoliate Snowflake, U+2745 */
   "\xE2\x9D\x86"  /* Heavy Chevron Snowflake, U+2746 */
};

#define MENU_SS_NUM_STARFIELD_SYMBOLS 8
static const char * const menu_ss_starfield_symbols[] = {
   "\xE2\x98\x85", /* Black Star, U+2605 */
   "\xE2\x9C\xA6", /* Black Four Pointed Star, U+2726 */
   "\xE2\x9C\xB4", /* Eight Pointed Black Star, U+2734 */
   "\xE2\x9C\xB6", /* Six Pointed Black Star, U+2736 */
   "\xE2\x9C\xB7", /* Eight Pointed Rectilinear Black Star, U+2737 */
   "\xE2\x9C\xB8", /* Heavy Eight Pointed Rectilinear Black Star, U+2738 */
   "\xE2\x9C\xB9", /* Twelve Pointed Black Star, U+2739 */
   "\xE2\x97\x8F"  /* Black Circle, U+25CF */
};

#define MENU_SS_NUM_VORTEX_SYMBOLS 6
static const char * const menu_ss_vortex_symbols[] = {
   "\xE2\x9D\x8B", /* Heavy Eight Teardrop-Spoked Propeller Asterisk, U+274B */
   "\xE2\x9C\xBD", /* Heavy Teardrop-Spoked Asterisk, U+273D */
   "\xE2\x9C\xBB", /* Teardrop-Spoked Asterisk, U+273B */
   "\xE2\x97\x89", /* Fisheye, U+25C9 */
   "\xE2\x97\x8F", /* Black Circle, U+25CF */
   "\xE2\x97\x86"  /* Black Diamond, U+25C6 */
};

/******************/
/* Initialisation */
/******************/

/* Creates and initialises a new screensaver object.
 * Returned object must be freed using
 * menu_screensaver_free().
 * Returns NULL in the event of an error. */
menu_screensaver_t *menu_screensaver_init(void)
{
   menu_screensaver_t *screensaver = NULL;
   /* Screensaver background must be pure black,
    * 100% opacity */
   float bg_color[16]              = COLOR_HEX_TO_FLOAT(0x000000, 1.0f);

   /* Create menu_screensaver_t object */
   screensaver = (menu_screensaver_t*)malloc(sizeof(*screensaver));

   if (!screensaver)
      return NULL;

   /* Initial effect is always 'blank' */
   screensaver->effect = MENU_SCREENSAVER_BLANK;

   /* > Fonts must be enabled for the screensaver to
    *   function. 'font_enabled' flag exists purely
    *   to prevent re-initialisation spam in the event
    *   that font creation fails
    * > Initialise 'Unicode enabled' state based on
    *   compiler flags */
   screensaver->font_enabled    = true;
   screensaver->unicode_enabled = MENU_SS_UNICODE_ENABLED;

   /* Set background colour */
   memcpy(screensaver->bg_color, bg_color, sizeof(screensaver->bg_color));

   /* Font is loaded on-demand
    * (Don't waste memory unless we are actually
    *  drawing an effect) */
   memset(&screensaver->font_data, 0, sizeof(screensaver->font_data));

   /* Particle array is created on-demand
    * (Don't waste memory unless we are actually
    *  drawing an effect) */
   screensaver->particles           = NULL;
   screensaver->particle_tint.color = 0;
   screensaver->particle_tint.r     = 0;
   screensaver->particle_tint.g     = 0;
   screensaver->particle_tint.b     = 0;

   /* Initial dimensions are zeroed out - will be set
    * on first call of menu_screensaver_iterate() */
   screensaver->last_width     = 0;
   screensaver->last_height    = 0;
   screensaver->font_size      = 0.0f;
   screensaver->particle_scale = 0.0f;

   return screensaver;
}

/* Frees specified screensaver object */
void menu_screensaver_free(menu_screensaver_t *screensaver)
{
   if (!screensaver)
      return;

   /* Free font */
   if (screensaver->font_data.font)
   {
      gfx_display_font_free(screensaver->font_data.font);
      video_coord_array_free(&screensaver->font_data.raster_block.carr);
      screensaver->font_data.font = NULL;

      font_driver_bind_block(NULL, NULL);
   }

   /* Free particle array */
   if (screensaver->particles)
      free(screensaver->particles);
   screensaver->particles = NULL;

   free(screensaver);
}

/*********************/
/* Context functions */
/*********************/

/* Called when the graphics context is destroyed
 * or reset (a dedicated 'reset' function is
 * unnecessary) */
void menu_screensaver_context_destroy(menu_screensaver_t *screensaver)
{
   if (!screensaver)
      return;

   /* Free any existing font
    * (will be recreated, if required, on the next
    * call of menu_screensaver_iterate()) */
   if (screensaver->font_data.font)
   {
      gfx_display_font_free(screensaver->font_data.font);
      video_coord_array_free(&screensaver->font_data.raster_block.carr);
      screensaver->font_data.font = NULL;
   }
}

/**********************/
/* Run loop functions */
/**********************/

/* qsort() helpers */
static int menu_ss_starfield_qsort_func(const menu_ss_particle_t *a,
      const menu_ss_particle_t *b)
{
   return a->c > b->c ? -1 : 1;
}

static int menu_ss_vortex_qsort_func(const menu_ss_particle_t *a,
      const menu_ss_particle_t *b)
{
   return a->a < b->a ? -1 : 1;
}

/* Calculates base font size and particle
 * scale based on current screen dimensions */
static INLINE void menu_screensaver_set_dimensions(
      menu_screensaver_t *screensaver,
      unsigned width, unsigned height)
{
   float screen_size           = (float)((width < height) ? width : height);
   screensaver->font_size      = (screen_size * MENU_SS_FONT_SIZE_FACTOR) + 0.5f;
   screensaver->particle_scale = (screen_size * MENU_SS_PARTICLE_SIZE_FACTOR) / screensaver->font_size;
   screensaver->last_width     = width;
   screensaver->last_height    = height;
}

static bool menu_screensaver_init_effect(menu_screensaver_t *screensaver)
{
   unsigned width;
   unsigned height;
   size_t i;

   /* Create particle array, if required */
   if (!screensaver->particles)
   {
      screensaver->particles = (menu_ss_particle_t*)
            calloc(MENU_SS_NUM_PARTICLES, sizeof(*screensaver->particles));

      if (!screensaver->particles)
         return false;
   }
   
   width  = screensaver->last_width;
   height = screensaver->last_height;

   /* Initialise array */
   switch (screensaver->effect)
   {
      case MENU_SCREENSAVER_SNOW:
         {
            for (i = 0; i < MENU_SS_NUM_PARTICLES; i++)
            {
               menu_ss_particle_t *particle = &screensaver->particles[i];
               float size_factor;

               particle->x    = (float)(rand() % width);
               particle->y    = (float)(rand() % height);
               particle->a    = (float)(rand() % 64 - 16) * 0.1f;
               particle->b    = (float)(rand() % 64 - 48) * 0.1f;

               /* Get particle size */
               size_factor    = (float)i / (float)MENU_SS_NUM_PARTICLES;
               size_factor    = size_factor * size_factor;
               particle->size = 1.0f + (size_factor * 2.0f);

               /* If Unicode is supported, select a random
                * snowflake symbol */
               particle->symbol    = menu_ss_fallback_symbol;
               if (screensaver->unicode_enabled)
                  particle->symbol = menu_ss_snow_symbols[(unsigned)(rand() % MENU_SS_NUM_SNOW_SYMBOLS)];
            }
         }
         break;
      case MENU_SCREENSAVER_STARFIELD:
         {
            float max_depth            = (float)(width > height ? width : height);
            float initial_speed_factor = 0.02f * max_depth / 240.0f;

            for (i = 0; i < MENU_SS_NUM_PARTICLES; i++)
            {
               menu_ss_particle_t *particle = &screensaver->particles[i];

               /* x pos ('physical' space) */
               particle->a = (float)(rand() % width);
               /* y pos ('physical' space) */
               particle->b = (float)(rand() % height);
               /* depth */
               particle->c = max_depth;
               /* speed */
               particle->d = 1.0f + ((float)(rand() % 20) * initial_speed_factor);

               /* If Unicode is supported, select a random
                * star symbol */
               particle->symbol    = menu_ss_fallback_symbol;
               if (screensaver->unicode_enabled)
                  particle->symbol = menu_ss_starfield_symbols[(unsigned)(rand() % MENU_SS_NUM_STARFIELD_SYMBOLS)];
            }
         }
         break;
      case MENU_SCREENSAVER_VORTEX:
         {
            float min_screen_dimension = (float)(width < height ? width : height);
            float max_radius           = (float)sqrt((double)((width * width) + (height * height))) / 2.0f;
            float radial_speed_factor  = 0.001f * min_screen_dimension / 240.0f;

            for (i = 0; i < MENU_SS_NUM_PARTICLES; i++)
            {
               menu_ss_particle_t *particle = &screensaver->particles[i];

               /* radius */
               particle->a = 1.0f + (((float)rand() / (float)RAND_MAX) * max_radius);
               /* theta */
               particle->b = ((float)rand() / (float)RAND_MAX) * 2.0f * PI;
               /* radial speed */
               particle->c = (float)((rand() % 100) + 1) * radial_speed_factor;
               /* rotational speed */
               particle->d = (((float)((rand() % 50) + 1) * 0.005f) + 0.1f) * (PI / 360.0f);

               /* If Unicode is supported, select a random
                * star symbol */
               particle->symbol    = menu_ss_fallback_symbol;
               if (screensaver->unicode_enabled)
                  particle->symbol = menu_ss_vortex_symbols[(unsigned)(rand() % MENU_SS_NUM_VORTEX_SYMBOLS)];
            }
         }
         break;
      default:
         /* Error condition - do nothing */
         return false;
   }

   return true;
}

/* Checks for and applies any pending screensaver
 * state changes */
static bool menu_screensaver_update_state(
      menu_screensaver_t *screensaver, gfx_display_t *p_disp,
      enum menu_screensaver_effect effect, uint32_t particle_tint,
      unsigned width, unsigned height, const char *dir_assets)
{
   bool init_effect = false;

#if defined(_3DS)
   /* 3DS has an 'incomplete' font driver,
    * and cannot render screensaver effects */
   effect = MENU_SCREENSAVER_BLANK;
#endif

   /* Check if dimensions have changed */
   if ((screensaver->last_width  != width) ||
       (screensaver->last_height != height))
   {
      menu_screensaver_set_dimensions(screensaver, width, height);

      /* Free any existing font */
      if (screensaver->font_data.font)
      {
         gfx_display_font_free(screensaver->font_data.font);
         video_coord_array_free(&screensaver->font_data.raster_block.carr);
         screensaver->font_data.font = NULL;
      }

      init_effect = true;
   }

   /* Check if effect has changed */
   if (screensaver->effect != effect)
   {
      screensaver->effect = effect;
      init_effect         = true;
   }

   /* Check if particle tint has changed */
   if (screensaver->particle_tint.color != particle_tint)
   {
      screensaver->particle_tint.color = particle_tint;
      screensaver->particle_tint.r     = (particle_tint >> 16) & 0xFF;
      screensaver->particle_tint.g     = (particle_tint >>  8) & 0xFF;
      screensaver->particle_tint.b     = (particle_tint      ) & 0xFF;
   }

   /* Create font, if required */
   if ((screensaver->effect != MENU_SCREENSAVER_BLANK) &&
       !screensaver->font_data.font &&
       screensaver->font_enabled)
   {
      char font_file[PATH_MAX_LENGTH];
#if defined(HAVE_FREETYPE) || (defined(__APPLE__) && defined(HAVE_CORETEXT)) || defined(HAVE_STB_FONT)
      char pkg_path[PATH_MAX_LENGTH];
      /* Get font file path */
      if (!string_is_empty(dir_assets))
         fill_pathname_join_special(pkg_path, dir_assets, MENU_SS_PKG_DIR, sizeof(pkg_path));
      else
         strlcpy(pkg_path, MENU_SS_PKG_DIR, sizeof(pkg_path));

      fill_pathname_join_special(font_file, pkg_path, MENU_SS_FONT_FILE,
            sizeof(font_file));

      /* Warn if font file is missing */
      if (!path_is_valid(font_file))
      {
         RARCH_WARN("[Menu Screensaver] Font asset missing: %s\n", font_file);
         screensaver->unicode_enabled = false;
      }
#else
      /* On platforms without TTF support, there is
       * no need to generate a font path (a bitmap
       * font will be created automatically) */
      font_file[0] = '\0';
#endif

      /* Create font */
      screensaver->font_data.font = gfx_display_font_file(p_disp,
            font_file, screensaver->font_size,
            video_driver_is_threaded());

      /* If font was created successfully, fetch metadata */
      if (screensaver->font_data.font)
         screensaver->font_data.y_centre_offset =
               (float)font_driver_get_line_centre_offset(
                     screensaver->font_data.font, 1.0f);
      /* In case of error, warn and disable
       * further attempts to create fonts */
      else
      {
         RARCH_WARN("[Menu Screensaver] Failed to initialise font - animation disabled\n");
         screensaver->font_enabled = false;
         return false;
      }
   }

   /* Initialise animation effect, if required */
   if (init_effect)
      return menu_screensaver_init_effect(screensaver);

   return true;
}

/* Processes screensaver animation logic
 * Called every frame on the main thread */
void menu_screensaver_iterate(
      menu_screensaver_t *screensaver,
      gfx_display_t *p_disp, gfx_animation_t *p_anim,
      enum menu_screensaver_effect effect, float effect_speed,
      uint32_t particle_tint, unsigned width, unsigned height,
      const char *dir_assets)
{
   float base_particle_size;
   uint32_t tint_r;
   uint32_t tint_g;
   uint32_t tint_b;
   float global_speed_factor;
   size_t i;

   if (!screensaver)
      return;

   /* Apply pending state changes */
   if (!menu_screensaver_update_state(
         screensaver, p_disp,
         effect, particle_tint,
         width, height, dir_assets) ||
       (screensaver->effect == MENU_SCREENSAVER_BLANK) ||
       !screensaver->particles)
      return;

   base_particle_size = screensaver->particle_scale * screensaver->font_size;
   tint_r             = screensaver->particle_tint.r;
   tint_g             = screensaver->particle_tint.g;
   tint_b             = screensaver->particle_tint.b;

   /* Set global animation speed */
   global_speed_factor = p_anim->delta_time / MENU_SS_EFFECT_PERIOD;
   if (effect_speed > 0.0001f) 
      global_speed_factor *= effect_speed;

   /* Update particle array */
   switch (screensaver->effect)
   {
      case MENU_SCREENSAVER_SNOW:
         for (i = 0; i < MENU_SS_NUM_PARTICLES; i++)
         {
            menu_ss_particle_t *particle = &screensaver->particles[i];
            float particle_size_px       = particle->size * base_particle_size;
            bool update_symbol           = false;
            float luminosity;

            /* Update particle 'speed' */
            particle->a = particle->a + (float)(rand() % 16 - 9) * 0.01f;
            particle->b = particle->b + (float)(rand() % 16 - 7) * 0.01f;

            particle->a = (particle->a < -0.4f) ? -0.4f : particle->a;
            particle->a = (particle->a >  0.1f) ?  0.1f : particle->a;

            particle->b = (particle->b < -0.1f) ? -0.1f : particle->b;
            particle->b = (particle->b >  0.4f) ?  0.4f : particle->b;

            /* Update particle location */
            particle->x = particle->x + (global_speed_factor * particle->size * particle->a);
            particle->y = particle->y + (global_speed_factor * particle->size * particle->b);

            /* Get particle colour */
            luminosity      = 0.5f + (particle->size / 6.0f);
            particle->color = MENU_SS_PARTICLE_COLOR(tint_r, tint_g, tint_b, luminosity);

            /* Reset particle if it has fallen off screen */
            if (particle->x < -particle_size_px)
            {
               particle->x   = (float)width + particle_size_px;
               update_symbol = true;
            }

            if (particle->y > (float)height + particle_size_px)
            {
               particle->y   = -particle_size_px;
               update_symbol = true;
            }

            if (update_symbol && screensaver->unicode_enabled)
               particle->symbol = menu_ss_snow_symbols[(unsigned)(rand() % MENU_SS_NUM_SNOW_SYMBOLS)];
         }
         break;
      case MENU_SCREENSAVER_STARFIELD:
         {
            float max_depth            = (float)(width > height ? width : height);
            float initial_speed_factor = 0.02f * max_depth / 240.0f;
            float focal_length         = max_depth * 2.0f;
            float x_centre             = (float)(width >> 1);
            float y_centre             = (float)(height >> 1);
            float particle_size_px;
            float luminosity;

            /* Based on an example found here:
             * https://codepen.io/nodws/pen/pejBNb */
            for (i = 0; i < MENU_SS_NUM_PARTICLES; i++)
            {
               menu_ss_particle_t *particle = &screensaver->particles[i];

               /* Get particle size */
               particle->size   = focal_length / (2.0f * particle->c);
               particle_size_px = particle->size * base_particle_size;

               /* Update depth */
               particle->c -= particle->d * global_speed_factor;

               /* Reset particle if it has:
                * - Dropped off the edge of the screen
                * - Reached the screen depth */
               if ((particle->x < -particle_size_px) ||
                   (particle->x > (float)width + particle_size_px) ||
                   (particle->y < -particle_size_px) ||
                   (particle->y > (float)height + particle_size_px) ||
                   (particle->c <= 0.0f))
               {
                  /* x pos ('physical' space) */
                  particle->a = (float)(rand() % width);
                  /* y pos ('physical' space) */
                  particle->b = (float)(rand() % height);
                  /* depth */
                  particle->c = max_depth;
                  /* speed */
                  particle->d = 1.0f + ((float)(rand() % 20) * initial_speed_factor);

                  /* Reset size */
                  particle->size = 1.0f;

                  /* If Unicode is supported, select a random
                   * star symbol */
                  if (screensaver->unicode_enabled)
                     particle->symbol = menu_ss_starfield_symbols[(unsigned)(rand() % MENU_SS_NUM_STARFIELD_SYMBOLS)];
               }

               /* Get particle location */
               particle->x = (particle->a - x_centre) * (focal_length / particle->c);
               particle->x += x_centre;

               particle->y = (particle->b - y_centre) * (focal_length / particle->c);
               particle->y += y_centre;

               /* Get particle colour */
               luminosity      = 0.25f + (0.75f - (particle->c / max_depth) * 0.75f);
               particle->color = MENU_SS_PARTICLE_COLOR(tint_r, tint_g, tint_b, luminosity);
            }

            /* Particles must be drawn in order of depth,
             * from furthest away to nearest */
            qsort(screensaver->particles,
                  MENU_SS_NUM_PARTICLES, sizeof(menu_ss_particle_t),
                  (int (*)(const void *, const void *))menu_ss_starfield_qsort_func);
         }
         break;
      case MENU_SCREENSAVER_VORTEX:
         {
            float min_screen_dimension = (float)(width < height ? width : height);
            float max_radius           = (float)sqrt((double)((width * width) + (height * height))) / 2.0f;
            float radial_speed_factor  = 0.001f * min_screen_dimension / 240.0f;
            float x_centre             = (float)(width >> 1);
            float y_centre             = (float)(height >> 1);
            float r_speed;
            float theta_speed;
            float size_factor;
            float luminosity;

            for (i = 0; i < MENU_SS_NUM_PARTICLES; i++)
            {
               menu_ss_particle_t *particle = &screensaver->particles[i];

               /* Update particle speed */
               r_speed     = particle->c * global_speed_factor;
               theta_speed = particle->d * global_speed_factor;
               if ((particle->a > 0.0f) && (particle->a < min_screen_dimension))
               {
                  float base_scale_factor = (min_screen_dimension - particle->a) / min_screen_dimension;
                  r_speed     *= 1.0f + (base_scale_factor * 8.0f);
                  theta_speed *= 1.0f + (base_scale_factor * base_scale_factor * 6.0f);
               }
               particle->a -= r_speed;
               particle->b += theta_speed;

               /* Reset particle if it has reached the centre of the screen */
               if (particle->a < 0.0f)
               {
                  /* radius
                   * Note: In theory, this should be:
                   * > particle->a = max_radius;
                   * ...but it turns out that spawning new particles at random
                   * locations produces a more visually appealing result... */
                  particle->a = 1.0f + (((float)rand() / (float)RAND_MAX) * max_radius);
                  /* theta */
                  particle->b = ((float)rand() / (float)RAND_MAX) * 2.0f * PI;
                  /* radial speed */
                  particle->c = (float)((rand() % 100) + 1) * radial_speed_factor;
                  /* rotational speed */
                  particle->d = (((float)((rand() % 50) + 1) * 0.005f) + 0.1f) * (PI / 360.0f);

                  /* If Unicode is supported, select a random
                   * star symbol */
                  if (screensaver->unicode_enabled)
                     particle->symbol = menu_ss_vortex_symbols[(unsigned)(rand() % MENU_SS_NUM_VORTEX_SYMBOLS)];
               }

               /* Get particle location */
               particle->x = (particle->a * cos(particle->b)) + x_centre;
               particle->y = (particle->a * sin(particle->b)) + y_centre;

               /* Get particle size */
               size_factor    = 1.0f - ((max_radius - particle->a) / max_radius);
               particle->size = sqrt(size_factor) * 2.5f;

               /* Get particle colour */
               luminosity      = 0.2f + (0.8f - size_factor * 0.8f);
               particle->color = MENU_SS_PARTICLE_COLOR(tint_r, tint_g, tint_b, luminosity);
            }

            /* Particles must be drawn in order of radius;
             * particles closest to the centre are further away
             * from the screen, and must be drawn first */
            qsort(screensaver->particles,
                  MENU_SS_NUM_PARTICLES, sizeof(menu_ss_particle_t),
                  (int (*)(const void *, const void *))menu_ss_vortex_qsort_func);
         }
         break;
      default:
         /* Error condition - do nothing */
         break;
   }
}

/* Draws screensaver
 * Called every frame (on the video thread,
 * if threaded video is on) */
void menu_screensaver_frame(menu_screensaver_t *screensaver,
      video_frame_info_t *video_info, gfx_display_t *p_disp)
{
   void *userdata = NULL;
   unsigned video_width;
   unsigned video_height;

   if (!screensaver)
      return;

   video_width  = video_info->width;
   video_height = video_info->height;
   userdata     = video_info->userdata;

   /* Set viewport */
   video_driver_set_viewport(video_width, video_height, true, false);

   /* Draw background */
   gfx_display_draw_quad(
         p_disp,
         userdata,
         video_width,
         video_height,
         0, 0,
         screensaver->last_width, screensaver->last_height,
         screensaver->last_width, screensaver->last_height,
         screensaver->bg_color,
         NULL);

   /* Draw particle effect, if required */
   if ((screensaver->effect != MENU_SCREENSAVER_BLANK) &&
       screensaver->font_data.font &&
       screensaver->particles)
   {
      font_data_t *font     = screensaver->font_data.font;
      float y_centre_offset = screensaver->font_data.y_centre_offset;
      float particle_scale  = screensaver->particle_scale;
      size_t i;

      /* Bind font */
      font_driver_bind_block(font, &screensaver->font_data.raster_block);
      screensaver->font_data.raster_block.carr.coords.vertices = 0;

      /* Render text */
      for (i = 0; i < MENU_SS_NUM_PARTICLES; i++)
      {
         menu_ss_particle_t *particle = &screensaver->particles[i];

         gfx_display_draw_text(
               font,
               particle->symbol,
               particle->x,
               particle->y + y_centre_offset,
               video_width, video_height,
               particle->color,
               TEXT_ALIGN_CENTER,
               particle->size * particle_scale,
               false, 0.0f, true);
      }

      /* Flush text and unbind font */
      if (screensaver->font_data.raster_block.carr.coords.vertices != 0)
      {
         font_driver_flush(video_width, video_height, font);
         screensaver->font_data.raster_block.carr.coords.vertices = 0;
      }
      font_driver_bind_block(font, NULL);
   }

   /* Unset viewport */
   video_driver_set_viewport(video_width, video_height, false, true);
}
