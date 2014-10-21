/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "menu_shader.h"
#include "menu_action.h"
#include "menu_common.h"
#include "menu_entries.h"
#include <file/file_path.h>
#include "../../settings_data.h"

#ifdef HAVE_SHADER_MANAGER

void menu_shader_manager_init(void *data)
{
   char cgp_path[PATH_MAX];
   struct gfx_shader *shader = NULL;
   config_file_t *conf = NULL;
   const char *config_path = NULL;
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;

   shader = (struct gfx_shader*)menu->shader;

   if (*g_extern.core_specific_config_path
         && g_settings.core_specific_config)
      config_path = g_extern.core_specific_config_path;
   else if (*g_extern.config_path)
      config_path = g_extern.config_path;

   /* In a multi-config setting, we can't have
    * conflicts on menu.cgp/menu.glslp. */
   if (config_path)
   {
      fill_pathname_base(menu->default_glslp, config_path,
            sizeof(menu->default_glslp));
      path_remove_extension(menu->default_glslp);
      strlcat(menu->default_glslp, ".glslp", sizeof(menu->default_glslp));
      fill_pathname_base(menu->default_cgp, config_path,
            sizeof(menu->default_cgp));
      path_remove_extension(menu->default_cgp);
      strlcat(menu->default_cgp, ".cgp", sizeof(menu->default_cgp));
   }
   else
   {
      strlcpy(menu->default_glslp, "menu.glslp", sizeof(menu->default_glslp));
      strlcpy(menu->default_cgp, "menu.cgp", sizeof(menu->default_cgp));
   }


   const char *ext = path_get_extension(g_settings.video.shader_path);
   if (strcmp(ext, "glslp") == 0 || strcmp(ext, "cgp") == 0)
   {
      conf = config_file_new(g_settings.video.shader_path);
      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, shader))
         {
            gfx_shader_resolve_relative(shader, g_settings.video.shader_path);
            gfx_shader_resolve_parameters(conf, shader);
         }
         config_file_free(conf);
      }
   }
   else if (strcmp(ext, "glsl") == 0 || strcmp(ext, "cg") == 0)
   {
      strlcpy(shader->pass[0].source.path, g_settings.video.shader_path,
            sizeof(shader->pass[0].source.path));
      shader->passes = 1;
   }
   else
   {
      const char *shader_dir = *g_settings.video.shader_dir ?
         g_settings.video.shader_dir : g_settings.system_directory;

      fill_pathname_join(cgp_path, shader_dir, "menu.glslp", sizeof(cgp_path));
      conf = config_file_new(cgp_path);

      if (!conf)
      {
         fill_pathname_join(cgp_path, shader_dir, "menu.cgp", sizeof(cgp_path));
         conf = config_file_new(cgp_path);
      }

      if (conf)
      {
         if (gfx_shader_read_conf_cgp(conf, shader))
         {
            gfx_shader_resolve_relative(shader, cgp_path);
            gfx_shader_resolve_parameters(conf, shader);
         }
         config_file_free(conf);
      }
   }
}

void menu_shader_manager_set_preset(struct gfx_shader *shader,
      unsigned type, const char *cgp_path)
{
   RARCH_LOG("Setting Menu shader: %s.\n", cgp_path ? cgp_path : "N/A (stock)");
   g_settings.video.shader_enable = false;

   if (driver.video->set_shader && driver.video->set_shader(driver.video_data,
            (enum rarch_shader_type)type, cgp_path))
   {
      /* Makes sure that we use Menu CGP shader on driver reinit.
       * Only do this when the cgp actually works to avoid potential errors. */
      strlcpy(g_settings.video.shader_path, cgp_path ? cgp_path : "",
            sizeof(g_settings.video.shader_path));
      g_settings.video.shader_enable = true;

      if (cgp_path && shader)
      {
         /* Load stored CGP into menu on success.
          * Used when a preset is directly loaded.
          * No point in updating when the CGP was 
          * created from the menu itself. */
         config_file_t *conf = config_file_new(cgp_path);

         if (conf)
         {
            if (gfx_shader_read_conf_cgp(conf, shader))
            {
               gfx_shader_resolve_relative(shader, cgp_path);
               gfx_shader_resolve_parameters(conf, shader);
            }
            config_file_free(conf);
         }

         driver.menu->need_refresh = true;
      }
   }
}

void menu_shader_manager_get_str(struct gfx_shader *program,
      char *type_str, size_t type_str_size, const char *menu_label,
      const char *label, unsigned type)
{
   *type_str = '\0';

   if (!strcmp(label, "video_shader_num_passes"))
      snprintf(type_str, type_str_size, "%u", program->passes);
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      /* menu->parameter_shader here. */
      if (program)
      {
         const struct gfx_shader_parameter *param =
            (const struct gfx_shader_parameter*)&program->parameters
            [type - MENU_SETTINGS_SHADER_PARAMETER_0];
         snprintf(type_str, type_str_size, "%.2f [%.2f %.2f]",
               param->current, param->minimum, param->maximum);
      }
   }
   else if (!strcmp(label, "video_shader_default_filter"))
      snprintf(type_str, type_str_size, "%s",
            g_settings.video.smooth ? "Linear" : "Nearest");
   else if (!strcmp(label, "video_shader_pass"))
   {
      unsigned pass = (type - MENU_SETTINGS_SHADER_PASS_0);
      if (*program->pass[pass].source.path)
         fill_pathname_base(type_str,
               program->pass[pass].source.path, type_str_size);
      else
         strlcpy(type_str, "N/A", type_str_size);
   }
   else if (!strcmp(label, "video_shader_filter_pass"))
   {
      unsigned pass = (type - MENU_SETTINGS_SHADER_PASS_FILTER_0);
      static const char *modes[] = {
         "Don't care",
         "Linear",
         "Nearest"
      };

      strlcpy(type_str, modes[program->pass[pass].filter],
            type_str_size);
   }
   else if (!strcmp(label, "video_shader_scale_pass"))
   {
      unsigned pass        = (type - MENU_SETTINGS_SHADER_PASS_SCALE_0);
      unsigned scale_value = program->pass[pass].fbo.scale_x;
      if (!scale_value)
         strlcpy(type_str, "Don't care", type_str_size);
      else
         snprintf(type_str, type_str_size, "%ux", scale_value);
   }
}

void menu_shader_manager_save_preset(
      const char *basename, bool apply)
{
   char buffer[PATH_MAX], config_directory[PATH_MAX], cgp_path[PATH_MAX];
   unsigned d, type = RARCH_SHADER_NONE;
   config_file_t *conf = NULL;
   bool ret = false;

   if (!driver.menu)
   {
      RARCH_ERR("Cannot save shader preset, menu handle is not initialized.\n");
      return;
   }

   type = menu_shader_manager_get_type(driver.menu->shader);

   if (type == RARCH_SHADER_NONE)
      return;

   *config_directory = '\0';

   if (basename)
   {
      strlcpy(buffer, basename, sizeof(buffer));

      /* Append extension automatically as appropriate. */
      if (!strstr(basename, ".cgp") && !strstr(basename, ".glslp"))
      {
         if (type == RARCH_SHADER_GLSL)
            strlcat(buffer, ".glslp", sizeof(buffer));
         else if (type == RARCH_SHADER_CG)
            strlcat(buffer, ".cgp", sizeof(buffer));
      }
   }
   else
   {
      const char *conf_path = (type == RARCH_SHADER_GLSL) ?
         driver.menu->default_glslp : driver.menu->default_cgp;
      strlcpy(buffer, conf_path, sizeof(buffer));
   }

   if (*g_extern.config_path)
      fill_pathname_basedir(config_directory,
            g_extern.config_path, sizeof(config_directory));

   const char *dirs[] = {
      g_settings.video.shader_dir,
      g_settings.menu_config_directory,
      config_directory,
   };

   if (!(conf = (config_file_t*)config_file_new(NULL)))
      return;
   gfx_shader_write_conf_cgp(conf, driver.menu->shader);

   for (d = 0; d < ARRAY_SIZE(dirs); d++)
   {
      if (!*dirs[d])
         continue;

      fill_pathname_join(cgp_path, dirs[d], buffer, sizeof(cgp_path));
      if (config_file_write(conf, cgp_path))
      {
         RARCH_LOG("Saved shader preset to %s.\n", cgp_path);
         if (apply)
            menu_shader_manager_set_preset(NULL, type, cgp_path);
         ret = true;
         break;
      }
      else
         RARCH_LOG("Failed writing shader preset to %s.\n", cgp_path);
   }

   config_file_free(conf);
   if (!ret)
      RARCH_ERR("Failed to save shader preset. Make sure config directory and/or shader dir are writable.\n");
}

unsigned menu_shader_manager_get_type(const struct gfx_shader *shader)
{
   /* All shader types must be the same, or we cannot use it. */
   unsigned i = 0, type = 0;

   if (!shader)
      return RARCH_SHADER_NONE;

   for (i = 0; i < shader->passes; i++)
   {
      enum rarch_shader_type pass_type = 
         gfx_shader_parse_type(shader->pass[i].source.path,
            RARCH_SHADER_NONE);

      switch (pass_type)
      {
         case RARCH_SHADER_CG:
         case RARCH_SHADER_GLSL:
            if (type == RARCH_SHADER_NONE)
               type = pass_type;
            else if (type != pass_type)
               return RARCH_SHADER_NONE;
            break;
         default:
            return RARCH_SHADER_NONE;
      }
   }

   return type;
}

void menu_shader_manager_apply_changes(void)
{
   unsigned shader_type = menu_shader_manager_get_type(driver.menu->shader);

   if (driver.menu->shader->passes 
         && shader_type != RARCH_SHADER_NONE)
   {
      menu_shader_manager_save_preset(NULL, true);
      return;
   }

   /* Fall-back */
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
   shader_type = gfx_shader_parse_type("", DEFAULT_SHADER_TYPE);
#endif

   if (shader_type == RARCH_SHADER_NONE)
   {
#if defined(HAVE_GLSL)
      shader_type = RARCH_SHADER_GLSL;
#elif defined(HAVE_CG) || defined(HAVE_HLSL)
      shader_type = RARCH_SHADER_CG;
#endif
   }
   menu_shader_manager_set_preset(NULL, shader_type, NULL);
}

#else

void menu_shader_manager_init(void *data) {}
void menu_shader_manager_set_preset(struct gfx_shader *shader,
      unsigned type, const char *cgp_path) { }

void menu_shader_manager_save_preset(
      const char *basename, bool apply) { }

unsigned menu_shader_manager_get_type(const struct gfx_shader *shader)
{
   return RARCH_SHADER_NONE;
}

void menu_shader_manager_get_str(struct gfx_shader *shader,
      char *type_str, size_t type_str_size, const char *menu_label,
      const char *label, unsigned type) { }

void menu_shader_manager_apply_changes(void) { }

#endif
