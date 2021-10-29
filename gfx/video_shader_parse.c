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
 *  If not, see <http:www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <libretro.h>
#include <compat/posix_string.h>
#include <compat/msvc.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_shader.h"
#endif

#include "../configuration.h"
#include "../verbosity.h"
#include "../frontend/frontend_driver.h"
#include "../command.h"
#include "../list_special.h"
#include "../file_path_special.h"
#include "../paths.h"
#include "../retroarch.h"

#if defined(HAVE_GFX_WIDGETS)
#include "gfx_widgets.h"
#endif

#include "video_shader_parse.h"

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
#include "drivers_shader/slang_process.h"
#endif

/* Maximum depth of chain of referenced shader presets. 
 * 16 seems to be a very large number of references at the moment. */
#define SHADER_MAX_REFERENCE_DEPTH 16

/* TODO/FIXME - global state - perhaps move outside this file */
static path_change_data_t *file_change_data = NULL;

/**
 * fill_pathname_expanded_and_absolute:
 * @out_path              : string to write into
 * @in_refpath            : used to get the base path if in_path is relative
 * @in_path               : path to turn into an absolute path
 *
 * Takes a path and returns an absolute path, 
 * It will expand it if the path was using the root path format  
 * e.g. :\shaders
 * If the path was relative it will take this path and get the 
 * absolute path using in_refpath 
 * as the path to extract a base path
 *
 * out_path is filled with the absolute path
 **/
static void fill_pathname_expanded_and_absolute(char *out_path,
      const char *in_refpath, const char *in_path)
{
   char expanded_path[PATH_MAX_LENGTH];

   expanded_path[0] = '\0';

   /* Expand paths which start with :\ to an absolute path */
   fill_pathname_expand_special(expanded_path,
         in_path, sizeof(expanded_path));

   /* Resolve the reference path relative to the config */
   if (path_is_absolute(expanded_path))
      strlcpy(out_path, expanded_path, PATH_MAX_LENGTH);
   else
      fill_pathname_resolve_relative(out_path, in_refpath,
            in_path, PATH_MAX_LENGTH);

   pathname_conform_slashes_to_os(out_path);
}

/**
 * wrap_mode_to_str:
 * @type              : Wrap type.
 *
 * Translates wrap mode to human-readable string identifier.
 *
 * Returns: human-readable string identifier of wrap mode.
 **/
static const char *wrap_mode_to_str(enum gfx_wrap_type type)
{
   switch (type)
   {
      case RARCH_WRAP_BORDER:
         return "clamp_to_border";
      case RARCH_WRAP_EDGE:
         return "clamp_to_edge";
      case RARCH_WRAP_REPEAT:
         return "repeat";
      case RARCH_WRAP_MIRRORED_REPEAT:
         return "mirrored_repeat";
      default:
         break;
   }

   return "???";
}

/**
 * wrap_str_to_mode:
 * @type              : Wrap type in human-readable string format.
 *
 * Translates wrap mode from human-readable string to enum mode value.
 *
 * Returns: enum mode value of wrap type.
 **/
static enum gfx_wrap_type wrap_str_to_mode(const char *wrap_mode)
{
   if (string_is_equal(wrap_mode,      "clamp_to_border"))
      return RARCH_WRAP_BORDER;
   else if (string_is_equal(wrap_mode, "clamp_to_edge"))
      return RARCH_WRAP_EDGE;
   else if (string_is_equal(wrap_mode, "repeat"))
      return RARCH_WRAP_REPEAT;
   else if (string_is_equal(wrap_mode, "mirrored_repeat"))
      return RARCH_WRAP_MIRRORED_REPEAT;

   RARCH_WARN("[Shaders]:  Invalid wrapping type %s. Valid ones are: clamp_to_border"
         " (default), clamp_to_edge, repeat and mirrored_repeat. Falling back to default.\n",
         wrap_mode);
   return RARCH_WRAP_DEFAULT;
}

/**
 * video_shader_parse_pass:
 * @conf              : Preset file to read from.
 * @pass              : Shader passes handle.
 * @i                 : Index of shader pass.
 *
 * Parses shader pass from preset file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool video_shader_parse_pass(config_file_t *conf,
      struct video_shader_pass *pass, unsigned i)
{
   char shader_name[64];
   char filter_name_buf[64];
   char wrap_name_buf[64];
   char frame_count_mod_buf[64];
   char srgb_output_buf[64];
   char fp_fbo_buf[64];
   char mipmap_buf[64];
   char alias_buf[64];
   char scale_name_buf[64];
   char attr_name_buf[64];
   char scale_type[64];
   char scale_type_x[64];
   char scale_type_y[64];
   char tmp_path[PATH_MAX_LENGTH];
   struct gfx_fbo_scale *scale      = NULL;
   bool tmp_bool                    = false;
   float fattr                      = 0.0f;
   int iattr                        = 0;
   struct config_entry_list *entry  = NULL;

   fp_fbo_buf[0]      = mipmap_buf[0]          = alias_buf[0]       =
   scale_name_buf[0]  = attr_name_buf[0]       = scale_type[0]      =
   scale_type_x[0]    = scale_type_y[0]        =
   shader_name[0]     = filter_name_buf[0]     = wrap_name_buf[0]   = 
                        frame_count_mod_buf[0] = srgb_output_buf[0] = '\0';

   /* Source */
   snprintf(shader_name, sizeof(shader_name), "shader%u", i);
   if (!config_get_path(conf, shader_name, tmp_path, sizeof(tmp_path)))
   {
      RARCH_ERR("[Shaders]:  Couldn't parse shader source (%s).\n", shader_name);
      return false;
   }

   /* Get the absolute path */
   fill_pathname_expanded_and_absolute(pass->source.path,
         conf->path, tmp_path);

   /* Smooth */
   snprintf(filter_name_buf, sizeof(filter_name_buf), "filter_linear%u", i);

   if (config_get_bool(conf, filter_name_buf, &tmp_bool))
   {
      bool smooth  = tmp_bool;
      pass->filter = smooth ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
   }
   else
      pass->filter = RARCH_FILTER_UNSPEC;

   /* Wrapping mode */
   snprintf(wrap_name_buf, sizeof(wrap_name_buf), "wrap_mode%u", i);
   if ((entry = config_get_entry(conf, wrap_name_buf)) 
         && !string_is_empty(entry->value))
      pass->wrap = wrap_str_to_mode(entry->value);
   entry = NULL;

   /* Frame count mod */
   snprintf(frame_count_mod_buf, sizeof(frame_count_mod_buf),
         "frame_count_mod%u", i);
   if ((entry = config_get_entry(conf, frame_count_mod_buf)) 
         && !string_is_empty(entry->value))
      pass->frame_count_mod = (unsigned)strtoul(entry->value, NULL, 0);
   entry = NULL;

   /* FBO types and mipmapping */
   snprintf(srgb_output_buf, sizeof(srgb_output_buf), "srgb_framebuffer%u", i);
   if (config_get_bool(conf, srgb_output_buf, &tmp_bool))
      pass->fbo.srgb_fbo = tmp_bool;

   snprintf(fp_fbo_buf, sizeof(fp_fbo_buf), "float_framebuffer%u", i);
   if (config_get_bool(conf, fp_fbo_buf, &tmp_bool))
      pass->fbo.fp_fbo = tmp_bool;

   snprintf(mipmap_buf, sizeof(mipmap_buf), "mipmap_input%u", i);
   if (config_get_bool(conf, mipmap_buf, &tmp_bool))
      pass->mipmap = tmp_bool;

   snprintf(alias_buf, sizeof(alias_buf), "alias%u", i);
   if (!config_get_array(conf, alias_buf, pass->alias, sizeof(pass->alias)))
      *pass->alias = '\0';

   /* Scale */
   scale = &pass->fbo;
   snprintf(scale_name_buf, sizeof(scale_name_buf), "scale_type%u", i);
   config_get_array(conf, scale_name_buf, scale_type, sizeof(scale_type));

   snprintf(scale_name_buf, sizeof(scale_name_buf), "scale_type_x%u", i);
   config_get_array(conf, scale_name_buf, scale_type_x, sizeof(scale_type_x));

   snprintf(scale_name_buf, sizeof(scale_name_buf), "scale_type_y%u", i);
   config_get_array(conf, scale_name_buf, scale_type_y, sizeof(scale_type_y));

   if (!*scale_type && !*scale_type_x && !*scale_type_y)
      return true;

   if (*scale_type)
   {
      strlcpy(scale_type_x, scale_type, sizeof(scale_type_x));
      strlcpy(scale_type_y, scale_type, sizeof(scale_type_y));
   }

   scale->valid   = true;
   scale->type_x  = RARCH_SCALE_INPUT;
   scale->type_y  = RARCH_SCALE_INPUT;
   scale->scale_x = 1.0;
   scale->scale_y = 1.0;

   if (*scale_type_x)
   {
      if (string_is_equal(scale_type_x, "source"))
         scale->type_x = RARCH_SCALE_INPUT;
      else if (string_is_equal(scale_type_x, "viewport"))
         scale->type_x = RARCH_SCALE_VIEWPORT;
      else if (string_is_equal(scale_type_x, "absolute"))
         scale->type_x = RARCH_SCALE_ABSOLUTE;
      else
      {
         RARCH_ERR("[Shaders]:  Invalid attribute.\n");
         return false;
      }
   }

   if (*scale_type_y)
   {
      if (string_is_equal(scale_type_y, "source"))
         scale->type_y = RARCH_SCALE_INPUT;
      else if (string_is_equal(scale_type_y, "viewport"))
         scale->type_y = RARCH_SCALE_VIEWPORT;
      else if (string_is_equal(scale_type_y, "absolute"))
         scale->type_y = RARCH_SCALE_ABSOLUTE;
      else
      {
         RARCH_ERR("[Shaders]:  Invalid attribute.\n");
         return false;
      }
   }

   snprintf(attr_name_buf, sizeof(attr_name_buf), "scale%u", i);

   if (scale->type_x == RARCH_SCALE_ABSOLUTE)
   {
      if (config_get_int(conf, attr_name_buf, &iattr))
         scale->abs_x = iattr;
      else
      {
         snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_x%u", i);
         if (config_get_int(conf, attr_name_buf, &iattr))
            scale->abs_x = iattr;
      }
   }
   else
   {
      if (config_get_float(conf, attr_name_buf, &fattr))
         scale->scale_x = fattr;
      else
      {
         snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_x%u", i);
         if (config_get_float(conf, attr_name_buf, &fattr))
            scale->scale_x = fattr;
      }
   }

   snprintf(attr_name_buf, sizeof(attr_name_buf), "scale%u", i);

   if (scale->type_y == RARCH_SCALE_ABSOLUTE)
   {
      if (config_get_int(conf, attr_name_buf, &iattr))
         scale->abs_y = iattr;
      else
      {
         snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_y%u", i);
         if (config_get_int(conf, attr_name_buf, &iattr))
            scale->abs_y = iattr;
      }
   }
   else
   {
      if (config_get_float(conf, attr_name_buf, &fattr))
         scale->scale_y = fattr;
      else
      {
         snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_y%u", i);
         if (config_get_float(conf, attr_name_buf, &fattr))
            scale->scale_y = fattr;
      }
   }

   return true;
}

/**
 * video_shader_parse_textures:
 * @conf              : Preset file to read from.
 * @shader            : Shader pass handle.
 *
 * Parses shader textures.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool video_shader_parse_textures(config_file_t *conf,
      struct video_shader *shader)
{
   size_t path_size     = PATH_MAX_LENGTH;
   const char *id       = NULL;
   char *save           = NULL;
   char *textures       = (char*)malloc(1024 + path_size);
   char texture_path[PATH_MAX_LENGTH];

   if (!textures)
      return false;

   textures[0] = '\0';
   texture_path[0] = '\0';

   if (!config_get_array(conf, "textures", textures, 1024))
   {
      free(textures);
      return true;
   }

   for (id = strtok_r(textures, ";", &save);
         id && shader->luts < GFX_MAX_TEXTURES;
         shader->luts++, id = strtok_r(NULL, ";", &save))
   {
      char id_filter[64];
      char id_wrap[64];
      char id_mipmap[64];
      bool mipmap         = false;
      bool smooth         = false;
      struct config_entry_list 
         *entry           = NULL;

      id_filter[0] = id_wrap[0] = id_mipmap[0] = '\0';

      if (!(entry = config_get_entry(conf, id)) ||
            string_is_empty(entry->value))
      {
         RARCH_ERR("[Shaders]:  Cannot find path to texture \"%s\" ...\n",
               id);
         free(textures);
         return false;
      }

      config_get_path(conf, id, texture_path, sizeof(texture_path));

      /* Get the absolute path */
      fill_pathname_expanded_and_absolute(
            shader->lut[shader->luts].path, conf->path, texture_path);

      entry = NULL;

      strlcpy(shader->lut[shader->luts].id, id,
            sizeof(shader->lut[shader->luts].id));

      strlcpy(id_filter, id, sizeof(id_filter));
      strlcat(id_filter, "_linear", sizeof(id_filter));
      if (config_get_bool(conf, id_filter, &smooth))
         shader->lut[shader->luts].filter = smooth 
            ? RARCH_FILTER_LINEAR 
            : RARCH_FILTER_NEAREST;
      else
         shader->lut[shader->luts].filter = RARCH_FILTER_UNSPEC;

      strlcpy(id_wrap, id, sizeof(id_wrap));
      strlcat(id_wrap, "_wrap_mode", sizeof(id_wrap));
      if ((entry = config_get_entry(conf, id_wrap)) 
            && !string_is_empty(entry->value))
         shader->lut[shader->luts].wrap = wrap_str_to_mode(entry->value);
      entry = NULL;

      strlcpy(id_mipmap, id, sizeof(id_mipmap));
      strlcat(id_mipmap, "_mipmap", sizeof(id_mipmap));
      if (config_get_bool(conf, id_mipmap, &mipmap))
         shader->lut[shader->luts].mipmap = mipmap;
      else
         shader->lut[shader->luts].mipmap = false;
   }

   free(textures);
   return true;
}

/**
 * video_shader_parse_find_parameter:
 * @params            : Shader parameter handle.
 * @num_params        : Number of shader params in @params.
 * @id                : Identifier to search for.
 *
 * Finds a shader parameter with identifier @id in @params..
 *
 * Returns: handle to shader parameter if successful, otherwise NULL.
 **/
static struct video_shader_parameter *video_shader_parse_find_parameter(
      struct video_shader_parameter *params,
      unsigned num_params, const char *id)
{
   unsigned i;

   for (i = 0; i < num_params; i++)
   {
      if (string_is_equal(params[i].id, id))
         return &params[i];
   }

   return NULL;
}

/**
 * video_shader_resolve_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Resolves all shader parameters belonging to shaders
 * from the #pragma parameter lines in the shader for each pass.
 * 
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_resolve_parameters(struct video_shader *shader)
{
   unsigned i;
   struct video_shader_parameter *param = &shader->parameters[0];

   shader->num_parameters = 0;

   /* Find all parameters in our shaders. */

   RARCH_DBG("[Shaders]:  Finding Parameters in Shader Passes (#pragma parameter)\n");

   for (i = 0; i < shader->passes; i++)
   {
      const char *path          = shader->pass[i].source.path;
      uint8_t *buf              = NULL;
      int64_t buf_len           = 0;

      if (string_is_empty(path))
         continue;

      if (!path_is_valid(path))
         continue;

      /* First try to use the more robust slang implementation 
       * to support #includes. */

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      /* FIXME: The check for slang can be removed
       * if it's sufficiently tested for GLSL/Cg as well, 
       * it should be the same implementation. 
       * The problem with switching currently is that it looks 
       * for a #version string in the first line of the file 
       * which glsl doesn't have */

      if (     string_is_equal(path_get_extension(path), "slang") 
            && slang_preprocess_parse_parameters(path, shader))
         continue;
#endif

      /* Read file contents */
      if (filestream_read_file(path, (void**)&buf, &buf_len))
      {
         size_t line_index         = 0;
         struct string_list lines  = {0};
         bool lines_inited         = false;

         /* Split into lines */
         if (buf_len > 0)
         {
            string_list_initialize(&lines);
            lines_inited = string_split_noalloc(&lines, (const char*)buf, "\n");
         }

         /* Buffer is no longer required - clean up */
         if ((void*)buf)
            free((void*)buf);

         if (!lines_inited)
            continue;

         /* Even though the pass is set in the loop too, 
          * not all passes have parameters */
         param->pass = i;

         while ((shader->num_parameters < ARRAY_SIZE(shader->parameters)) 
               && (line_index < lines.size))
         {
            int ret;
            const char *line = lines.elems[line_index].data;
            line_index++;

            /* Check if this is a '#pragma parameter' line */
            if (strncmp("#pragma parameter", line,
                     STRLEN_CONST("#pragma parameter")))
               continue;

            /* Parse line */
            ret = sscanf(line, "#pragma parameter %63s \"%63[^\"]\" %f %f %f %f",
                  param->id,        param->desc,    &param->initial,
                  &param->minimum, &param->maximum, &param->step);

            if (ret < 5)
               continue;

            param->id[63]   = '\0';
            param->desc[63] = '\0';

            if (ret == 5)
               param->step  = 0.1f * (param->maximum - param->minimum);

            param->pass     = i;

            RARCH_DBG("[Shaders]:     Found #pragma parameter %s (%s) %f %f %f %f in pass %d\n",
                  param->desc,    param->id,      param->initial,
                  param->minimum, param->maximum, param->step, param->pass);
            param->current  = param->initial;

            shader->num_parameters++;
            param++;
         }

         string_list_deinitialize(&lines);
      }
   }

   return true;
}


/**
 * video_shader_load_current_parameter_values:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * For each parameter in the shader, if a value is set in the config file
 * load this value to the parameter's current value.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_load_current_parameter_values(
      config_file_t *conf, struct video_shader *shader)
{
   unsigned i;
   bool load_parameter_message_shown      = false;
   const struct config_entry_list *entry  = NULL;

   if (!conf)
   {
      RARCH_ERR("[Shaders]: Load Parameter Values - Config is Null.\n");
      return false;
   }

   /* For all parameters in the shader see if there is any config value set */
   for (i = 0; i < shader->num_parameters; i++)
   {
      entry = config_get_entry(conf, shader->parameters[i].id);
      
      /* Only try to load the parameter value if an entry exists in the config */
      if (entry)
      {
         struct video_shader_parameter *parameter = (struct video_shader_parameter*)
                                                   video_shader_parse_find_parameter(shader->parameters, 
                                                                                    shader->num_parameters, 
                                                                                    shader->parameters[i].id);
         /* Log the message for loading parameter values only once*/
         if (!load_parameter_message_shown)
         {
            RARCH_DBG("[Shaders]:    Loading base parameter values\n");
            load_parameter_message_shown = true;
         }

         /* Log each parameter read */
         if (config_get_float(conf, shader->parameters[i].id, &parameter->current))
            RARCH_DBG("[Shaders]:      Load parameter value:   %s = %f.\n", shader->parameters[i].id, parameter->current);
         else
            RARCH_WARN("[Shaders]:      Load parameter value: name %s is set in preset but couldn't load its value.\n", 
                        shader->parameters[i].id);
      }
   }

   return true;
}

static const char *scale_type_to_str(enum gfx_scale_type type)
{
   switch (type)
   {
      case RARCH_SCALE_INPUT:
         return "source";
      case RARCH_SCALE_VIEWPORT:
         return "viewport";
      case RARCH_SCALE_ABSOLUTE:
         return "absolute";
      default:
         break;
   }

   return "?";
}

static void shader_write_scale_dim(config_file_t *conf,
      const char *dim,
      enum gfx_scale_type type, 
      float scale,
      unsigned absolute, 
      unsigned i)
{
   char key[64];

   key[0] = '\0';

   snprintf(key, sizeof(key), "scale_type_%s%u", dim, i);
   config_set_string(conf, key, scale_type_to_str(type));

   snprintf(key, sizeof(key), "scale_%s%u", dim, i);
   if (type == RARCH_SCALE_ABSOLUTE)
      config_set_int(conf, key, absolute);
   else
      config_set_float(conf, key, scale);
}

static void shader_write_fbo(config_file_t *conf,
      const struct gfx_fbo_scale *fbo, unsigned i)
{
   char key[64];

   key[0] = '\0';

   snprintf(key, sizeof(key), "float_framebuffer%u", i);
   config_set_bool(conf, key, fbo->fp_fbo);
   snprintf(key, sizeof(key), "srgb_framebuffer%u", i);
   config_set_bool(conf, key, fbo->srgb_fbo);

   if (!fbo->valid)
      return;

   shader_write_scale_dim(conf, "x", fbo->type_x, fbo->scale_x, fbo->abs_x, i);
   shader_write_scale_dim(conf, "y", fbo->type_y, fbo->scale_y, fbo->abs_y, i);
}

/**
 * video_shader_write_root_preset:
 * @conf              : Preset file to write to.
 * @shader            : Shader passes handle.
 * @preset_path       : Optional path to where the preset will be written.
 *
 * Writes preset and all associated state (passes, textures, imports, 
 * etc) into @conf.
 * If @preset_path is not NULL, shader paths are saved relative to it.
 **/
static bool video_shader_write_root_preset(const struct video_shader *shader,
      const char *path)
{
   bool ret = true;
   unsigned i;
   char key[64];
   size_t tmp_size      = PATH_MAX_LENGTH;
   char *tmp            = (char*)malloc(3*tmp_size);
   char *tmp_rel        = tmp +   tmp_size;
   char *tmp_base       = tmp + 2*tmp_size;
   config_file_t *conf  = NULL;

   if (!(conf = config_file_new_alloc()))
   {
      ret = false;
      goto end;
   }

   if (!tmp)
   {
      ret = false;
      goto end;
   }

   RARCH_DBG("[Shaders]:  Saving FULL PRESET to: %s\n", path);

   config_set_int(conf, "shaders", shader->passes);
   if (shader->feedback_pass >= 0)
      config_set_int(conf, "feedback_pass", shader->feedback_pass);

   strlcpy(tmp_base, path, tmp_size);

   /* ensure we use a clean base like the shader passes and texture paths do */
   path_resolve_realpath(tmp_base, tmp_size, false);
   path_basedir(tmp_base);

   for (i = 0; i < shader->passes; i++)
   {
      const struct video_shader_pass *pass = &shader->pass[i];

      snprintf(key, sizeof(key), "shader%u", i);

      strlcpy(tmp, pass->source.path, tmp_size);
      path_relative_to(tmp_rel, tmp, tmp_base, tmp_size);

      pathname_make_slashes_portable(tmp_rel);

      config_set_path(conf, key, tmp_rel);


      if (pass->filter != RARCH_FILTER_UNSPEC)
      {
         snprintf(key, sizeof(key), "filter_linear%u", i);
         config_set_bool(conf, key, pass->filter == RARCH_FILTER_LINEAR);
      }

      snprintf(key, sizeof(key), "wrap_mode%u", i);
      config_set_string(conf, key, wrap_mode_to_str(pass->wrap));

      if (pass->frame_count_mod)
      {
         snprintf(key, sizeof(key), "frame_count_mod%u", i);
         config_set_int(conf, key, pass->frame_count_mod);
      }

      snprintf(key, sizeof(key), "mipmap_input%u", i);
      config_set_bool(conf, key, pass->mipmap);

      snprintf(key, sizeof(key), "alias%u", i);
      config_set_string(conf, key, pass->alias);

      shader_write_fbo(conf, &pass->fbo, i);
   }

   /* Write shader parameters which are different than the default shader values */
   if (shader->num_parameters)
      for (i = 0; i < shader->num_parameters; i++)
         if (shader->parameters[i].current != shader->parameters[i].initial)
            config_set_float(conf, shader->parameters[i].id, shader->parameters[i].current);

   if (shader->luts)
   {
      char textures[4096];

      textures[0] = '\0';

      /* Names of the textures */
      strlcpy(textures, shader->lut[0].id, sizeof(textures));

      for (i = 1; i < shader->luts; i++)
      {
         /* O(n^2), but number of textures is very limited. */
         strlcat(textures, ";", sizeof(textures));
         strlcat(textures, shader->lut[i].id, sizeof(textures));
      }

      config_set_string(conf, "textures", textures);

      /* Step through the textures in the shader */
      for (i = 0; i < shader->luts; i++)
      {
         fill_pathname_abbreviated_or_relative(tmp_rel,
               tmp_base, shader->lut[i].path, PATH_MAX_LENGTH);
         pathname_make_slashes_portable(tmp_rel);
         config_set_string(conf, shader->lut[i].id, tmp_rel);

         /* Linear filter ON or OFF */
         if (shader->lut[i].filter != RARCH_FILTER_UNSPEC)
         {
            char key[128];
            key[0]  = '\0';
            strlcpy(key, shader->lut[i].id, sizeof(key));
            strlcat(key, "_linear", sizeof(key));
            config_set_bool(conf, key, 
                  shader->lut[i].filter == RARCH_FILTER_LINEAR);
         }

         /* Wrap Mode */
         {
            char key[128];
            key[0]  = '\0';
            strlcpy(key, shader->lut[i].id, sizeof(key));
            strlcat(key, "_wrap_mode", sizeof(key));
            config_set_string(conf, key,
                  wrap_mode_to_str(shader->lut[i].wrap));
         }

         /* Mipmap On or Off */
         {
            char key[128];
            key[0]  = '\0';
            strlcpy(key, shader->lut[i].id, sizeof(key));
            strlcat(key, "_mipmap", sizeof(key));
            config_set_bool(conf, key, shader->lut[i].mipmap);
         }
      }
   }

   /* Write the File! */
   ret = config_file_write(conf, path, false);

end:
   config_file_free(conf);
   free(tmp);

   return ret;
}

static config_file_t *video_shader_get_root_preset_config(const char *path)
{
   int reference_depth           = 1;
   config_file_t *conf           = config_file_new_from_path_to_string(path);
   char* nested_reference_path   = (char*)malloc(PATH_MAX_LENGTH);

   if (!conf)
      goto end;

   while (conf->reference)
   {
      /* If we have reached the max depth of nested references,
       * stop attempting to read the next reference,
       * because we are likely in a self referential loop. 
       *
       * SHADER_MAX_REFERENCE_DEPTH references deep seems 
       * like more than enough depth for expected usage */
      if (reference_depth > SHADER_MAX_REFERENCE_DEPTH)
      {
         RARCH_ERR("[Shaders] - Get Root Preset - Exceeded maximum reference depth(%u) without finding a full preset. "
               "This chain of referenced presets is likely cyclical.\n", SHADER_MAX_REFERENCE_DEPTH);
         config_file_free(conf);
         conf = NULL;
         goto end;
      }

      /* Get the absolute path for the reference */
      fill_pathname_expanded_and_absolute(nested_reference_path, conf->path, conf->reference);

      /* Create a new config from the referenced path */
      config_file_free(conf);
      conf = config_file_new_from_path_to_string(nested_reference_path);

      /* If we can't read the reference preset */
      if (!conf)
      {
         RARCH_WARN("[Shaders]:  Could not read shader preset in #reference line: %s\n", nested_reference_path);
         goto end;
      }

      reference_depth += 1;
   }

end:
   free(nested_reference_path);
   return conf;
}

/**
 * video_shader_check_reference_chain:
 * @path_to_save              : Path of the preset we want to validate is safe to save 
 *                              as a simple preset
 * @reference_path            : Path of the reference which we would want to write into 
 *                              the new preset
 * 
 * Checks to see if we can save a valid simple preset (preset with a #reference in it) 
 * to this path
 * 
 * This takes into account reference links which can't be loaded and if saving 
 * this file would create a creating circular reference chain because some link in 
 * the chain references the file path we want to save to
 * 
 * Checks each preset in the chain of presets with #reference
 * Starts with reference_path, If it has no reference then our check is valid
 * If it has a #reference then check that the reference path is not the same as path_to_save
 * If it is not the same path then go the the next nested reference
 * 
 * Continues this until it finds a preset without #reference in it, 
 * or it hits the maximum recursion depth (at that point
 * it is probably in a self referential cycle)
 * 
 * Returns: true (1) if it was able to load all presets and found a full preset
 *          otherwise false (0).
 **/
static bool video_shader_check_reference_chain_for_save(
      const char *path_to_save, const char *reference_path)
{
   config_file_t *conf           = config_file_new_from_path_to_string(
         reference_path);
   char* nested_reference_path   = (char*)malloc(PATH_MAX_LENGTH);
   char* path_to_save_conformed  = (char*)malloc(PATH_MAX_LENGTH);
   bool return_val               = true;

   strlcpy(path_to_save_conformed, path_to_save, PATH_MAX_LENGTH);
   pathname_conform_slashes_to_os(path_to_save_conformed);

   if (!conf)
   {
      RARCH_ERR("[Shaders]:  Could not read the #reference preset: %s\n", reference_path);
      return_val = false;
   }
   else
   {
      int reference_depth = 1;

      while (conf->reference)
      {
         /* If we have reached the max depth of nested references stop attempting to read 
          * the next reference because we are likely in a self referential loop. */
         if (reference_depth > SHADER_MAX_REFERENCE_DEPTH)
         {
            RARCH_ERR("[Shaders] - Check Reference Chain for Save - Exceeded maximum reference depth(%u) without "
                      "finding a full preset. This chain of referenced presets is likely cyclical.\n", SHADER_MAX_REFERENCE_DEPTH);
            return_val = false;
            break;
         }

         /* Get the absolute path for the reference */
         fill_pathname_expanded_and_absolute(nested_reference_path, conf->path, conf->reference);

         /* If one of the reference paths is the same as the file we want to save then this reference chain would be 
          * self-referential / cyclical and we can't save this as a simple preset*/
         if (string_is_equal(nested_reference_path, path_to_save_conformed))
         {
            RARCH_WARN("[Shaders]:  Saving preset:\n"
                       "                                              %s\n"
                       "                                          With a #reference of:\n"
                       "                                              %s\n"
                       "                                          Would create a cyclical reference in preset:\n"
                       "                                              %s\n"
                       "                                          Which already references preset:\n"
                       "                                              %s\n\n",
                       path_to_save_conformed, reference_path, conf->path, nested_reference_path);
            return_val = false;
            break;
         }

         /* Create a new config from the referenced path */
         config_file_free(conf);
         conf = config_file_new_from_path_to_string(nested_reference_path);

         /* If we can't read the reference preset */
         if (!conf)
         {
            RARCH_WARN("[Shaders]:  Could not read shader preset"
                  " in #reference line: %s\n", nested_reference_path);
            return_val = false;
            break;
         }

         reference_depth += 1;
      }
   }


   free(path_to_save_conformed);
   free(nested_reference_path);
   config_file_free(conf);

   return return_val;
}

/**
 * video_shader_write_referenced_preset:
 * @path              : File to write to
 * @shader            : Shader preset to write
 *
 * Writes a referenced preset to disk
 *    A referenced preset is a preset which includes the #reference directive
 *    as it's first line to specify a root preset and can also 
 *    include parameter and texture values to override the values 
 *    of the root preset
 *
 * Returns false if a referenced preset cannot be saved
 **/
static bool video_shader_write_referenced_preset(
      const char *path_to_save,
      const char *shader_dir,
      const struct video_shader *shader)
{
   unsigned i;
   config_file_t *conf                    = NULL;
   config_file_t *reference_conf          = NULL;
   struct video_shader *referenced_shader = (struct video_shader*)
      calloc(1, sizeof(*referenced_shader));
   bool ret                               = false;
   bool continue_saving_reference         = true;
   char *new_preset_basedir               = strdup(path_to_save);
   char *config_dir                       = (char*)malloc(PATH_MAX_LENGTH);
   char *relative_temp_reference_path     = (char*)malloc(PATH_MAX_LENGTH);
   char *abs_temp_reference_path          = (char*)malloc(PATH_MAX_LENGTH);
   char *path_to_reference                = (char*)malloc(PATH_MAX_LENGTH);
   char* path_to_save_conformed           = (char*)malloc(PATH_MAX_LENGTH);

   strlcpy(path_to_save_conformed, path_to_save, PATH_MAX_LENGTH);
   pathname_conform_slashes_to_os(path_to_save_conformed);

   config_dir[0]                          = '\0';
   relative_temp_reference_path[0]        = '\0';
   abs_temp_reference_path[0]             = '\0';
   path_to_reference[0]                   = '\0';

   path_basedir(new_preset_basedir);

   /* Get the retroarch config dir where the automatically 
    * loaded presets are located
    * and where Save Game Preset, Save Core Preset, 
    * Save Global Preset save to */
   fill_pathname_application_special(config_dir, PATH_MAX_LENGTH,
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* If there is no initial preset path loaded */
   if (string_is_empty(shader->loaded_preset_path))
   {
      RARCH_WARN("[Shaders]: Saving Full Preset because the loaded Shader"
            "does not have "
            "a path to a previously loaded preset file on disk.\n");
      goto end;
   }
   
   /* If the initial preset loaded is the ever-changing retroarch 
    * preset don't save a reference
    * TODO/FIXME - remove once we don't write this preset anymore */
   if (!strncmp(path_basename_nocompression(shader->loaded_preset_path),
            "retroarch",
            STRLEN_CONST("retroarch")))
   {
      RARCH_WARN("[Shaders]: Saving Full Preset because we can't save"
            " a reference to the "
            "ever-changing retroarch preset.\n");
      goto end;
   }

   strlcpy(path_to_reference, shader->loaded_preset_path, PATH_MAX_LENGTH);
   pathname_conform_slashes_to_os(path_to_reference);

   /* Get a config from the file we want to make a reference to */
   reference_conf = config_file_new_from_path_to_string(path_to_reference);

   /* If the original preset can't be loaded, probably because 
    * it isn't there anymore */
   if (!reference_conf)
   {
      RARCH_WARN("[Shaders]: Saving Full Preset because the initially"
            " loaded preset can't be loaded. "
            "It was likely renamed or deleted.\n");
      goto end;
   }

   /* If we are trying to save on top the path referenced in the 
    * initially loaded preset.
    *
    * E.G. Preset_B references Preset_A, I load Preset_B do some 
    * parameter adjustments, 
    * then I save on top of Preset_A, we want to get a preset 
    * just like the original Preset_A with the new parameter 
    * adjustments.
    *
    * If there is a reference in the initially loaded preset,
    * we should check it against the preset path we are currently 
    * trying to save */
   if (reference_conf->reference)
   {
      /* Get the absolute path for the reference */
      fill_pathname_expanded_and_absolute(abs_temp_reference_path,
            reference_conf->path, reference_conf->reference);

      pathname_conform_slashes_to_os(abs_temp_reference_path);

      /* If the reference is the same as the path we are trying to save to 
         then this should be used as the reference to save */
      if (string_is_equal(abs_temp_reference_path, path_to_save_conformed))
      {
         strlcpy(path_to_reference, abs_temp_reference_path,
               PATH_MAX_LENGTH);
         config_file_free(reference_conf);
         reference_conf = config_file_new_from_path_to_string(
               path_to_reference);
      }
   }

   /* 
    * If 
    *    The new preset file we are trying to save is the 
    *    same as the initially loaded preset
    * or
    *    The initially loaded preset was located under the 
    *    retroarch config folder
    *    this means that it was likely saved from inside the retroarch UI
    * Then
    *    We should not save a preset with a reference to the initially loaded
    *    preset file itself, instead we need to save a new preset with 
    *    the same reference as was in the initially loaded preset.
    */

   /* If the reference path is the same as the path we want to save 
    * or the reference path is in the config (auto shader) folder */
   if (      string_is_equal(path_to_reference, path_to_save_conformed) 
         || !strncmp(config_dir, path_to_reference, strlen(config_dir)))
   {
      /* If the config from the reference path has a reference in it,
       * we will use this same nested reference for the new preset */
      if (reference_conf->reference)
      {
         /* Get the absolute path for the reference */
         fill_pathname_expanded_and_absolute(path_to_reference,
               reference_conf->path, reference_conf->reference);

         /* If the reference path is also the same as what 
          * we are trying to save 
            This can easily happen
            E.G.
            - Save Preset As
            - Save Game Preset
            - Save Preset As (use same name as first time)
         */
         if (string_is_equal(path_to_reference, path_to_save_conformed))
         {
            config_file_free(reference_conf);
            reference_conf = config_file_new_from_path_to_string(
                  path_to_reference);

            /* If the reference also has a reference inside it */
            if (reference_conf->reference)
            {
               /* Get the absolute path for the reference */
               fill_pathname_expanded_and_absolute(path_to_reference,
                     reference_conf->path, reference_conf->reference);
            }
            /* If the config referenced is a full preset */
            else
            {
               RARCH_WARN("[Shaders]: Saving Full Preset because we can't"
                     " save a preset which "
                     "would reference itself.\n");
               goto end;
            }
         }
      }
      /* If there is no reference in the initial preset we need to 
       * save a full preset */
      else
      {
         /* We can't save a reference to ourselves */
         RARCH_WARN("[Shaders]: Saving Full Preset because we can't save"
               " a preset which "
               "would reference itself.\n");
         goto end;
      }
   }

   /* Check the reference chain that we would be saving to make sure it 
    * is valid */
   if (!video_shader_check_reference_chain_for_save(
            path_to_save_conformed, path_to_reference))
   {
      RARCH_WARN("[Shaders]: Saving Full Preset because saving a"
            " Simple Preset would result "
            "in a cyclical reference, or a preset in the reference"
            " chain could not be read.\n");
      goto end;
   }

   RARCH_DBG("[Shaders]:  Reading Preset to Compare with"
         " Current Values: %s\n", path_to_save_conformed);

   /* Load the preset referenced in the preset into the shader */
   if (!video_shader_load_preset_into_shader(path_to_reference,
            referenced_shader))
   {
      RARCH_WARN("[Shaders]:  Saving Full Preset because we could"
            " not load the preset from the #reference line: %s.\n", 
            path_to_reference);
      goto end;
   }

   /* Create a new EMPTY config */
   conf = config_file_new_alloc();

   if (!(conf))
      goto end;

   conf->path = strdup(path_to_save_conformed);

   pathname_make_slashes_portable(relative_temp_reference_path);

   /* Add the reference path to the config */
   config_file_set_reference_path(conf, path_to_reference);

   /* Set modified to true so when you run config_file_write 
    * it will save a file */
   conf->modified = true;

   /* 
      Compare the shader to a shader created from the referenced 
      config to see if we can save a referenced preset and what 
      parameters and textures of the root_config are overridden
   */

   /* Check number of passes match */
   if (shader->passes != referenced_shader->passes)
   {
      RARCH_WARN("[Shaders]: passes (Number of Passes) "
                  "Current Value doesn't match Referenced Value"
                  " - Full Preset will be Saved instead of Simple Preset\n");
      continue_saving_reference = false;
   }

   /* Compare all passes from the shader, if anything is different 
    * then we should not save a reference and instead save a 
    * full preset instead.
   */
   if (continue_saving_reference)
   {
      /* Step through each pass comparing all the properties to 
       * make sure they match */
      for (i = 0; (i < shader->passes && continue_saving_reference == true);
            i++)
      {
         const struct video_shader_pass *pass      = &shader->pass[i];
         const struct video_shader_pass *root_pass = &referenced_shader->pass[i];
         const struct gfx_fbo_scale *fbo           = &pass->fbo;
         const struct gfx_fbo_scale *root_fbo      = &root_pass->fbo;

         if (!string_is_equal(pass->source.path, root_pass->source.path))
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u path", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && pass->filter != root_pass->filter)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u filter", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && pass->wrap != root_pass->wrap)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u wrap", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && pass->frame_count_mod != root_pass->frame_count_mod)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u frame_count", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && pass->mipmap != root_pass->mipmap)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u mipmap", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && !string_is_equal(pass->alias, root_pass->alias))
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u alias", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->type_x != root_fbo->type_x)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u type_x", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->type_y != root_fbo->type_y)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u type_y", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->scale_x != root_fbo->scale_x)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u scale_x", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->scale_y != root_fbo->scale_y)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u scale_y", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->fp_fbo != root_fbo->fp_fbo)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u fp_fbo", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->srgb_fbo != root_fbo->srgb_fbo)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u srgb_fbo", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->valid != root_fbo->valid)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u valid", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->abs_x != root_fbo->abs_x)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u abs_x", i);
#endif
            continue_saving_reference = false;
         }

         if (continue_saving_reference && fbo->abs_y != root_fbo->abs_y)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u abs_y", i);
#endif
            continue_saving_reference = false;
         }

         if (!continue_saving_reference)
         {
#ifdef DEBUG
            RARCH_WARN(" Current Value doesn't match Referenced Value -"
                  " Full Preset Will be Saved instead of Simple Preset\n");
#endif
            goto end;
         }
      }
   }

   /* If the shader has parameters */
   if (shader->num_parameters)
   {
      for (i = 0; i < shader->num_parameters; i++)
      {
         /* If the parameter's current value is different 
          * than the referenced shader then write the value 
          * into the new preset */
         if (     shader->parameters[i].current 
               != referenced_shader->parameters[i].current)
            config_set_float(conf, shader->parameters[i].id,
                  shader->parameters[i].current);
      }
   }

   /* If the shader has textures */
   if (shader->luts)
   {
      for (i = 0; i < shader->luts; i++)
      {
         /* If the current shader texture path is different 
          * than the referenced shader texture then write the 
          * current path into the new preset */
         if (!string_is_equal(referenced_shader->lut[i].path,
                  shader->lut[i].path))
         {
            char *path_for_save  = (char*)malloc(PATH_MAX_LENGTH);

            fill_pathname_abbreviated_or_relative(path_for_save,
                  conf->path, shader->lut[i].path, PATH_MAX_LENGTH);
            pathname_make_slashes_portable(path_for_save);
            config_set_string(conf, shader->lut[i].id, path_for_save);
#ifdef DEBUG
            RARCH_DBG("[Shaders]:  Texture override %s = %s.\n",
                  shader->lut[i].id, path_for_save);
#endif

            free(path_for_save);
         }
      }
   }

   /* Write the file, return will be true if successful */
   RARCH_DBG("[Shaders]:  Saving simple preset to: %s\n",
         path_to_save_conformed);
   ret = config_file_write(conf, path_to_save_conformed, false);

end:

   config_file_free(conf);
   config_file_free(reference_conf);
   free(referenced_shader);
   free(abs_temp_reference_path);
   free(relative_temp_reference_path);
   free(new_preset_basedir);
   free(config_dir);
   free(path_to_reference);
   free(path_to_save_conformed);

   return ret;
}

/**
 * video_shader_load_root_config_into_shader:
 * @conf              : Preset file to read from.
 * @shader            : Shader handle.
 *
 * Loads preset file and all associated state (passes, textures, imports, etc).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool video_shader_load_root_config_into_shader(
      config_file_t *conf, 
      settings_t *settings,
      struct video_shader *shader)
{
   unsigned i;
   unsigned num_passes              = 0;
   bool watch_files                 = settings->bools.video_shader_watch_files;

   /* This sets the shader to empty */
   memset(shader, 0, sizeof(*shader));

   if (!config_get_uint(conf, "shaders", &num_passes))
      return false;
   if (!num_passes)
      return false;

   if (!config_get_int(conf, "feedback_pass", &shader->feedback_pass))
      shader->feedback_pass = -1;

   shader->passes = MIN(num_passes, GFX_MAX_SHADERS);

   /* Set the path of the root preset for this shader */
   strlcpy(shader->path, conf->path, sizeof(shader->path));

   /* Set the path of the original preset which was loaded, for 
    * a full preset config this is the same as the root config 
    * For simple presets (using #reference) this different than 
    * the root preset and it is the path to the 
    * simple preset originally loaded, but that is set inside 
    * video_shader_load_preset_into_shader*/
   strlcpy( shader->loaded_preset_path, 
            conf->path,
            sizeof(shader->loaded_preset_path));

   if (watch_files)
   {
      union string_list_elem_attr attr;
      int flags                        = 
         PATH_CHANGE_TYPE_MODIFIED                   |
         PATH_CHANGE_TYPE_WRITE_FILE_CLOSED          |
         PATH_CHANGE_TYPE_FILE_MOVED                 |
         PATH_CHANGE_TYPE_FILE_DELETED;
      struct string_list file_list     = {0};

      attr.i         = 0;

      if (file_change_data)
         frontend_driver_watch_path_for_changes(NULL, 0, &file_change_data);

      file_change_data = NULL;
      string_list_initialize(&file_list);
      string_list_append(&file_list, conf->path, attr);

      /* TODO We aren't currently watching the originally loaded preset
       * We should probably watch it for changes too */

      for (i = 0; i < shader->passes; i++)
      {
         if (!video_shader_parse_pass(conf, &shader->pass[i], i))
         {
            string_list_deinitialize(&file_list);
            return false;
         }

         string_list_append(&file_list, shader->pass[i].source.path, attr);
      }

      frontend_driver_watch_path_for_changes(&file_list, flags,
            &file_change_data);
      string_list_deinitialize(&file_list);
   }
   else
   {
      for (i = 0; i < shader->passes; i++)
      {
         if (!video_shader_parse_pass(conf, &shader->pass[i], i))
            return false;
      }
   }

   if (!video_shader_parse_textures(conf, shader))
      return false;

   /* Load the parameter values */
   video_shader_resolve_parameters(shader);

   /* Load the parameter values */
   video_shader_load_current_parameter_values(conf, shader);

#ifdef DEBUG
   RARCH_DBG("[Shaders]:      Number of Passes:  %u\n", shader->passes);
   RARCH_DBG("[Shaders]:      Number of Textures:  %u\n", shader->luts);

   /* Log Texture Names & Paths */
   for (i = 0; i < shader->luts; i++)
      RARCH_DBG("[Shaders]:        %s = %s.\n", shader->lut[i].id,
            shader->lut[i].path);
#endif

   return true;
}

/**
 * override_shader_values:
 * @override_conf     : Config file who's values will be copied on top of conf
 * @shader            : Shader to be affected
 *
 * Takes values from override_config and overrides values of the shader
 *
 * Returns 0 if nothing is overridden 
 * Returns 1 if something is overridden
 **/
static bool override_shader_values(config_file_t *override_conf,
      struct video_shader *shader)
{
   unsigned i;
   bool return_val                     = false;
   struct config_entry_list *entry     = NULL;

   if (!shader || !override_conf) 
      return 0;

   /* If the shader has parameters */
   if (shader->num_parameters)
   {
      /* Step through the parameters in the shader and 
       * see if there is an entry for each in the override config */
      for (i = 0; i < shader->num_parameters; i++)
      {
         entry = config_get_entry(override_conf, shader->parameters[i].id);

         /* If the parameter is in the reference config */
         if (entry)
         {
            struct video_shader_parameter *parameter = 
               (struct video_shader_parameter*)
               video_shader_parse_find_parameter(
                     shader->parameters, 
                     shader->num_parameters, 
                     shader->parameters[i].id);

            /* Set the shader's parameter value */
            config_get_float(override_conf, shader->parameters[i].id,
                  &parameter->current);

#ifdef DEBUG
            RARCH_DBG("[Shaders]:      Parameter:  %s = %f.\n",
                  shader->parameters[i].id, 
                  shader->parameters[i].current);
#endif

            return_val = true;
         }
      }
   }

   /* ---------------------------------------------------------------------------------
    * ------------- Resolve Override texture paths to absolute paths-------------------
    * --------------------------------------------------------------------------------- */

   /* If the shader has textures */
   if (shader->luts)
   {
      char *override_tex_path             = (char*)malloc(PATH_MAX_LENGTH);

      override_tex_path[0]                = '\0';

      /* Step through the textures in the shader and see if there is an entry 
       * for each in the override config */
      for (i = 0; i < shader->luts; i++)
      {
         entry = config_get_entry(override_conf, shader->lut[i].id);

         /* If the texture is defined in the reference config */
         if (entry)
         {
            /* Texture path from shader the config */
            config_get_path(override_conf, shader->lut[i].id,
                  override_tex_path, PATH_MAX_LENGTH);

            /* Get the absolute path */
            fill_pathname_expanded_and_absolute(shader->lut[i].path,
                  override_conf->path, override_tex_path);

#ifdef DEBUG
            RARCH_DBG("[Shaders]:      Texture:    %s = %s.\n", 
                        shader->lut[i].id, 
                        shader->lut[i].path);
#endif

            return_val = true;
         }
      }

      free(override_tex_path);
   }

   return return_val;
}

/**
 * video_shader_write_preset:
 * @path              : File to write to
 * @shader            : Shader to write
 * @reference         : Whether a simple preset should be written 
 * with the #reference to another preset in it
 *
 * Writes a preset to disk. Can be written as a simple preset 
 * (With the #reference directive in it) or a full preset.
 **/
bool video_shader_write_preset(const char *path,
      const char *shader_dir,
      const struct video_shader *shader, 
      bool reference)
{
   /* We need to clean up paths to be able to properly process them
    * path and shader->loaded_preset_path can use '/' on 
    * Windows due to Qt being Qt */
   char preset_dir[PATH_MAX_LENGTH];

   if (!shader || string_is_empty(path))
      return false;

   fill_pathname_join(preset_dir, shader_dir, "presets", sizeof(preset_dir));

   /* If we should still save a referenced preset do it now */
   if (reference)
   {
      if (video_shader_write_referenced_preset(path, shader_dir, shader))
         return true;

      RARCH_WARN("[Shaders]:  Failed writing Simple Preset to %s - "
            "Full Preset Will be Saved instead.\n", path);
   }

   /* If we aren't saving a referenced preset or weren't able to save one
    * then save a full preset */
   if (path)
      return video_shader_write_root_preset(shader, path);
   return false;
}


/**
 * video_shader_load_preset_into_shader:
 * @path              : Path to preset file, could be a 
 * Simple Preset (including a #reference) or Full Preset
 * @shader            : Shader
 *
 * Loads preset file to a shader including passes, textures 
 * and parameters
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_load_preset_into_shader(const char *path,
      struct video_shader *shader)
{
   unsigned i                                        = 0;
   bool ret                                          = true;
   char override_conf_paths[SHADER_MAX_REFERENCE_DEPTH][PATH_MAX_LENGTH];
   config_file_t *conf                               = NULL;
   /* Get the root config, If we were able to get a root_config 
    * that means the reference chain is valid */
   config_file_t *root_conf                          = 
      video_shader_get_root_preset_config(path);

   if (!root_conf)
   {
#ifdef DEBUG
      RARCH_LOG("\n");
      RARCH_WARN("[Shaders]:  Could not read root preset: %s \n", path);
#endif
      ret = false;
      goto end;
   }

   /* If we were able to get a root_config that means that the 
    * whole reference chain is valid */
#ifdef DEBUG
   RARCH_DBG("\n");
#endif

   video_shader_load_root_config_into_shader(root_conf,
         config_get_ptr(), shader);
   /* If the root_conf path matches the original path then 
    * there are no references  so we just load it and go to the end */
   if (string_is_equal(root_conf->path, path))
      goto end;
   
   /* Get the config from the initial preset file 
    * We don't need to check it's validity because it must 
    * have been valid to get the root preset */
   conf = config_file_new_from_path_to_string(path);

   /* Set all override_conf_paths to empty so we know which 
    * ones have been filled */
   for (i = 0; i < SHADER_MAX_REFERENCE_DEPTH; i++)
      override_conf_paths[i][0] = '\0';

   i = 0;

#ifdef DEBUG
   RARCH_DBG("\n");
   RARCH_DBG("[Shaders]:  Crawl Preset Reference Chain\n");
#endif

   /* If the config has a reference then we need gather all presets from the 
    * chain of references to apply their values later */
   while (conf->reference)
   {
      char* reference_preset_path = (char*)malloc(PATH_MAX_LENGTH);
      i++;

#ifdef DEBUG
      RARCH_DBG("[Shaders]:    Preset (Depth %u):  %s \n", i, conf->path);
#endif

      /* Add the reference to the list */
      strlcpy(override_conf_paths[i], conf->path, PATH_MAX_LENGTH);

      /* Get the absolute path for the reference */
      fill_pathname_expanded_and_absolute(reference_preset_path,
            conf->path, conf->reference);

#ifdef DEBUG
      RARCH_DBG("[Shaders]:      #reference     = %s \n",
            reference_preset_path);
#endif

      /* Create a new config from this reference level */
      config_file_free(conf);
      conf = config_file_new_from_path_to_string(reference_preset_path);

      free(reference_preset_path);
   }
   
   /* Step back through the references starting with the one 
    * referencing the root config and apply overrides for each one */
#ifdef DEBUG
   RARCH_DBG("\n");
   RARCH_DBG("[Shaders]:  Start Applying Simple Preset Overrides\n");
#endif

   while (i)
   {
      config_file_t *override_conf = config_file_new_from_path_to_string(
            override_conf_paths[i]);
      
#ifdef DEBUG
      RARCH_DBG("[Shaders]:    Depth %u Apply Overrides\n", i);
      RARCH_DBG("[Shaders]:      Apply values from:   %s\n",
            override_conf->path);
#endif
      override_shader_values(override_conf, shader);

      config_file_free(override_conf);
      i--;
   }

#ifdef DEBUG
   RARCH_DBG("[Shaders]:  End Apply Overrides\n");
   RARCH_DBG("\n");
#endif

   /* Set Path for originally loaded preset because it is 
    * different than the root preset path */
   strlcpy( shader->loaded_preset_path, path,
         sizeof(shader->loaded_preset_path));
end:

   config_file_free(conf);
   config_file_free(root_conf);

   return ret;
}

const char *video_shader_type_to_str(enum rarch_shader_type type)
{
   switch (type)
   {
      case RARCH_SHADER_CG:
         return "Cg";
      case RARCH_SHADER_HLSL:
         return "HLSL";
      case RARCH_SHADER_GLSL:
         return "GLSL";
      case RARCH_SHADER_SLANG:
         return "Slang";
      case RARCH_SHADER_METAL:
         return "Metal";
      case RARCH_SHADER_NONE:
         return "none";
      default:
         break;
   }

   return "???";
}

/**
 * video_shader_is_supported:
 * Tests if a shader type is supported.
 * This is only accurate once the context driver was initialized.
 **/
bool video_shader_is_supported(enum rarch_shader_type type)
{
   gfx_ctx_flags_t flags;
   enum display_flags testflag = GFX_CTX_FLAGS_NONE;

   flags.flags     = 0;

   switch (type)
   {
      case RARCH_SHADER_SLANG:
         testflag = GFX_CTX_FLAGS_SHADERS_SLANG;
         break;
      case RARCH_SHADER_GLSL:
         testflag = GFX_CTX_FLAGS_SHADERS_GLSL;
         break;
      case RARCH_SHADER_CG:
         testflag = GFX_CTX_FLAGS_SHADERS_CG;
         break;
      case RARCH_SHADER_HLSL:
         testflag = GFX_CTX_FLAGS_SHADERS_HLSL;
         break;
      case RARCH_SHADER_NONE:
      default:
         return false;
   }
   video_context_driver_get_flags(&flags);

   return BIT32_GET(flags.flags, testflag);
}

const char *video_shader_get_preset_extension(enum rarch_shader_type type)
{
   switch (type)
   {
      case RARCH_SHADER_GLSL:
         return ".glslp";
      case RARCH_SHADER_SLANG:
         return ".slangp";
      case RARCH_SHADER_HLSL:
      case RARCH_SHADER_CG:
         return ".cgp";
      default:
         break;
   }

   return NULL;
}

bool video_shader_any_supported(void)
{
   gfx_ctx_flags_t flags;
   flags.flags     = 0;
   video_context_driver_get_flags(&flags);

   return
      BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_SLANG) ||
      BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL)  ||
      BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG)    ||
      BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_HLSL);
}

enum rarch_shader_type video_shader_get_type_from_ext(
      const char *ext, bool *is_preset)
{
   if (string_is_empty(ext))
      return RARCH_SHADER_NONE;

   if (strlen(ext) > 1 && ext[0] == '.')
      ext++;

   if (is_preset)
      *is_preset =
         string_is_equal_case_insensitive(ext, "cgp")   ||
         string_is_equal_case_insensitive(ext, "glslp") ||
         string_is_equal_case_insensitive(ext, "slangp");

   if (string_is_equal_case_insensitive(ext, "cgp") ||
       string_is_equal_case_insensitive(ext, "cg")
      )
      return RARCH_SHADER_CG;

   if (string_is_equal_case_insensitive(ext, "glslp") ||
       string_is_equal_case_insensitive(ext, "glsl")
      )
      return RARCH_SHADER_GLSL;

   if (string_is_equal_case_insensitive(ext, "slangp") ||
       string_is_equal_case_insensitive(ext, "slang")
      )
      return RARCH_SHADER_SLANG;

   return RARCH_SHADER_NONE;
}

bool video_shader_check_for_changes(void)
{
   if (!file_change_data)
      return false;

   return frontend_driver_check_for_path_changes(file_change_data);
}

void dir_free_shader(
      struct rarch_dir_shader_list *dir_list,
      bool shader_remember_last_dir)
{
   if (dir_list->shader_list)
   {
      dir_list_free(dir_list->shader_list);
      dir_list->shader_list = NULL;
   }

   if (dir_list->directory)
   {
      free(dir_list->directory);
      dir_list->directory = NULL;
   }

   dir_list->selection                = 0;
   dir_list->shader_loaded            = false;
   dir_list->remember_last_preset_dir = shader_remember_last_dir;
}

static bool dir_init_shader_internal(
      bool shader_remember_last_dir,
      struct rarch_dir_shader_list *dir_list,
      const char *shader_dir,
      const char *shader_file_name,
      bool show_hidden_files)
{
   size_t i;
   struct string_list *new_list           = dir_list_new_special(
         shader_dir, DIR_LIST_SHADERS, NULL, show_hidden_files);
   bool search_file_name                  = shader_remember_last_dir &&
         !string_is_empty(shader_file_name);

   if (!new_list)
      return false;

   if (new_list->size < 1)
   {
      dir_list_free(new_list);
      return false;
   }

   dir_list_sort(new_list, false);

   dir_list->shader_list              = new_list;
   dir_list->directory                = strdup(shader_dir);
   dir_list->selection                = 0;
   dir_list->shader_loaded            = false;
   dir_list->remember_last_preset_dir = shader_remember_last_dir;

   if (search_file_name)
   {
      for (i = 0; i < new_list->size; i++)
      {
         const char *file_name = NULL;
         const char *file_path = new_list->elems[i].data;

         if (string_is_empty(file_path))
            continue;

         /* If a shader file name has been provided,
          * search the list for a match and set 'selection'
          * index if found */
         file_name = path_basename(file_path);

         if (!string_is_empty(file_name) &&
               string_is_equal(file_name, shader_file_name))
         {
            RARCH_LOG("[Shaders]: %s \"%s\"\n",
                  msg_hash_to_str(MSG_FOUND_SHADER),
                  file_path);

            dir_list->selection = i;
            break;
         }
      }
   }

   return true;
}

void dir_init_shader(
      void *menu_driver_data_,
      settings_t *settings,
      struct rarch_dir_shader_list *dir_list)
{
   bool show_hidden_files                         = settings->bools.show_hidden_files;
   bool shader_remember_last_dir                  = settings->bools.video_shader_remember_last_dir;
   const char *directory_video_shader             = settings->paths.directory_video_shader;
   const char *directory_menu_config              = settings->paths.directory_menu_config;
   bool video_shader_remember_last_dir            = settings->bools.video_shader_remember_last_dir;
   const char *last_shader_preset_dir             = NULL;
   const char *last_shader_preset_file_name       = NULL;
#if defined(HAVE_MENU)
   menu_handle_t *menu                            = (menu_handle_t*)menu_driver_data_;
   enum rarch_shader_type last_shader_preset_type = menu ? menu->last_shader_selection.preset_type : RARCH_SHADER_NONE;
   menu_driver_get_last_shader_preset_path(
         &last_shader_preset_dir, &last_shader_preset_file_name);
#else
   enum rarch_shader_type last_shader_preset_type = RARCH_SHADER_NONE;
#endif

   /* Always free existing shader list */
   dir_free_shader(dir_list,
         video_shader_remember_last_dir);

   /* Try directory of last selected shader preset */
   if (shader_remember_last_dir &&
       (last_shader_preset_type != RARCH_SHADER_NONE) &&
       !string_is_empty(last_shader_preset_dir) &&
       dir_init_shader_internal(
          video_shader_remember_last_dir,
          dir_list,
          last_shader_preset_dir,
          last_shader_preset_file_name,
          show_hidden_files))
      return;

   /* Try video shaders directory */
   if (!string_is_empty(directory_video_shader) &&
       dir_init_shader_internal(
            video_shader_remember_last_dir,
            dir_list,
            directory_video_shader, NULL, show_hidden_files))
      return;

   /* Try config directory */
   if (!string_is_empty(directory_menu_config) &&
       dir_init_shader_internal(
            video_shader_remember_last_dir,
            dir_list,
            directory_menu_config, NULL, show_hidden_files))
      return;

   /* Try 'top level' directory containing main
    * RetroArch config file */
   if (!path_is_empty(RARCH_PATH_CONFIG))
   {
      char *rarch_config_directory = strdup(path_get(RARCH_PATH_CONFIG));
      path_basedir(rarch_config_directory);

      if (!string_is_empty(rarch_config_directory))
         dir_init_shader_internal(
               video_shader_remember_last_dir,
               dir_list,
               rarch_config_directory, NULL, show_hidden_files);

      free(rarch_config_directory);
   }
}

void dir_check_shader(
      void *menu_driver_data_,
      settings_t *settings,
      struct rarch_dir_shader_list *dir_list,
      bool pressed_next,
      bool pressed_prev)
{
   bool video_shader_remember_last_dir            = settings->bools.video_shader_remember_last_dir;
   const char *last_shader_preset_dir             = NULL;
   const char *last_shader_preset_file_name       = NULL;
   const char *set_shader_path                    = NULL;
   bool dir_list_initialised                      = false;
#if defined(HAVE_MENU)
   void *menu_ptr                                 = menu_driver_data_;
   menu_handle_t *menu                            = (menu_handle_t*)menu_ptr;
   enum rarch_shader_type last_shader_preset_type = menu ? menu->last_shader_selection.preset_type : RARCH_SHADER_NONE;
   menu_driver_get_last_shader_preset_path(
         &last_shader_preset_dir, &last_shader_preset_file_name);
#else
   void *menu_ptr                                 = NULL;
   enum rarch_shader_type last_shader_preset_type = RARCH_SHADER_NONE;
#endif

   /* Check whether shader list needs to be
    * (re)initialised */
   if (!dir_list->shader_list ||
       (dir_list->remember_last_preset_dir != video_shader_remember_last_dir) ||
       (video_shader_remember_last_dir &&
        (last_shader_preset_type != RARCH_SHADER_NONE) &&
        !string_is_equal(dir_list->directory, last_shader_preset_dir)))
   {
      dir_init_shader(menu_ptr, settings, dir_list);
      dir_list_initialised = true;
   }

   if (!dir_list->shader_list ||
       (dir_list->shader_list->size < 1))
      return;

   /* Check whether a 'last used' shader file
    * name is provided
    * > Note: We can end up calling
    *   string_is_equal(dir_list->directory, last_shader_preset_dir)
    *   twice. This is wasteful, but we cannot safely cache
    *   the first result since dir_init_shader() is called
    *   in-between the two invocations... */
   if (video_shader_remember_last_dir &&
       (last_shader_preset_type != RARCH_SHADER_NONE) &&
       string_is_equal(dir_list->directory, last_shader_preset_dir) &&
       !string_is_empty(last_shader_preset_file_name))
   {
      /* Ensure that we start with a dir_list selection
       * index matching the last used shader */
      if (!dir_list_initialised)
      {
         const char *current_file_path = NULL;
         const char *current_file_name = NULL;

         if (dir_list->selection < dir_list->shader_list->size)
            current_file_path = dir_list->shader_list->elems[dir_list->selection].data;

         if (!string_is_empty(current_file_path))
            current_file_name = path_basename(current_file_path);

         if (!string_is_empty(current_file_name) &&
             !string_is_equal(current_file_name, last_shader_preset_file_name))
         {
            size_t i;
            for (i = 0; i < dir_list->shader_list->size; i++)
            {
               const char *file_path = dir_list->shader_list->elems[i].data;
               const char *file_name = NULL;

               if (string_is_empty(file_path))
                  continue;

               file_name = path_basename(file_path);

               if (string_is_empty(file_name))
                  continue;

               if (string_is_equal(file_name, last_shader_preset_file_name))
               {
                  dir_list->selection = i;
                  break;
               }
            }
         }
      }

#ifdef HAVE_MENU
      /* Check whether the shader referenced by the
       * current selection index is already loaded */
      if (!dir_list->shader_loaded)
      {
         struct video_shader *shader = menu_shader_get();

         if (shader && !string_is_empty(shader->loaded_preset_path))
         {
            char last_shader_path[PATH_MAX_LENGTH];
            last_shader_path[0] = '\0';

            fill_pathname_join(last_shader_path,
                  last_shader_preset_dir, last_shader_preset_file_name,
                  sizeof(last_shader_path));

            if (string_is_equal(last_shader_path, shader->loaded_preset_path))
               dir_list->shader_loaded = true;
         }
      }
#endif
   }

   /* Select next shader in list */
   if (pressed_next)
   {
      /* Only increment selection if a shader
       * from this list has already been loaded
       * (otherwise first entry in the list may
       * be skipped) */
      if (dir_list->shader_loaded)
      {
         if (dir_list->selection < dir_list->shader_list->size - 1)
            dir_list->selection++;
         else
            dir_list->selection = 0;
      }
   }
   /* Select previous shader in list */
   else if (pressed_prev)
   {
      if (dir_list->selection > 0)
         dir_list->selection--;
      else
         dir_list->selection = dir_list->shader_list->size - 1;
   }
   else
      return;

   set_shader_path = dir_list->shader_list->elems[dir_list->selection].data;
#if defined(HAVE_MENU)
   menu_driver_set_last_shader_preset_path(set_shader_path);
#endif
   command_set_shader(NULL, set_shader_path);
   dir_list->shader_loaded = true;
}

static bool retroarch_load_shader_preset_internal(
      char *s,
      size_t len,
      const char *shader_directory,
      const char *core_name,
      const char *special_name)
{
   unsigned i;

   static enum rarch_shader_type types[] =
   {
      /* Shader preset priority, highest to lowest
       * only important for video drivers with multiple shader backends */
      RARCH_SHADER_GLSL, RARCH_SHADER_SLANG, RARCH_SHADER_CG, RARCH_SHADER_HLSL
   };

   for (i = 0; i < ARRAY_SIZE(types); i++)
   {
      if (!video_shader_is_supported(types[i]))
         continue;

      /* Concatenate strings into full paths */
      if (!string_is_empty(core_name))
         fill_pathname_join_special_ext(s,
               shader_directory, core_name,
               special_name,
               video_shader_get_preset_extension(types[i]),
               len);
      else
      {
         if (string_is_empty(special_name))
            break;

         fill_pathname_join(s, shader_directory, special_name, len);
         strlcat(s, video_shader_get_preset_extension(types[i]), len);
      }

      if (path_is_valid(s))
         return true;
   }

   return false;
}

bool load_shader_preset(settings_t *settings, const char *core_name,
      char *s, size_t len)
{
   const char *video_shader_directory = settings->paths.directory_video_shader;
   const char *menu_config_directory  = settings->paths.directory_menu_config;
   const char *rarch_path_basename    = path_get(RARCH_PATH_BASENAME);

   const char *game_name              = path_basename(rarch_path_basename);
   const char *dirs[3]                = {0};
   size_t i                           = 0;

   char shader_path[PATH_MAX_LENGTH];
   char content_dir_name[PATH_MAX_LENGTH];
   char config_file_directory[PATH_MAX_LENGTH];
   char old_presets_directory[PATH_MAX_LENGTH];

   shader_path[0]                     = '\0';
   content_dir_name[0]                = '\0';
   config_file_directory[0]           = '\0';
   old_presets_directory[0]           = '\0';

   if (!string_is_empty(rarch_path_basename))
      fill_pathname_parent_dir_name(content_dir_name,
            rarch_path_basename, sizeof(content_dir_name));

   config_file_directory[0]           = '\0';

   if (!path_is_empty(RARCH_PATH_CONFIG))
      fill_pathname_basedir(config_file_directory,
            path_get(RARCH_PATH_CONFIG), sizeof(config_file_directory));

   old_presets_directory[0]           = '\0';

   if (!string_is_empty(video_shader_directory))
      fill_pathname_join(old_presets_directory,
         video_shader_directory, "presets", sizeof(old_presets_directory));

   dirs[0]                            = menu_config_directory;
   dirs[1]                            = config_file_directory;
   dirs[2]                            = old_presets_directory;

   for (i = 0; i < ARRAY_SIZE(dirs); i++)
   {
      if (string_is_empty(dirs[i]))
         continue;
      /* Game-specific shader preset found? */
      if (retroarch_load_shader_preset_internal(
               shader_path,
               sizeof(shader_path),
               dirs[i], core_name,
               game_name))
         goto success;
      /* Folder-specific shader preset found? */
      if (retroarch_load_shader_preset_internal(
               shader_path,
               sizeof(shader_path),
               dirs[i], core_name,
               content_dir_name))
         goto success;
      /* Core-specific shader preset found? */
      if (retroarch_load_shader_preset_internal(
               shader_path,
               sizeof(shader_path),
               dirs[i], core_name,
               core_name))
         goto success;
      /* Global shader preset found? */
      if (retroarch_load_shader_preset_internal(
               shader_path,
               sizeof(shader_path),
               dirs[i], NULL,
               "global"))
         goto success;
   }
   return false;

success:
   /* Shader preset exists, load it. */
   strlcpy(s, shader_path, len);
   return true;
}

bool apply_shader(
      settings_t *settings,
      enum rarch_shader_type type,
      const char *preset_path, bool message)
{
   char msg[256];
   video_driver_state_t 
      *video_st                 = video_state_get_ptr();
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   const char      *core_name   = runloop_st->system.info.library_name;
   const char      *preset_file = NULL;
#ifdef HAVE_MENU
   struct video_shader *shader  = menu_shader_get();
#endif

   /* Disallow loading shaders when no core is loaded */
   if (string_is_empty(core_name))
      return false;

   if (!string_is_empty(preset_path))
      preset_file = path_basename_nocompression(preset_path);

   /* TODO/FIXME - This loads the shader into the video driver
    * But then we load the shader from disk twice more to put it in the menu
    * We need to reconfigure this at some point to only load it once */
   if (video_st->current_video->set_shader)
   {
      if ((video_st->current_video->set_shader(
                  video_st->data, type, preset_path)))
      {
         configuration_set_bool(settings, settings->bools.video_shader_enable, true);
         if (!string_is_empty(preset_path))
         {
            strlcpy(runloop_st->runtime_shader_preset_path, preset_path,
                  sizeof(runloop_st->runtime_shader_preset_path));
#ifdef HAVE_MENU
            /* reflect in shader manager */
            if (menu_shader_manager_set_preset(
                     shader, type, preset_path, false))
               shader->modified = false;
#endif
         }
         else
            runloop_st->runtime_shader_preset_path[0] = '\0';

         if (message)
         {
            /* Display message */
            if (preset_file)
               snprintf(msg, sizeof(msg),
                     "%s: \"%s\"",
                     msg_hash_to_str(MSG_SHADER),
                     preset_file);
            else
               snprintf(msg, sizeof(msg),
                     "%s: %s", 
                     msg_hash_to_str(MSG_SHADER),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE)
                     );
#ifdef HAVE_GFX_WIDGETS
            if (dispwidget_get_ptr()->active)
               gfx_widget_set_generic_message(msg, 2000);
            else
#endif
               runloop_msg_queue_push(msg, 1, 120, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }

         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_APPLYING_SHADER),
               preset_path ? preset_path : "null");

         return true;
      }
   }

#ifdef HAVE_MENU
   /* reflect in shader manager */
   menu_shader_manager_set_preset(shader, type, NULL, false);
#endif

   /* Display error message */
   fill_pathname_join_delim(msg,
         msg_hash_to_str(MSG_FAILED_TO_APPLY_SHADER_PRESET),
         preset_file ? preset_file : "null",
         ' ',
         sizeof(msg));

   runloop_msg_queue_push(
         msg, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   return false;
}

/* get the name of the current shader preset */
const char *retroarch_get_shader_preset(void)
{
   settings_t *settings           = config_get_ptr();
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   const char *core_name          = runloop_st->system.info.library_name;
   bool video_shader_enable       = settings->bools.video_shader_enable;
   unsigned video_shader_delay    = settings->uints.video_shader_delay;
   bool auto_shaders_enable       = settings->bools.auto_shaders_enable;
   bool cli_shader_disable        = video_st->cli_shader_disable;

   if (!video_shader_enable)
      return NULL;

   if (video_shader_delay && !runloop_st->shader_delay_timer.timer_end)
      return NULL;

   /* Disallow loading auto-shaders when no core is loaded */
   if (string_is_empty(core_name))
      return NULL;

   if (!string_is_empty(runloop_st->runtime_shader_preset_path))
      return runloop_st->runtime_shader_preset_path;

   /* load auto-shader once, --set-shader works like a global auto-shader */
   if (video_st->shader_presets_need_reload && !cli_shader_disable)
   {
      video_st->shader_presets_need_reload = false;

      if (video_shader_is_supported(
               video_shader_parse_type(video_st->cli_shader_path)))
         strlcpy(runloop_st->runtime_shader_preset_path,
               video_st->cli_shader_path,
               sizeof(runloop_st->runtime_shader_preset_path));
      else
      {
         if (auto_shaders_enable) /* sets runtime_shader_preset_path */
         {
            if (load_shader_preset(
                     settings,
                     runloop_st->system.info.library_name,
                     runloop_st->runtime_shader_preset_path,
                     sizeof(runloop_st->runtime_shader_preset_path)))
            {
               RARCH_LOG("[Shaders]: Specific shader preset found at %s.\n",
                     runloop_st->runtime_shader_preset_path);
            }
         }
      }
      return runloop_st->runtime_shader_preset_path;
   }

   return NULL;
}
