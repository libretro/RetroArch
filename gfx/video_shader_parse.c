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
#include <rhash.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <lists/string_list.h>

#include "../configuration.h"
#include "../retroarch.h"
#include "../verbosity.h"
#include "../frontend/frontend_driver.h"
#include "../command.h"
#include "../file_path_special.h"
#include "video_shader_parse.h"

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
#include "drivers_shader/slang_process.h"
#endif

static path_change_data_t *file_change_data = NULL;

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

   RARCH_WARN("Invalid wrapping type %s. Valid ones are: clamp_to_border"
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
   char wrap_mode[64];
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
   char frame_count_mod[64];
   size_t path_size             = PATH_MAX_LENGTH;
   char *tmp_path               = (char*)malloc(path_size);
   struct gfx_fbo_scale *scale  = NULL;
   bool tmp_bool                = false;
   float fattr                  = 0.0f;
   int iattr                    = 0;

   if (!tmp_path)
      return false;

   fp_fbo_buf[0]      = mipmap_buf[0]          = alias_buf[0]       =
   scale_name_buf[0]  = attr_name_buf[0]       = scale_type[0]      =
   scale_type_x[0]    = scale_type_y[0]        = frame_count_mod[0] =
   shader_name[0]     = filter_name_buf[0]     = wrap_name_buf[0]   = 
   wrap_mode[0]       = frame_count_mod_buf[0] = srgb_output_buf[0] = '\0';

   /* Source */
   snprintf(shader_name, sizeof(shader_name), "shader%u", i);
   if (!config_get_path(conf, shader_name, tmp_path, path_size))
   {
      RARCH_ERR("Couldn't parse shader source (%s).\n", shader_name);
      free(tmp_path);
      return false;
   }

   fill_pathname_resolve_relative(pass->source.path,
         conf->path, tmp_path, sizeof(pass->source.path));
   free(tmp_path);

   /* Smooth */
   snprintf(filter_name_buf, sizeof(filter_name_buf), "filter_linear%u", i);

   if (config_get_bool(conf, filter_name_buf, &tmp_bool))
   {
      bool smooth = tmp_bool;
      pass->filter = smooth ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
   }
   else
      pass->filter = RARCH_FILTER_UNSPEC;

   /* Wrapping mode */
   snprintf(wrap_name_buf, sizeof(wrap_name_buf), "wrap_mode%u", i);
   if (config_get_array(conf, wrap_name_buf, wrap_mode, sizeof(wrap_mode)))
      pass->wrap = wrap_str_to_mode(wrap_mode);

   /* Frame count mod */
   snprintf(frame_count_mod_buf, sizeof(frame_count_mod_buf), "frame_count_mod%u", i);
   if (config_get_array(conf, frame_count_mod_buf,
            frame_count_mod, sizeof(frame_count_mod)))
      pass->frame_count_mod = (unsigned)strtoul(frame_count_mod, NULL, 0);

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
         RARCH_ERR("Invalid attribute.\n");
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
         RARCH_ERR("Invalid attribute.\n");
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
   char *tmp_path       = textures + 1024;

   if (!textures)
      return false;

   textures[0] = '\0';

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
      char wrap_mode[64];
      char id_mipmap[64];
      bool mipmap         = false;
      bool smooth         = false;

      id_filter[0] = id_wrap[0] = wrap_mode[0] = id_mipmap[0] = '\0';

      if (!config_get_array(conf, id, tmp_path, path_size))
      {
         RARCH_ERR("Cannot find path to texture \"%s\" ...\n", id);
         free(textures);
         return false;
      }

      fill_pathname_resolve_relative(shader->lut[shader->luts].path,
            conf->path, tmp_path, sizeof(shader->lut[shader->luts].path));

      strlcpy(shader->lut[shader->luts].id, id,
            sizeof(shader->lut[shader->luts].id));

      strlcpy(id_filter, id, sizeof(id_filter));
      strlcat(id_filter, "_linear", sizeof(id_filter));
      if (config_get_bool(conf, id_filter, &smooth))
         shader->lut[shader->luts].filter = smooth ?
            RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
      else
         shader->lut[shader->luts].filter = RARCH_FILTER_UNSPEC;

      strlcpy(id_wrap, id, sizeof(id_wrap));
      strlcat(id_wrap, "_wrap_mode", sizeof(id_wrap));
      if (config_get_array(conf, id_wrap, wrap_mode, sizeof(wrap_mode)))
         shader->lut[shader->luts].wrap = wrap_str_to_mode(wrap_mode);

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
 * video_shader_set_current_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Reads the current value for all parameters from config file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_resolve_current_parameters(config_file_t *conf,
      struct video_shader *shader)
{
   size_t param_size     = 4096 * sizeof(char);
   const char *id        = NULL;
   char *parameters      = NULL;
   char *save            = NULL;

   if (!conf)
      return false;

   parameters            = (char*)malloc(param_size);

   if (!parameters)
      return false;

   parameters[0]         = '\0';

   /* Read in parameters which override the defaults. */
   if (!config_get_array(conf, "parameters",
            parameters, param_size))
   {
      free(parameters);
      return true;
   }

   for (id = strtok_r(parameters, ";", &save); id;
         id = strtok_r(NULL, ";", &save))
   {
      struct video_shader_parameter *parameter =
         (struct video_shader_parameter*)
         video_shader_parse_find_parameter(
               shader->parameters, shader->num_parameters, id);

      if (!parameter)
      {
         RARCH_WARN("[CGP/GLSLP]: Parameter %s is set in the preset,"
               " but no shader uses this parameter, ignoring.\n", id);
         continue;
      }

      if (!config_get_float(conf, id, &parameter->current))
         RARCH_WARN("[CGP/GLSLP]: Parameter %s is not set in preset.\n", id);
   }

   free(parameters);
   return true;
}

/**
 * video_shader_resolve_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Resolves all shader parameters belonging to shaders.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_resolve_parameters(config_file_t *conf,
      struct video_shader *shader)
{
   unsigned i;
   struct video_shader_parameter *param = &shader->parameters[0];

   shader->num_parameters = 0;

   /* Find all parameters in our shaders. */

   for (i = 0; i < shader->passes; i++)
   {
      const char *path          = shader->pass[i].source.path;
      uint8_t *buf              = NULL;
      int64_t buf_len           = 0;
      struct string_list *lines = NULL;
      size_t line_index         = 0;

      if (string_is_empty(path))
         continue;

      if (!path_is_valid(path))
         continue;

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      /* First try to use the more robust slang
       * implementation to support #includes. */
      /* FIXME: The check for slang can be removed
       * if it's sufficiently tested for
       * GLSL/Cg as well, it should be the same implementation. */
      if (string_is_equal(path_get_extension(path), "slang") &&
            slang_preprocess_parse_parameters(path, shader))
         continue;

      /* If that doesn't work, fallback to the old path.
       * Ideally, we'd get rid of this path sooner or later. */
#endif

      /* Read file contents */
      if (!filestream_read_file(path, (void**)&buf, &buf_len))
         continue;

      /* Split into lines */
      if (buf_len > 0)
         lines = string_split((const char*)buf, "\n");

      /* Buffer is no longer required - clean up */
      if ((void*)buf)
         free((void*)buf);

      if (!lines)
         continue;

      /* even though the pass is set in the loop too, not all passes have parameters */
      param->pass = i;

      while ((shader->num_parameters < ARRAY_SIZE(shader->parameters)) &&
             (line_index < lines->size))
      {
         int ret;
         const char *line = lines->elems[line_index].data;
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
            param->step = 0.1f * (param->maximum - param->minimum);

         param->pass = i;

         RARCH_LOG("Found #pragma parameter %s (%s) %f %f %f %f in pass %d\n",
               param->desc,    param->id,      param->initial,
               param->minimum, param->maximum, param->step, param->pass);
         param->current = param->initial;

         shader->num_parameters++;
         param++;
      }

      string_list_free(lines);
   }

   return video_shader_resolve_current_parameters(conf, shader);
}

/**
 * video_shader_write_preset:
 * @path              : File to write to
 * @shader            : Shader preset to write
 * @reference         : Whether a reference preset should be written
 *
 * Writes a preset to disk. Can be written as a reference preset.
 * See: video_shader_read_preset
 **/
bool video_shader_write_preset(const char *path,
      const struct video_shader *shader, bool reference)
{
   /* We need to clean up paths to be able to properly process them
    * path and shader->path can use '/' on Windows due to Qt being Qt */
   char clean_path[PATH_MAX_LENGTH];
   char clean_shader_path[PATH_MAX_LENGTH];
   char preset_dir[PATH_MAX_LENGTH];

   if (!shader || string_is_empty(path))
      return false;

   fill_pathname_join(
      preset_dir,
      config_get_ptr()->paths.directory_video_shader,
      "presets",
      sizeof(preset_dir));

   strlcpy(clean_shader_path, shader->path, PATH_MAX_LENGTH);
   path_resolve_realpath(clean_shader_path, PATH_MAX_LENGTH, false);

   if (string_is_empty(shader->path))
      reference = false;

   /* Auto-shaders can be written as copies or references.
    * If we write a reference to a copy, we could then overwrite the copy 
    * with any reference, thus creating a reference to a reference.
    * To prevent this, we disallow saving references to auto-shaders. */
   if (reference && !strncmp(preset_dir, clean_shader_path, strlen(preset_dir)))
      reference = false;

   /* Don't ever create a reference to the ever-changing retroarch preset
    * TODO remove once we don't write this preset anymore */
   if (reference && !strncmp(path_basename(clean_shader_path), "retroarch", STRLEN_CONST("retroarch")))
      reference = false;

   if (reference)
   {
      /* write a reference preset */
      char buf[STRLEN_CONST("#reference \"") + PATH_MAX_LENGTH + 1] = "#reference \"";
      size_t len = STRLEN_CONST("#reference \"");

      char *preset_ref = buf + len;

      strlcpy(clean_path, path, PATH_MAX_LENGTH);
      path_resolve_realpath(clean_path, PATH_MAX_LENGTH, false);

      path_relative_to(preset_ref, clean_shader_path, clean_path, PATH_MAX_LENGTH);
      len += strlen(preset_ref);

      buf[len++] = '\"';

      return filestream_write_file(clean_path, (void *)buf, len);
   }
   else
   {
      /* regular saving function */
      config_file_t *conf;
      bool ret;

      if (!(conf = config_file_new_alloc()))
         return false;

      video_shader_write_conf_preset(conf, shader, path);

      ret = config_file_write(conf, path, false);

      config_file_free(conf);

      return ret;
   }
}

/**
 * video_shader_read_reference_path:
 * @path              : File to read
 *
 * Returns: the reference path of a preset if it exists,
 * otherwise returns NULL.
 *
 * The returned string needs to be freed.
 */
char *video_shader_read_reference_path(const char *path)
{
   /* We want shader presets that point to other presets.
    *
    * While config_file_new_from_path_to_string() does support the
    * #include directive, it will not rebase relative paths on
    * the include path.
    *
    * There's a plethora of reasons why a general solution is hard:
    *  - it's impossible to distinguish a generic string from a (relative) path
    *  - config_file_new_from_path_to_string() doesn't return the include path,
    *    so we cannot rebase afterwards
    *  - #include is recursive, so we'd actually need to track the include path
    *    for every setting
    *
    * So instead, we use a custom #reference directive, which is just a
    * one-time/non-recursive redirection, e.g.
    *
    * #reference "<path to config>"
    * or
    * #reference <path to config>
    *
    * which we will load as config_file_new_from_path_to_string(<path to config>).
    */
   char *reference     = NULL;
   RFILE *file         = NULL;
   char *line          = NULL;

   if (string_is_empty(path))
     goto end;
   if (!path_is_valid(path))
     goto end;

   file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
     goto end;

   line = filestream_getline(file);
   filestream_close(file);

   if (line && !strncmp("#reference", line, STRLEN_CONST("#reference")))
   {
      char *ref_path = line + STRLEN_CONST("#reference");

      /* have at least 1 whitespace */
      if (!isspace(*ref_path))
         goto end;
      ref_path++;

      while (isspace(*ref_path))
         ref_path++;

      if (*ref_path == '\"')
      {
         /* remove "" */
         char *p;
         ref_path++;

         p = ref_path;
         while (*p != '\0' && *p != '\"')
            p++;

         if (*p == '\"')
         {
            *p = '\0';
         }
         else
         {
            /* if there's no second ", remove whitespace at the end */
            p--;
            while (isspace(*p))
               *p-- = '\0';
         }
      }
      else
      {
         /* remove whitespace at the end (e.g. carriage return) */
         char *end = ref_path + strlen(ref_path) - 1;
         while (isspace(*end))
            *end-- = '\0';
      }

      if (string_is_empty(ref_path))
         goto end;

      reference = (char *)malloc(PATH_MAX_LENGTH);

      if (!reference)
         goto end;

      /* rebase relative reference path */
      if (!path_is_absolute(ref_path))
      {
         fill_pathname_resolve_relative(reference,
               path, ref_path, PATH_MAX_LENGTH);
      }
      else
         strlcpy(reference, ref_path, PATH_MAX_LENGTH);
   }

end:
   if (line)
      free(line);

   return reference;
}

/**
 * video_shader_read_preset:
 * @path              : File to read
 *
 * Reads a preset from disk.
 * If the preset is a reference preset, the referenced preset
 * is loaded instead.
 *
 * Returns the read preset as a config object.
 *
 * The returned config object needs to be freed.
 **/
config_file_t *video_shader_read_preset(const char *path)
{
   config_file_t *conf;
   char *reference = video_shader_read_reference_path(path);
   if (reference)
   {
      conf = config_file_new_from_path_to_string(reference);
      free(reference);
   }
   else
      conf = config_file_new_from_path_to_string(path);

   return conf;
}

/**
 * video_shader_read_conf_preset:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Loads preset file and all associated state (passes,
 * textures, imports, etc).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_read_conf_preset(config_file_t *conf,
      struct video_shader *shader)
{
   unsigned i;
   union string_list_elem_attr attr;
   unsigned shaders                 = 0;
   settings_t *settings             = config_get_ptr();
   struct string_list *file_list    = NULL;
   bool watch_files                 = settings->bools.video_shader_watch_files;

   memset(shader, 0, sizeof(*shader));

   if (!config_get_uint(conf, "shaders", &shaders))
   {
      RARCH_ERR("Cannot find \"shaders\" param.\n");
      return false;
   }

   if (!shaders)
   {
      RARCH_ERR("Need to define at least 1 shader.\n");
      return false;
   }

   if (!config_get_int(conf, "feedback_pass",
            &shader->feedback_pass))
      shader->feedback_pass = -1;

   shader->passes = MIN(shaders, GFX_MAX_SHADERS);
   attr.i         = 0;

   strlcpy(shader->path, conf->path, sizeof(shader->path));

   if (watch_files)
   {
      if (file_change_data)
         frontend_driver_watch_path_for_changes(NULL,
               0, &file_change_data);

      file_change_data = NULL;
      file_list        = string_list_new();
      string_list_append(file_list, conf->path, attr);
   }

   for (i = 0; i < shader->passes; i++)
   {
      if (!video_shader_parse_pass(conf, &shader->pass[i], i))
      {
         if (file_list)
         {
            string_list_free(file_list);
            file_list = NULL;
         }
         return false;
      }

      if (watch_files && file_list)
         string_list_append(file_list,
               shader->pass[i].source.path, attr);
   }

   if (watch_files)
   {
      int flags = PATH_CHANGE_TYPE_MODIFIED          |
                  PATH_CHANGE_TYPE_WRITE_FILE_CLOSED |
                  PATH_CHANGE_TYPE_FILE_MOVED        |
                  PATH_CHANGE_TYPE_FILE_DELETED;

      frontend_driver_watch_path_for_changes(file_list,
            flags, &file_change_data);
      if (file_list)
         string_list_free(file_list);
   }

   if (!video_shader_parse_textures(conf, shader))
      return false;

   return true;
}

/* CGP store */
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
      enum gfx_scale_type type, float scale,
      unsigned absolute, unsigned i)
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

   shader_write_scale_dim(conf, "x", fbo->type_x,
         fbo->scale_x, fbo->abs_x, i);
   shader_write_scale_dim(conf, "y", fbo->type_y,
         fbo->scale_y, fbo->abs_y, i);
}

static void make_relative_path_portable(char *path)
{
#ifdef _WIN32
   /* use '/' instead of '\' for maximum portability */
   if (!path_is_absolute(path))
   {
      char *p;
      for (p = path; *p; p++)
         if (*p == '\\')
            *p = '/';
   }
#endif
}

/**
 * video_shader_write_conf_preset:
 * @conf              : Preset file to write to.
 * @shader            : Shader passes handle.
 * @preset_path       : Optional path to where the preset will be written.
 *
 * Writes preset and all associated state (passes,
 * textures, imports, etc) into @conf.
 * If @preset_path is not NULL, shader paths are saved
 * relative to it.
 **/
void video_shader_write_conf_preset(config_file_t *conf,
      const struct video_shader *shader, const char *preset_path)
{
   unsigned i;
   char key[64];
   size_t tmp_size = PATH_MAX_LENGTH;
   char *tmp       = (char*)malloc(3*tmp_size);
   char *tmp_rel   = tmp +   tmp_size;
   char *tmp_base  = tmp + 2*tmp_size;

   if (!tmp)
      return;

   config_set_int(conf, "shaders", shader->passes);
   if (shader->feedback_pass >= 0)
      config_set_int(conf, "feedback_pass", shader->feedback_pass);

   if (preset_path)
   {
      strlcpy(tmp_base, preset_path, tmp_size);

      /* ensure we use a clean base like the shader passes and texture paths do */
      path_resolve_realpath(tmp_base, tmp_size, false);
      path_basedir(tmp_base);
   }

   for (i = 0; i < shader->passes; i++)
   {
      const struct video_shader_pass *pass = &shader->pass[i];

      snprintf(key, sizeof(key), "shader%u", i);

      if (preset_path)
      {
         strlcpy(tmp, pass->source.path, tmp_size);
         path_relative_to(tmp_rel, tmp, tmp_base, tmp_size);
         make_relative_path_portable(tmp_rel);

         config_set_path(conf, key, tmp_rel);
      }
      else
         config_set_path(conf, key, pass->source.path);


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


   if (shader->num_parameters)
   {
      size_t param_size = 4096 * sizeof(char);
      char *parameters  = (char*)malloc(param_size);

      if (parameters)
      {
         parameters[0] = '\0';

         strlcpy(parameters, shader->parameters[0].id, param_size);

         for (i = 1; i < shader->num_parameters; i++)
         {
            /* O(n^2), but number of parameters is very limited. */
            strlcat(parameters, ";", param_size);
            strlcat(parameters, shader->parameters[i].id, param_size);
         }

         config_set_string(conf, "parameters", parameters);

         for (i = 0; i < shader->num_parameters; i++)
            config_set_float(conf, shader->parameters[i].id,
                  shader->parameters[i].current);
         free(parameters);
      }
   }

   if (shader->luts)
   {
      size_t tex_size = 4096 * sizeof(char);
      char *textures  = (char*)malloc(tex_size);

      if (textures)
      {
         textures[0] = '\0';

         strlcpy(textures, shader->lut[0].id, tex_size);

         for (i = 1; i < shader->luts; i++)
         {
            /* O(n^2), but number of textures is very limited. */
            strlcat(textures, ";", tex_size);
            strlcat(textures, shader->lut[i].id, tex_size);
         }

         config_set_string(conf, "textures", textures);

         free(textures);

         for (i = 0; i < shader->luts; i++)
         {
            if (preset_path)
            {
               strlcpy(tmp, shader->lut[i].path, tmp_size);
               path_relative_to(tmp_rel, tmp, tmp_base, tmp_size);
               make_relative_path_portable(tmp_rel);

               config_set_path(conf, shader->lut[i].id, tmp_rel);
            }
            else
               config_set_path(conf, shader->lut[i].id, shader->lut[i].path);

            if (shader->lut[i].filter != RARCH_FILTER_UNSPEC)
            {
               char key[128];
               key[0]  = '\0';
               strlcpy(key, shader->lut[i].id, sizeof(key));
               strlcat(key, "_linear", sizeof(key));
               config_set_bool(conf, key,
                     shader->lut[i].filter == RARCH_FILTER_LINEAR);
            }

            {
               char key[128];
               key[0]  = '\0';
               strlcpy(key, shader->lut[i].id, sizeof(key));
               strlcat(key, "_wrap_mode", sizeof(key));
               config_set_string(conf, key,
                     wrap_mode_to_str(shader->lut[i].wrap));
            }

            {
               char key[128];
               key[0]  = '\0';
               strlcpy(key, shader->lut[i].id, sizeof(key));
               strlcat(key, "_mipmap", sizeof(key));
               config_set_bool(conf, key,
                     shader->lut[i].mipmap);
            }
         }
      }
   }

   free(tmp);
}

const char *video_shader_to_str(enum rarch_shader_type type)
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

enum rarch_shader_type video_shader_get_type_from_ext(const char *ext,
      bool *is_preset)
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

/**
 * video_shader_parse_type:
 * @path              : Shader path.
 *
 * Parses type of shader.
 *
 * Returns: value of shader type if it could be determined,
 * otherwise RARCH_SHADER_NONE.
 **/
enum rarch_shader_type video_shader_parse_type(const char *path)
{
   return video_shader_get_type_from_ext(path_get_extension(path), NULL);
}

bool video_shader_check_for_changes(void)
{
   if (!file_change_data)
      return false;

   return frontend_driver_check_for_path_changes(file_change_data);
}
