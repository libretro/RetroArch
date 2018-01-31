/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <compat/strl.h>
#include <retro_assert.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "menu_driver.h"
#include "menu_shader.h"
#include "../file_path_special.h"
#include "../configuration.h"
#include "../paths.h"
#include "../retroarch.h"
#include "../verbosity.h"

#ifdef HAVE_SHADER_MANAGER
/* Menu shader */
static char default_glslp[PATH_MAX_LENGTH];
static char default_cgp[PATH_MAX_LENGTH];
static char default_slangp[PATH_MAX_LENGTH];
static struct video_shader *menu_driver_shader = NULL;

struct video_shader *menu_shader_get(void)
{
   return menu_driver_shader;
}

struct video_shader_parameter *menu_shader_manager_get_parameters(unsigned i)
{
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return NULL;

   return &shader->parameters[i];
}

struct video_shader_pass *menu_shader_manager_get_pass(unsigned i)
{
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return NULL;

   return &shader->pass[i];
}

unsigned menu_shader_manager_get_amount_passes(void)
{
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return 0;

   return shader->passes;
}

void menu_shader_manager_decrement_amount_passes(void)
{
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return;

   shader->passes--;
}

void menu_shader_manager_increment_amount_passes(void)
{
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return;

   shader->passes++;
}

void menu_shader_manager_free(void)
{
   if (menu_driver_shader)
      free(menu_driver_shader);
   menu_driver_shader = NULL;
}
#else
struct video_shader *menu_shader_get(void)
{
   return NULL;
}

struct video_shader_parameter *menu_shader_manager_get_parameters(unsigned i)
{
   return NULL;
}

struct video_shader_pass *menu_shader_manager_get_pass(unsigned i)
{
   return NULL;
}

unsigned menu_shader_manager_get_amount_passes(void) { return 0; }
void menu_shader_manager_free(void) { }
#endif

/**
 * menu_shader_manager_init:
 *
 * Initializes shader manager.
 **/
bool menu_shader_manager_init(void)
{
#ifdef HAVE_SHADER_MANAGER
   settings_t *settings        = config_get_ptr();
   const char *config_path     = path_get(RARCH_PATH_CONFIG);
   const char *path_shader     = retroarch_get_shader_preset();

   menu_shader_manager_free();

   menu_driver_shader          = (struct video_shader*)
      calloc(1, sizeof(struct video_shader));

   if (!menu_driver_shader || !path_shader)
      return false;

   /* In a multi-config setting, we can't have
    * conflicts on menu.cgp/menu.glslp. */
   if (config_path)
   {
      fill_pathname_base_ext(default_glslp, config_path,
            file_path_str(FILE_PATH_GLSLP_EXTENSION),
            sizeof(default_glslp));

      fill_pathname_base_ext(default_cgp, config_path,
            file_path_str(FILE_PATH_CGP_EXTENSION),
            sizeof(default_cgp));

      fill_pathname_base_ext(default_slangp, config_path,
            file_path_str(FILE_PATH_SLANGP_EXTENSION),
            sizeof(default_slangp));
   }
   else
   {
      strlcpy(default_glslp, "menu.glslp",
            sizeof(default_glslp));
      strlcpy(default_cgp, "menu.cgp",
            sizeof(default_cgp));
      strlcpy(default_slangp, "menu.slangp",
            sizeof(default_slangp));
   }

   switch (msg_hash_to_file_type(msg_hash_calculate(
               path_get_extension(path_shader))))
   {
      case FILE_TYPE_SHADER_PRESET_GLSLP:
      case FILE_TYPE_SHADER_PRESET_CGP:
      case FILE_TYPE_SHADER_PRESET_SLANGP:
         {
            config_file_t *conf = config_file_new(path_shader);

            if (conf)
            {
               if (video_shader_read_conf_cgp(conf, menu_driver_shader))
               {
                  video_shader_resolve_relative(menu_driver_shader,
                        path_shader);
                  video_shader_resolve_parameters(conf, menu_driver_shader);
               }
               config_file_free(conf);
            }
         }
         break;
      case FILE_TYPE_SHADER_GLSL:
      case FILE_TYPE_SHADER_CG:
      case FILE_TYPE_SHADER_SLANG:
         strlcpy(menu_driver_shader->pass[0].source.path, path_shader,
               sizeof(menu_driver_shader->pass[0].source.path));
         menu_driver_shader->passes = 1;
         break;
      default:
         {
            char preset_path[PATH_MAX_LENGTH];
            config_file_t *conf               = NULL;
            const char *shader_dir            =
               *settings->paths.directory_video_shader ?
               settings->paths.directory_video_shader :
               settings->paths.directory_system;

            preset_path[0] = '\0';

            fill_pathname_join(preset_path, shader_dir,
                  "menu.glslp", sizeof(preset_path));
            conf = config_file_new(preset_path);

            if (!conf)
            {
               fill_pathname_join(preset_path, shader_dir,
                     "menu.cgp", sizeof(preset_path));
               conf = config_file_new(preset_path);
            }

            if (!conf)
            {
               fill_pathname_join(preset_path, shader_dir,
                     "menu.slangp", sizeof(preset_path));
               conf = config_file_new(preset_path);
            }

            if (conf)
            {
               if (video_shader_read_conf_cgp(conf, menu_driver_shader))
               {
                  video_shader_resolve_relative(menu_driver_shader, preset_path);
                  video_shader_resolve_parameters(conf, menu_driver_shader);
               }
               config_file_free(conf);
            }
         }
         break;
   }
#endif

   return true;
}

/**
 * menu_shader_manager_set_preset:
 * @shader                   : Shader handle.
 * @type                     : Type of shader.
 * @preset_path              : Preset path to load from.
 *
 * Sets shader preset.
 **/
void menu_shader_manager_set_preset(void *data,
      unsigned type, const char *preset_path)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader   = (struct video_shader*)data;
   config_file_t *conf           = NULL;
   bool refresh                  = false;
   settings_t *settings          = config_get_ptr();

   if (!video_driver_set_shader((enum rarch_shader_type)type, preset_path))
   {
      configuration_set_bool(settings, settings->bools.video_shader_enable, false);
      return;
   }

   /* Makes sure that we use Menu Preset shader on driver reinit.
    * Only do this when the cgp actually works to avoid potential errors. */
   strlcpy(settings->paths.path_shader,
         preset_path ? preset_path : "",
         sizeof(settings->paths.path_shader));
   configuration_set_bool(settings, settings->bools.video_shader_enable, true);

   if (!preset_path || !shader)
      return;

   /* Load stored Preset into menu on success.
    * Used when a preset is directly loaded.
    * No point in updating when the Preset was
    * created from the menu itself. */
   conf = config_file_new(preset_path);

   if (!conf)
      return;

   RARCH_LOG("Setting Menu shader: %s.\n", preset_path);

   if (video_shader_read_conf_cgp(conf, shader))
   {
      video_shader_resolve_relative(shader, preset_path);
      video_shader_resolve_parameters(conf, shader);
   }
   config_file_free(conf);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
#endif
}

/**
 * menu_shader_manager_save_preset:
 * @basename                 : basename of preset
 * @apply                    : immediately set preset after saving
 *
 * Save a shader preset to disk.
 **/
bool menu_shader_manager_save_preset(
      const char *basename, bool apply, bool fullpath)
{
#ifdef HAVE_SHADER_MANAGER
   bool ret                               = false;
   char buffer[PATH_MAX_LENGTH];
   char config_directory[PATH_MAX_LENGTH];
   char preset_path[PATH_MAX_LENGTH];
   unsigned d, type                       = RARCH_SHADER_NONE;
   const char *dirs[3]                    = {0};
   config_file_t *conf                    = NULL;
   struct video_shader *shader            = menu_shader_get();

   buffer[0] = config_directory[0]        = '\0';
   preset_path[0]                         = '\0';


   if (!shader)
      return false;

   type = menu_shader_manager_get_type(shader);

   if (type == RARCH_SHADER_NONE)
      return false;

   *config_directory = '\0';

   if (basename)
   {
      strlcpy(buffer, basename, sizeof(buffer));

      /* Append extension automatically as appropriate. */
      if (     !strstr(basename, file_path_str(FILE_PATH_CGP_EXTENSION))
            && !strstr(basename, file_path_str(FILE_PATH_GLSLP_EXTENSION))
            && !strstr(basename, file_path_str(FILE_PATH_SLANGP_EXTENSION)))
      {
         switch (type)
         {
            case RARCH_SHADER_GLSL:
               strlcat(buffer,
                     file_path_str(FILE_PATH_GLSLP_EXTENSION),
                     sizeof(buffer));
               break;
            case RARCH_SHADER_SLANG:
               strlcat(buffer,
                     file_path_str(FILE_PATH_SLANGP_EXTENSION),
                     sizeof(buffer));
               break;
            case RARCH_SHADER_CG:
               strlcat(buffer,
                     file_path_str(FILE_PATH_CGP_EXTENSION),
                     sizeof(buffer));
               break;
         }
      }
   }
   else
   {
      const char *conf_path = NULL;
      switch (type)
      {
         case RARCH_SHADER_GLSL:
            conf_path = default_glslp;
            break;

         case RARCH_SHADER_SLANG:
            conf_path = default_slangp;
            break;

         default:
         case RARCH_SHADER_CG:
            conf_path = default_cgp;
            break;
      }

      if (!string_is_empty(conf_path))
         strlcpy(buffer, conf_path, sizeof(buffer));
   }

   if (!path_is_empty(RARCH_PATH_CONFIG))
      fill_pathname_basedir(
            config_directory,
            path_get(RARCH_PATH_CONFIG),
            sizeof(config_directory));

   if (!fullpath)
   {
      settings_t *settings = config_get_ptr();
      dirs[0]              = settings->paths.directory_video_shader;
      dirs[1]              = settings->paths.directory_menu_config;
      dirs[2]              = config_directory;
   }

   conf = (config_file_t*)config_file_new(NULL);

   if (!conf)
      return false;

   video_shader_write_conf_cgp(conf, shader);

   if (fullpath)
   {
      if (!string_is_empty(basename))
         strlcpy(preset_path, buffer, sizeof(preset_path));

      if (config_file_write(conf, preset_path))
      {
         RARCH_LOG("Saved shader preset to %s.\n", preset_path);
         if (apply)
            menu_shader_manager_set_preset(NULL, type, preset_path);
         ret = true;
      }
      else
         RARCH_LOG("Failed writing shader preset to %s.\n", preset_path);
   }
   else
   {
      for (d = 0; d < ARRAY_SIZE(dirs); d++)
      {
         if (!*dirs[d])
            continue;

         fill_pathname_join(preset_path, dirs[d],
               buffer, sizeof(preset_path));

         if (config_file_write(conf, preset_path))
         {
            RARCH_LOG("Saved shader preset to %s.\n", preset_path);
            if (apply)
               menu_shader_manager_set_preset(NULL, type, preset_path);
            ret = true;
            break;
         }
         else
            RARCH_LOG("Failed writing shader preset to %s.\n", preset_path);
      }
   }

   config_file_free(conf);
   if (ret)
      return true;

   RARCH_ERR("Failed to save shader preset. Make sure config directory"
         " and/or shader dir are writable.\n");
#endif
   return false;
}

int menu_shader_manager_clear_num_passes(void)
{
#ifdef HAVE_SHADER_MANAGER
   bool refresh                = false;
   struct video_shader *shader = menu_shader_get();

   if (shader->passes)
      shader->passes = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   video_shader_resolve_parameters(NULL, shader);
#endif

   return 0;
}

int menu_shader_manager_clear_parameter(unsigned i)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader_parameter *param =
      menu_shader_manager_get_parameters(i);

   if (!param)
      return 0;

   param->current = param->initial;
   param->current = MIN(MAX(param->minimum,
            param->current), param->maximum);
#endif

   return 0;
}

int menu_shader_manager_clear_pass_filter(unsigned i)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader_pass *shader_pass =
      menu_shader_manager_get_pass(i);

   if (!shader_pass)
      return -1;

   shader_pass->filter = RARCH_FILTER_UNSPEC;

   return 0;
#else
   return -1;
#endif
}

void menu_shader_manager_clear_pass_scale(unsigned i)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader_pass *shader_pass =
      menu_shader_manager_get_pass(i);

   if (!shader_pass)
      return;

   shader_pass->fbo.scale_x = 0;
   shader_pass->fbo.scale_y = 0;
   shader_pass->fbo.valid   = false;
#endif
}

void menu_shader_manager_clear_pass_path(unsigned i)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader_pass *shader_pass =
      menu_shader_manager_get_pass(i);

   if (shader_pass)
      *shader_pass->source.path = '\0';
#endif
}

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle
 *
 * Gets type of shader.
 *
 * Returns: type of shader.
 **/
unsigned menu_shader_manager_get_type(const void *data)
{
   unsigned type                     = RARCH_SHADER_NONE;
#ifdef HAVE_SHADER_MANAGER
   const struct video_shader *shader = (const struct video_shader*)data;
   /* All shader types must be the same, or we cannot use it. */
   uint8_t i                         = 0;

   if (!shader)
      return RARCH_SHADER_NONE;

   for (i = 0; i < shader->passes; i++)
   {
      enum rarch_shader_type pass_type =
         video_shader_parse_type(shader->pass[i].source.path,
               RARCH_SHADER_NONE);

      switch (pass_type)
      {
         case RARCH_SHADER_CG:
         case RARCH_SHADER_GLSL:
         case RARCH_SHADER_SLANG:
            if (type == RARCH_SHADER_NONE)
               type = pass_type;
            else if (type != pass_type)
               return RARCH_SHADER_NONE;
            break;
         default:
            break;
      }
   }

#endif
   return type;
}

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(void)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned shader_type;
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return;

   shader_type = menu_shader_manager_get_type(shader);

   if (shader->passes && shader_type != RARCH_SHADER_NONE)
   {
      menu_shader_manager_save_preset(NULL, true, false);
      return;
   }

   /* Fall-back */
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   shader_type = video_shader_parse_type("", DEFAULT_SHADER_TYPE);
#endif

   if (shader_type == RARCH_SHADER_NONE)
   {
#if defined(HAVE_GLSL)
      shader_type = RARCH_SHADER_GLSL;
#elif defined(HAVE_CG) || defined(HAVE_HLSL)
      shader_type = RARCH_SHADER_CG;
#elif defined(HAVE_SLANG)
      shader_type = RARCH_SHADER_SLANG;
#endif
   }
   menu_shader_manager_set_preset(NULL, shader_type, NULL);
#endif
}
