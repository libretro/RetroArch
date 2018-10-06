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

/* Menu shader */

static struct video_shader *menu_driver_shader = NULL;

struct video_shader *menu_shader_get(void)
{
   if (video_shader_any_supported())
      return menu_driver_shader;
   return NULL;
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

/**
 * menu_shader_manager_init:
 *
 * Initializes shader manager.
 **/
bool menu_shader_manager_init(void)
{
   bool is_preset              = false;
   config_file_t *conf         = NULL;
   char *new_path              = NULL;
   const char *path_shader     = retroarch_get_shader_preset();
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   menu_shader_manager_free();

   menu_driver_shader          = (struct video_shader*)
      calloc(1, sizeof(struct video_shader));

   if (!menu_driver_shader || !path_shader)
      return false;

   type = video_shader_get_type_from_ext(path_get_extension(path_shader),
         &is_preset);

   if (is_preset)
   {
      conf     = config_file_new(path_shader);
      new_path = strdup(path_shader);
   }
   else
   {
      if (video_shader_is_supported(type))
      {
         strlcpy(menu_driver_shader->pass[0].source.path, path_shader,
               sizeof(menu_driver_shader->pass[0].source.path));
         menu_driver_shader->passes = 1;
      }
      else
      {
         char preset_path[PATH_MAX_LENGTH];
         settings_t *settings        = config_get_ptr();
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

         new_path = strdup(preset_path);
      }
   }

   if (
         !string_is_empty(new_path) && conf &&
         video_shader_read_conf_cgp(conf, menu_driver_shader)
      )
   {
      video_shader_resolve_relative(menu_driver_shader, new_path);
      video_shader_resolve_parameters(conf, menu_driver_shader);
   }

   if (new_path)
      free(new_path);
   if (conf)
      config_file_free(conf);

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
bool menu_shader_manager_set_preset(void *data,
      unsigned type, const char *preset_path)
{
   struct video_shader *shader   = (struct video_shader*)data;
   config_file_t *conf           = NULL;
   bool refresh                  = false;
   settings_t *settings          = config_get_ptr();

   if (!video_driver_set_shader((enum rarch_shader_type)type, preset_path))
   {
      configuration_set_bool(settings, settings->bools.video_shader_enable, false);
      return false;
   }

   /* Makes sure that we use Menu Preset shader on driver reinit.
    * Only do this when the cgp actually works to avoid potential errors. */
   strlcpy(settings->paths.path_shader,
         preset_path ? preset_path : "",
         sizeof(settings->paths.path_shader));
   configuration_set_bool(settings, settings->bools.video_shader_enable, true);

   if (!preset_path || !shader)
      return false;

   /* Load stored Preset into menu on success.
    * Used when a preset is directly loaded.
    * No point in updating when the Preset was
    * created from the menu itself. */
   conf = config_file_new(preset_path);

   if (!conf)
      return false;

   RARCH_LOG("Setting Menu shader: %s.\n", preset_path);

   if (video_shader_read_conf_cgp(conf, shader))
   {
      video_shader_resolve_relative(shader, preset_path);
      video_shader_resolve_parameters(conf, shader);
   }
   config_file_free(conf);

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

   return true;
}

static const char *shader_get_preset_extension(unsigned type)
{
   switch (type)
   {
      case RARCH_SHADER_GLSL:
         return file_path_str(FILE_PATH_GLSLP_EXTENSION);
      case RARCH_SHADER_SLANG:
         return file_path_str(FILE_PATH_SLANGP_EXTENSION);
      case RARCH_SHADER_HLSL:
      case RARCH_SHADER_CG:
         return file_path_str(FILE_PATH_CGP_EXTENSION);
   }

   return NULL;
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
   char buffer[PATH_MAX_LENGTH];
   char preset_path[PATH_MAX_LENGTH];
   char config_directory[PATH_MAX_LENGTH];
   bool ret                               = false;
   unsigned d, type                       = RARCH_SHADER_NONE;
   const char *dirs[3]                    = {0};
   config_file_t *conf                    = NULL;
   struct video_shader *shader            = menu_shader_get();

   config_directory[0]                    = '\0';
   buffer[0]                              = '\0';
   preset_path[0]                         = '\0';

   if (!shader)
      return false;

   type = menu_shader_manager_get_type(shader);

   if (type == RARCH_SHADER_NONE)
      return false;

   if (basename)
   {
      strlcpy(buffer, basename, sizeof(buffer));

      /* Append extension automatically as appropriate. */
      if (     !strstr(basename,
               file_path_str(FILE_PATH_CGP_EXTENSION))
            && !strstr(basename,
               file_path_str(FILE_PATH_GLSLP_EXTENSION))
            && !strstr(basename,
               file_path_str(FILE_PATH_SLANGP_EXTENSION)))
      {
         const char *preset_ext = shader_get_preset_extension(type);
         if (!string_is_empty(preset_ext))
            strlcat(buffer, preset_ext, sizeof(buffer));
      }
   }
   else
   {
      char default_preset[PATH_MAX_LENGTH];

      default_preset[0] = '\0';

      if (video_shader_is_supported((enum rarch_shader_type)type))
      {
         const char *config_path     = path_get(RARCH_PATH_CONFIG);
         /* In a multi-config setting, we can't have
          * conflicts on menu.cgp/menu.glslp. */
         const char *preset_ext      = shader_get_preset_extension(type);

         if (!string_is_empty(preset_ext))
         {
            if (config_path)
            {
               fill_pathname_base_ext(default_preset,
                     config_path,
                     preset_ext,
                     sizeof(default_preset));
            }
            else
            {
               strlcpy(default_preset, "menu",
                     sizeof(default_preset));
               strlcat(default_preset,
                     preset_ext,
                     sizeof(default_preset));
            }
         }
      }

      if (!string_is_empty(default_preset))
         strlcpy(buffer, default_preset, sizeof(buffer));
   }

   if (!fullpath)
   {
      settings_t *settings = config_get_ptr();

      if (!path_is_empty(RARCH_PATH_CONFIG))
         fill_pathname_basedir(
               config_directory,
               path_get(RARCH_PATH_CONFIG),
               sizeof(config_directory));

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
   return false;
}

int menu_shader_manager_clear_num_passes(void)
{
   bool refresh                = false;
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return 0;

   if (shader->passes)
      shader->passes = 0;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   video_shader_resolve_parameters(NULL, shader);

   return 0;
}

int menu_shader_manager_clear_parameter(unsigned i)
{
   struct video_shader *shader          = menu_shader_get();
   struct video_shader_parameter *param = shader ?
      &shader->parameters[i] : NULL;

   if (!param)
      return 0;

   param->current = param->initial;
   param->current = MIN(MAX(param->minimum,
            param->current), param->maximum);

   return 0;
}

int menu_shader_manager_clear_pass_filter(unsigned i)
{
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (!shader_pass)
      return -1;

   shader_pass->filter = RARCH_FILTER_UNSPEC;

   return 0;
}

void menu_shader_manager_clear_pass_scale(unsigned i)
{
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (!shader_pass)
      return;

   shader_pass->fbo.scale_x = 0;
   shader_pass->fbo.scale_y = 0;
   shader_pass->fbo.valid   = false;
}

void menu_shader_manager_clear_pass_path(unsigned i)
{
   struct video_shader *shader           = menu_shader_get();
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (shader_pass)
      *shader_pass->source.path = '\0';
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

   return type;
}

/**
 * menu_shader_manager_apply_changes:
 *
 * Apply shader state changes.
 **/
void menu_shader_manager_apply_changes(void)
{
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
   shader_type = DEFAULT_SHADER_TYPE;

   if (shader_type == RARCH_SHADER_NONE)
   {
      if (video_shader_is_supported(RARCH_SHADER_GLSL))
         shader_type = RARCH_SHADER_GLSL;
      else if (
            video_shader_is_supported(RARCH_SHADER_CG) ||
            video_shader_is_supported(RARCH_SHADER_HLSL)
            )
         shader_type = RARCH_SHADER_CG;
      else if (video_shader_is_supported(RARCH_SHADER_SLANG))
         shader_type = RARCH_SHADER_SLANG;
   }
   menu_shader_manager_set_preset(NULL, shader_type, NULL);
}
