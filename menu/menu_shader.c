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
#include <streams/file_stream.h>

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
/* indicative of whether shader was modified from the menus: */
static bool menu_driver_shader_modified = true;

void menu_shader_set_modified(bool modified)
{
   menu_driver_shader_modified = modified;
}

static enum rarch_shader_type shader_types[] =
{
   RARCH_SHADER_GLSL, RARCH_SHADER_SLANG, RARCH_SHADER_CG
};

enum auto_shader_operation
{
   AUTO_SHADER_OP_SAVE = 0,
   AUTO_SHADER_OP_REMOVE,
   AUTO_SHADER_OP_EXISTS
};

struct video_shader *menu_shader_get(void)
{
   if (video_shader_any_supported())
      return menu_driver_shader;
   return NULL;
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
   enum rarch_shader_type type      = RARCH_SHADER_NONE;
   bool ret                         = true;
   bool is_preset                   = false;
   const char *path_shader          = NULL;
   struct video_shader *menu_shader = NULL;

   /* We get the shader preset directly from the video driver, so that
    * we are in sync with it (it could fail loading an auto-shader)
    * If we can't (e.g. get_current_shader is not implemented),
    * we'll load retroarch_get_shader_preset() like always */
   video_shader_ctx_t shader_info = {0};

   video_shader_driver_get_current_shader(&shader_info);

   if (shader_info.data)
      path_shader = shader_info.data->path;
   else
      path_shader = retroarch_get_shader_preset();

   menu_shader_manager_free();

   menu_shader          = (struct video_shader*)
      calloc(1, sizeof(*menu_shader));

   if (!menu_shader)
   {
      ret = false;
      goto end;
   }

   if (string_is_empty(path_shader))
      goto end;

   type = video_shader_get_type_from_ext(path_get_extension(path_shader),
         &is_preset);

   if (!video_shader_is_supported(type))
   {
      ret = false;
      goto end;
   }

   if (is_preset)
   {
      config_file_t *conf = NULL;

      conf = video_shader_read_preset(path_shader);

      if (!conf)
      {
         ret = false;
         goto end;
      }

      if (video_shader_read_conf_preset(conf, menu_shader))
         video_shader_resolve_parameters(conf, menu_shader);

      menu_driver_shader_modified = false;

      config_file_free(conf);
   }
   else
   {
      strlcpy(menu_shader->pass[0].source.path, path_shader,
            sizeof(menu_shader->pass[0].source.path));
      menu_shader->passes = 1;
   }

end:
   menu_driver_shader = menu_shader;
   command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
   return ret;
}

/**
 * menu_shader_manager_set_preset:
 * @shader                   : Shader handle.
 * @type                     : Type of shader.
 * @preset_path              : Preset path to load from.
 * @apply                    : Whether to apply the shader or just update shader information
 *
 * Sets shader preset.
 **/
bool menu_shader_manager_set_preset(struct video_shader *shader,
      enum rarch_shader_type type, const char *preset_path, bool apply)
{
   config_file_t *conf           = NULL;
   bool refresh                  = false;
   bool ret                      = false;

   if (apply && !retroarch_apply_shader(type, preset_path, true))
   {
      /* We don't want to disable shaders entirely here,
       * just reset number of passes
       * > Note: Disabling shaders at this point would in
       *   fact be dangerous, since it changes the number of
       *   entries in the shader options menu which can in
       *   turn lead to the menu selection pointer going out
       *   of bounds. This causes undefined behaviour/segfaults */
      menu_shader_manager_clear_num_passes(shader);
      command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
      return false;
   }

   if (string_is_empty(preset_path))
   {
      menu_shader_manager_clear_num_passes(shader);
      command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
      return true;
   }

   if (!shader)
   {
      ret = false;
      goto end;
   }

   /* Load stored Preset into menu on success.
    * Used when a preset is directly loaded.
    * No point in updating when the Preset was
    * created from the menu itself. */
   if (!(conf = video_shader_read_preset(preset_path)))
   {
      ret = false;
      goto end;
   }

   RARCH_LOG("Setting Menu shader: %s.\n", preset_path);

   if (video_shader_read_conf_preset(conf, shader))
      video_shader_resolve_parameters(conf, shader);

   if (conf)
      config_file_free(conf);

   ret = true;

end:
   command_event(CMD_EVENT_SHADER_PRESET_LOADED, NULL);
#ifdef HAVE_MENU
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
#endif
   return ret;
}

static bool menu_shader_manager_save_preset_internal(
      const struct video_shader *shader, const char *basename,
      bool apply, bool save_reference)
{
   bool ret                       = false;
   enum rarch_shader_type type    = RARCH_SHADER_NONE;
   char *preset_path              = NULL;
   size_t i                       = 0;
   char fullname[PATH_MAX_LENGTH];
   char buffer[PATH_MAX_LENGTH];

   fullname[0] = buffer[0] = '\0';

   if (!shader || !shader->passes)
      return false;

   type = menu_shader_manager_get_type(shader);

   if (type == RARCH_SHADER_NONE)
      return false;

   if (menu_driver_shader_modified)
      save_reference = false;

   if (!string_is_empty(basename))
   {
      strlcpy(fullname, basename, sizeof(fullname));

      /* Append extension automatically as appropriate. */
      if (     !strstr(basename, ".cgp")
            && !strstr(basename, ".glslp")
            && !strstr(basename, ".slangp"))
      {
         const char *preset_ext = video_shader_get_preset_extension(type);
         strlcat(fullname, preset_ext, sizeof(fullname));
      }
   }
   else
   {
      strlcpy(fullname, "retroarch", sizeof(fullname));
      strlcat(fullname, 
            video_shader_get_preset_extension(type), sizeof(fullname));
   }

   if (path_is_absolute(fullname))
   {
      preset_path = fullname;

      ret = video_shader_write_preset(preset_path, shader, save_reference);

      if (ret)
         RARCH_LOG("Saved shader preset to %s.\n", preset_path);
      else
         RARCH_ERR("Failed writing shader preset to %s.\n", preset_path);
   }
   else
   {
      const char *dirs[3]  = {0};
      settings_t *settings = config_get_ptr();
      char config_directory[PATH_MAX_LENGTH];

      config_directory[0] = '\0';

      if (!path_is_empty(RARCH_PATH_CONFIG))
         fill_pathname_basedir(
               config_directory,
               path_get(RARCH_PATH_CONFIG),
               sizeof(config_directory));

      dirs[0] = settings->paths.directory_video_shader;
      dirs[1] = settings->paths.directory_menu_config;
      dirs[2] = config_directory;

      for (i = 0; i < ARRAY_SIZE(dirs); i++)
      {
         if (string_is_empty(dirs[i]))
            continue;

         fill_pathname_join(buffer, dirs[i],
               fullname, sizeof(buffer));

         preset_path = buffer;

         ret = video_shader_write_preset(preset_path, shader, save_reference);

         if (ret)
         {
            RARCH_LOG("Saved shader preset to %s.\n", preset_path);
            break;
         }
         else
            RARCH_WARN("Failed writing shader preset to %s.\n", preset_path);
      }

      if (!ret)
         RARCH_ERR("Failed to write shader preset. Make sure shader directory"
               " and/or config directory are writable.\n");
   }

   if (ret && apply)
      menu_shader_manager_set_preset(NULL, type, preset_path, true);

   return ret;
}

static bool menu_shader_manager_operate_auto_preset(enum auto_shader_operation op,
      const struct video_shader *shader, enum auto_shader_type type, bool apply)
{
   char tmp[PATH_MAX_LENGTH];
   char directory[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   settings_t *settings             = config_get_ptr();
   struct retro_system_info *system = runloop_get_libretro_system_info();
   const char *core_name            = system ? system->library_name : NULL;

   tmp[0] = directory[0] = file[0] = '\0';

   if (type == SHADER_PRESET_GLOBAL)
   {
      fill_pathname_join(
            directory,
            settings->paths.directory_video_shader,
            "presets",
            sizeof(directory));
   }
   else if (string_is_empty(core_name))
      return false;
   else
   {
      fill_pathname_join(
            tmp,
            settings->paths.directory_video_shader,
            "presets",
            sizeof(tmp));
      fill_pathname_join(
            directory,
            tmp,
            core_name,
            sizeof(directory));
   }

   switch (type)
   {
      case SHADER_PRESET_GLOBAL:
         fill_pathname_join(file, directory, "global", sizeof(file));
         break;
      case SHADER_PRESET_CORE:
         fill_pathname_join(file, directory, core_name, sizeof(file));
         break;
      case SHADER_PRESET_PARENT:
         fill_pathname_parent_dir_name(tmp,
               path_get(RARCH_PATH_BASENAME), sizeof(tmp));
         fill_pathname_join(file, directory, tmp, sizeof(file));
         break;
      case SHADER_PRESET_GAME:
         {
            const char *game_name = 
               path_basename(path_get(RARCH_PATH_BASENAME));
            if (string_is_empty(game_name))
               return false;
            fill_pathname_join(file, directory, game_name, sizeof(file));
            break;
         }
      default:
         return false;
   }

   if (op == AUTO_SHADER_OP_SAVE)
   {
      if (!path_is_directory(directory))
         path_mkdir(directory);

      return menu_shader_manager_save_preset_internal(
            shader, file, apply, true);
   }
   else if (op == AUTO_SHADER_OP_REMOVE)
   {
      /* remove all supported auto-shaders of given type */
      char *end = file + strlen(file);
      size_t i;
      bool success = false;

      for (i = 0; i < ARRAY_SIZE(shader_types); i++)
      {
         const char *preset_ext;

         if (!video_shader_is_supported(shader_types[i]))
            continue;

         preset_ext = video_shader_get_preset_extension(shader_types[i]);
         strlcpy(end, preset_ext, sizeof(file) - (end-file));

         if (!filestream_delete(file))
            success = true;
      }
      return success;
   }
   else if (op == AUTO_SHADER_OP_EXISTS)
   {
      /* test if any supported auto-shaders of given type exists */
      char *end = file + strlen(file);
      size_t i;

      for (i = 0; i < ARRAY_SIZE(shader_types); i++)
      {
         const char *preset_ext;

         if (!video_shader_is_supported(shader_types[i]))
            continue;

         preset_ext = video_shader_get_preset_extension(shader_types[i]);
         strlcpy(end, preset_ext, sizeof(file) - (end-file));

         if (path_is_valid(file))
            return true;
      }
   }

   return false;
}

/**
 * menu_shader_manager_save_auto_preset:
 * @shader                   : shader to save
 * @type                     : type of shader preset which determines save path
 * @apply                    : immediately set preset after saving
 *
 * Save a shader as an auto-shader to it's appropriate path:
 *    SHADER_PRESET_GLOBAL: <shader dir>/presets/global
 *    SHADER_PRESET_CORE:   <shader dir>/presets/<core name>/<core name>
 *    SHADER_PRESET_PARENT: <shader dir>/presets/<core name>/<parent>
 *    SHADER_PRESET_GAME:   <shader dir>/presets/<core name>/<game name>
 * Needs to be consistent with retroarch_load_shader_preset()
 * Auto-shaders will be saved as a reference if possible
 **/
bool menu_shader_manager_save_auto_preset(const struct video_shader *shader,
      enum auto_shader_type type, bool apply)
{
   return menu_shader_manager_operate_auto_preset(
         AUTO_SHADER_OP_SAVE, shader, type, apply);
}

/**
 * menu_shader_manager_save_preset:
 * @shader                   : shader to save
 * @type                     : type of shader preset which determines save path
 * @basename                 : basename of preset
 * @apply                    : immediately set preset after saving
 *
 * Save a shader preset to disk.
 **/
bool menu_shader_manager_save_preset(const struct video_shader *shader,
      const char *basename, bool apply)
{
   return menu_shader_manager_save_preset_internal(
         shader, basename, apply, false);
}

/**
 * menu_shader_manager_remove_auto_preset:
 * @type                     : type of shader preset to delete
 *
 * Deletes an auto-shader.
 **/
bool menu_shader_manager_remove_auto_preset(enum auto_shader_type type)
{
   return menu_shader_manager_operate_auto_preset(
         AUTO_SHADER_OP_REMOVE, NULL, type, false);
}

/**
 * menu_shader_manager_auto_preset_exists:
 * @type                     : type of shader preset
 *
 * Tests if an auto-shader of the given type exists.
 **/
bool menu_shader_manager_auto_preset_exists(enum auto_shader_type type)
{
   return menu_shader_manager_operate_auto_preset(
         AUTO_SHADER_OP_EXISTS, NULL, type, false);
}

int menu_shader_manager_clear_num_passes(struct video_shader *shader)
{
   bool refresh                = false;

   if (!shader)
      return 0;

   shader->passes = 0;

#ifdef HAVE_MENU
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
#endif

   video_shader_resolve_parameters(NULL, shader);

   menu_driver_shader_modified = true;

   return 0;
}

int menu_shader_manager_clear_parameter(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_parameter *param = shader ?
      &shader->parameters[i] : NULL;

   if (!param)
      return 0;

   param->current = param->initial;
   param->current = MIN(MAX(param->minimum,
            param->current), param->maximum);

   menu_driver_shader_modified = true;

   return 0;
}

int menu_shader_manager_clear_pass_filter(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (!shader_pass)
      return -1;

   shader_pass->filter = RARCH_FILTER_UNSPEC;

   menu_driver_shader_modified = true;

   return 0;
}

void menu_shader_manager_clear_pass_scale(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (!shader_pass)
      return;

   shader_pass->fbo.scale_x = 0;
   shader_pass->fbo.scale_y = 0;
   shader_pass->fbo.valid   = false;

   menu_driver_shader_modified = true;
}

void menu_shader_manager_clear_pass_path(struct video_shader *shader,
      unsigned i)
{
   struct video_shader_pass *shader_pass = shader ?
      &shader->pass[i] : NULL;

   if (shader_pass)
      *shader_pass->source.path = '\0';

   menu_driver_shader_modified = true;
}

/**
 * menu_shader_manager_get_type:
 * @shader                   : shader handle
 *
 * Gets type of shader.
 *
 * Returns: type of shader.
 **/
enum rarch_shader_type menu_shader_manager_get_type(
      const struct video_shader *shader)
{
   enum rarch_shader_type type       = RARCH_SHADER_NONE;
   /* All shader types must be the same, or we cannot use it. */
   size_t i                         = 0;

   if (!shader)
      return RARCH_SHADER_NONE;

   type = video_shader_parse_type(shader->path);

   if (!shader->passes)
      return type;

   if (type == RARCH_SHADER_NONE)
   {
      type = video_shader_parse_type(shader->pass[0].source.path);
      i = 1;
   }

   for (; i < shader->passes; i++)
   {
      enum rarch_shader_type pass_type =
         video_shader_parse_type(shader->pass[i].source.path);

      switch (pass_type)
      {
         case RARCH_SHADER_CG:
         case RARCH_SHADER_GLSL:
         case RARCH_SHADER_SLANG:
            if (type != pass_type)
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
void menu_shader_manager_apply_changes(struct video_shader *shader)
{
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   if (!shader)
      return;

   type = menu_shader_manager_get_type(shader);

   if (shader->passes && type != RARCH_SHADER_NONE)
   {
      menu_shader_manager_save_preset(shader, NULL, true);
      return;
   }

   menu_shader_manager_set_preset(NULL, type, NULL, true);
}
