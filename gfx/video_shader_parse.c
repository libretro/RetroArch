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
#include <rhash.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <lists/string_list.h>

#include "../configuration.h"
#include "../verbosity.h"
#include "../frontend/frontend_driver.h"
#include "../command.h"
#include "../file_path_special.h"
#include "../retroarch.h"
#include "video_shader_parse.h"

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
#include "drivers_shader/slang_process.h"
#endif

/* TODO/FIXME - global state - perhaps move outside this file */
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

   RARCH_WARN("[ Shaders ]:  Invalid wrapping type %s. Valid ones are: clamp_to_border"
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
   struct gfx_fbo_scale *scale  = NULL;
   bool tmp_bool                = false;
   float fattr                  = 0.0f;
   int iattr                    = 0;
   struct config_entry_list 
      *entry                    = NULL;

   fp_fbo_buf[0]      = mipmap_buf[0]          = alias_buf[0]       =
   scale_name_buf[0]  = attr_name_buf[0]       = scale_type[0]      =
   scale_type_x[0]    = scale_type_y[0]        =
   shader_name[0]     = filter_name_buf[0]     = wrap_name_buf[0]   = 
                        frame_count_mod_buf[0] = srgb_output_buf[0] = '\0';

   /* Source */
   snprintf(shader_name, sizeof(shader_name), "shader%u", i);
   if (!config_get_path(conf, shader_name, tmp_path, sizeof(tmp_path)))
   {
      RARCH_ERR("[ Shaders ]:  Couldn't parse shader source (%s).\n", shader_name);
      return false;
   }

   fill_pathname_resolve_relative(pass->source.path,
         conf->path, tmp_path, sizeof(pass->source.path));

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
   snprintf(wrap_name_buf,
         sizeof(wrap_name_buf), "wrap_mode%u", i);
   if ((entry = config_get_entry(conf, wrap_name_buf))
            && !string_is_empty(entry->value))
      pass->wrap = wrap_str_to_mode(entry->value);
   entry = NULL;

   /* Frame count mod */
   snprintf(frame_count_mod_buf,
         sizeof(frame_count_mod_buf), "frame_count_mod%u", i);
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
         RARCH_ERR("[ Shaders ]:  Invalid attribute.\n");
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
         RARCH_ERR("[ Shaders ]:  Invalid attribute.\n");
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
      char id_mipmap[64];
      bool mipmap         = false;
      bool smooth         = false;
      struct config_entry_list 
         *entry           = NULL;

      id_filter[0] = id_wrap[0] = id_mipmap[0] = '\0';

      if (!(entry = config_get_entry(conf, id)) ||
            string_is_empty(entry->value))
      {
         RARCH_ERR("[ Shaders ]:  Cannot find path to texture \"%s\" ...\n", id);
         free(textures);
         return false;
      }

      fill_pathname_resolve_relative(shader->lut[shader->luts].path,
            conf->path, entry->value, sizeof(shader->lut[shader->luts].path));
      entry = NULL;

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
 * video_shader_set_current_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * For each parameter in the shader, if a value is set in the config file
 * load this value to the parameter's current value.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_resolve_current_parameters(config_file_t *conf,
      struct video_shader *shader)
{
   unsigned i;
   const struct config_entry_list *entry = NULL;

   if (!conf)
      return false;

   /* For all parameters in the shader see if there is any config value set */
   for (i = 0; i < shader->num_parameters; i++)
   {
      entry = config_get_entry(conf, shader->parameters[i].id);
      
      /* Only try to load the parameter value if an entry exists in the config */
      if (entry)
      {
         struct video_shader_parameter *parameter =
            (struct video_shader_parameter*)
            video_shader_parse_find_parameter(
                  shader->parameters, shader->num_parameters, shader->parameters[i].id);

         if (config_get_float(conf, shader->parameters[i].id, &parameter->current))
            RARCH_LOG("[ Shaders - Load Parameter Values]:  %s = %f.\n", 
                  shader->parameters[i].id, parameter->current);
         else
            RARCH_WARN("[ Shaders - Load Parameter Values]:  Name %s is set in preset "
                       "but couldn't load its value.\n", shader->parameters[i].id);
      }
   }

   return true;
}

/**
 * video_shader_resolve_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Resolves all shader parameters belonging to shaders.
 * Fills the parameter definition list of the shader
 * then calls video_shader_resolve_current_parameters to
 * read in the config parameter values
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
      struct string_list lines  = {0};
      size_t line_index         = 0;
      bool lines_inited         = false;

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
      {
         string_list_initialize(&lines);
         lines_inited = string_split_noalloc(&lines, (const char*)buf, "\n");
      }

      /* Buffer is no longer required - clean up */
      if ((void*)buf)
         free((void*)buf);

      if (!lines_inited)
         continue;

      /* even though the pass is set in the loop too, not all passes have parameters */
      param->pass = i;

      while ((shader->num_parameters < ARRAY_SIZE(shader->parameters)) &&
             (line_index < lines.size))
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

         RARCH_LOG("[ Shaders ]: Found #pragma parameter %s (%s) %f %f %f %f in pass %d\n",
               param->desc,    param->id,      param->initial,
               param->minimum, param->maximum, param->step, param->pass);
         param->current  = param->initial;

         shader->num_parameters++;
         param++;
      }

      string_list_deinitialize(&lines);
   }

   return video_shader_resolve_current_parameters(conf, shader);
}

#ifdef _WIN32
static void make_relative_path_portable(char* path)
{
   /* use '/' instead of '\' for maximum portability */
   char* p;
   for (p = path; *p; p++)
      if (*p == '\\')
         *p = '/';
}
#endif

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
bool video_shader_check_reference_chain_for_save(const char *path_to_save, const char *reference_path)
{
   config_file_t *conf           = config_file_new_from_path_to_string(reference_path);
   char* nested_reference_path   = (char*)malloc(PATH_MAX_LENGTH);
   bool return_val               = true;

   if (!conf)
   {
      return_val = false;
   }
   else
   {
      int reference_depth = 1;

      while (conf->reference)
      {
         /* If we have reached the max depth of nested references stop attempting to read 
          * the next reference because we are likely in a self referential loop. 
          * 16 references deep seems like more than enough depth for expected usage */
         if (reference_depth > 16)
         {
            RARCH_ERR("[ Shaders - Save Simple Preset ]:  Exceeded maximum reference "
                        "depth(16) without finding a full preset. "
                        "This chain of referenced presets is likely cyclical.\n");
            return_val = false;
            break;
         }

         /* Resolve the reference path relative to the config */
         if (path_is_absolute(conf->reference))
            strlcpy(nested_reference_path, conf->reference, PATH_MAX_LENGTH);
         else
            fill_pathname_resolve_relative(nested_reference_path,
                                             conf->path,
                                             conf->reference,
                                             PATH_MAX_LENGTH);

         /* If one of the reference paths is the same as the file we want to save then
          * this reference chain would be self-referential / cyclical and we can't save
          * this as a simple preset*/
         if (string_is_equal(nested_reference_path, path_to_save))
         {
            RARCH_WARN("[ Shaders - Save Simple Preset ]:  Saving preset:\n"
                       "                                              %s\n"
                       "                                          With a #reference of:\n"
                       "                                              %s\n"
                       "                                          Would create a cyclical reference in preset:\n"
                       "                                              %s\n"
                       "                                          Which already references preset:\n"
                       "                                              %s\n\n",
                       path_to_save, reference_path, conf->path, nested_reference_path);
            return_val = false;
            break;
         }

         /* Create a new config from the referenced path */
         config_file_free(conf);
         conf = config_file_new_from_path_to_string(nested_reference_path);

         /* If we can't read the reference preset */
         if (!conf)
         {
            RARCH_WARN("[ Shaders - Save Simple Preset ]:  Could not read shader preset "
                        "in #reference line: %s\n", nested_reference_path);
            return_val = false;
            break;
         }

         reference_depth += 1;
      }
   }

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
 *    as it's first line to specify a root preset and can also include parameter 
 *    and texture values to override the values of the root preset
 * Returns false if a referenced preset cannot be saved
 * See: video_shader_read_preset
 **/
bool video_shader_write_referenced_preset(const char *path,
                                          const char *shader_dir,
                                          const struct video_shader *shader)
{
   unsigned i;
   config_file_t *conf                    = NULL;
   config_file_t *reference_conf          = NULL;
   bool ret                               = false;
   bool continue_saving_reference         = true;
   char *new_preset_basedir               = strdup(path);
   char *config_dir                       = (char*)malloc(PATH_MAX_LENGTH);
   char *relative_temp_reference_path     = (char*)malloc(PATH_MAX_LENGTH);
   char *abs_temp_reference_path          = (char*)malloc(PATH_MAX_LENGTH);
   char *path_to_reference                = (char*)malloc(PATH_MAX_LENGTH);
   
   config_dir[0]                          = '\0';
   relative_temp_reference_path[0]        = '\0';
   abs_temp_reference_path[0]             = '\0';
   path_to_reference[0]                   = '\0';

   path_basedir(new_preset_basedir);

   /* Get the retroarch config dir where the automatically loaded presets are located
    * and where Save Game Preset, Save Core Preset, Save Global Preset save to */
   fill_pathname_application_special(config_dir,
                                     PATH_MAX_LENGTH, 
                                     APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* If there is no initial preset path loaded */
   if (!shader->loaded_preset_path)
   {
         RARCH_WARN("[ Shaders - Save Simple Preset ]: Saving Full Preset because the "
                     "loaded Shader does not have a path to a previously loaded preset "
                     "file on disk.\n");
         continue_saving_reference = false;
         goto end;
   }
   
   /* If the initial preset loaded is the ever-changing retroarch preset don't save a reference
   * TODO remove once we don't write this preset anymore */
   if (!strncmp(path_basename(shader->loaded_preset_path), "retroarch", STRLEN_CONST("retroarch")))
   {
      continue_saving_reference = false;
      RARCH_WARN("[ Shaders - Save Simple Preset ]: Saving Full Preset because we can't "
                 "save a reference to the ever-changing retroarch preset.\n");
      continue_saving_reference = false;
      goto end;
   }

   strlcpy(path_to_reference, shader->loaded_preset_path, PATH_MAX_LENGTH);

   /* Get a config from the file we want to make a reference to */
   reference_conf = config_file_new_from_path_to_string(path_to_reference);

   /* If the original preset can't be loaded, probably because it isn't there anymore */
   if (!reference_conf)
   {
      RARCH_WARN("[ Shaders - Save Simple Preset ]: Saving Full Preset because the "
                  "initially loaded preset can't be loaded. It was likely "
                  "renamed or deleted.\n");
      continue_saving_reference = false;
      goto end;
   }

   /* If we are trying to save on top the path referenced in the initially loaded preset.
    * E.G. Preset_B references Preset_A, I load Preset_B do some parameter adjustments, 
    *       then I save on top of Preset_A, we want to get a preset just like the 
    *       original Preset_A with the new parameter adjustments
    * If there is a reference in the initially loaded preset we should check it against 
    * the preset path we are currently trying to save */
   if (reference_conf->reference)
   {
      /* Get the absolute reference path in the initially loaded preset file*/
      if (!path_is_absolute(reference_conf->reference))
         fill_pathname_resolve_relative(abs_temp_reference_path, 
                                          reference_conf->path, 
                                          reference_conf->reference, 
                                          PATH_MAX_LENGTH);
      else
         abs_temp_reference_path = strdup(reference_conf->reference);

      /* If the reference is the same as the path we are trying to save to 
         then this should be used as the reference to save */
      if (string_is_equal(abs_temp_reference_path, path))
      {
         strlcpy(path_to_reference, abs_temp_reference_path, PATH_MAX_LENGTH);
         config_file_free(reference_conf);
         reference_conf = config_file_new_from_path_to_string(path_to_reference);
      }
   }

   /* If 
    *    The new preset file we are trying to save is the same as the initially loaded preset
    * or
    *    The initially loaded preset was located under the retroarch config folder
    *    this means that it was likely saved from inside the retroarch UI
    * Then
    *    We should not save a preset with a reference to the initially loaded
    *    preset file itself, instead we need to save a new preset with the same reference 
    *    as was in the initially loaded preset.
    */

   /* If the reference path is the same as the path we want to save or the reference 
    * path is in the config (auto shader) folder */
   if (string_is_equal(path_to_reference, path) || 
      !strncmp(config_dir, path_to_reference, strlen(config_dir)))
   {
      /* If the config from the reference path has a reference in it we will use this same 
       * nested reference for the new preset */
      if (reference_conf->reference)
      {
         /* Get the absolute path for the reference */
         if (!path_is_absolute(reference_conf->reference))
            fill_pathname_resolve_relative(path_to_reference, 
                                             reference_conf->path, 
                                             reference_conf->reference, 
                                             PATH_MAX_LENGTH);
         else
            strlcpy(path_to_reference, reference_conf->reference, PATH_MAX_LENGTH);

         /* If the reference path is also the same as what we are trying to save 
            This can easily happen
            E.G.
            - Save Preset As
            - Save Game Preset
            - Save Preset As (use same name as first time)
         */
         if (string_is_equal(path_to_reference, path))
         {
            config_file_free(reference_conf);
            reference_conf = config_file_new_from_path_to_string(path_to_reference);

            /* If the reference also has a reference inside it */
            if (reference_conf->reference)
            {
               /* Get the absolute path for the reference */
               if (!path_is_absolute(reference_conf->reference))
                  fill_pathname_resolve_relative(path_to_reference, 
                                                   reference_conf->path, 
                                                   reference_conf->reference, 
                                                   PATH_MAX_LENGTH);
               else
                  strlcpy(path_to_reference, reference_conf->reference, PATH_MAX_LENGTH);
            }
            /* If the config referenced is a full preset */
            else
            {
               RARCH_WARN("[ Shaders - Save Simple Preset ]: Saving Full Preset because we "
                           "can't save a preset which would reference itself.\n");
               continue_saving_reference = false;
               goto end;
            }
         }
      }
      /* If there is no reference in the initial preset we need to save a full preset */
      else
      {
         /* We can't save a reference to ourselves */
         RARCH_WARN("[ Shaders - Save Simple Preset ]: Saving Full Preset because we "
                     "can't save a preset which would reference itself.\n");
         continue_saving_reference = false;
         goto end;
      }
   }

   /* Check the reference chain that we would be saving to make sure it is valid */
   if (!video_shader_check_reference_chain_for_save(path, path_to_reference))
   {
      RARCH_WARN("[ Shaders - Save Simple Preset ]: Saving Full Preset because "
                 "saving a Simple Preset would result in a cyclical reference, "
                 "or a preset in the reference chain could not be read.\n");
      continue_saving_reference = false;
      goto end;
   }

   if (continue_saving_reference)
   {

      path_relative_to(relative_temp_reference_path, 
                         path_to_reference, 
                         new_preset_basedir,
                         PATH_MAX_LENGTH);
#ifdef _WIN32
       if (!path_is_absolute(relative_temp_reference_path))
          make_relative_path_portable(relative_temp_reference_path);
#endif

      /* Create a new EMPTY config */
      conf = config_file_new_alloc();
      if (!(conf))
         goto end;
      conf->path = strdup(path);

      /* Add the reference path to the config */
      config_file_set_reference_path(conf, relative_temp_reference_path);

      /* Set modified to true so when you run config_file_write it will save a file */
      conf->modified = true;

      /* Get a config from the #reference line in the preset 
       * We will compare the current shader against this config */
      config_file_free(reference_conf);
      reference_conf = video_shader_read_preset(path_to_reference);

      /* reference_conf could be NULL if the file was not found or if the 
       * chain of references was too deep */
      if (reference_conf == NULL)
      {
         RARCH_WARN("[ Shaders - Save Simple Preset ]:  Saving Full Preset because we "
                     "could not load the preset from the #reference line: %s.\n", 
                     path_to_reference);
         continue_saving_reference = false;
      }
      else
      {
         /* 
            Compare the shader to a shader created from the referenced config to see if  
            we can save a referenced preset and what parameters and textures of the 
            root_config are overridden
         */

         struct video_shader *root_shader = NULL;
         root_shader          = (struct video_shader*) calloc(1, sizeof(*root_shader));
         video_shader_read_conf_preset(reference_conf, root_shader);

         /* Check number of passes match */
         if (shader->passes != root_shader->passes)
         {
            RARCH_WARN("[ Shaders - Save Simple Preset ]: passes (Number of Passes) "
                        "Current Value doesn't match Referenced Value - Full Preset "
                        "will be Saved instead of Simple Preset\n");
            continue_saving_reference = false;
         }

         /*
            Compare all passes from the shader
            if anything is different then we should not save a reference 
            and save instead safe a full preset instead
         */
         if (continue_saving_reference)
         {
            /* Step through each pass comparing all the properties to make sure they match */
            for (i = 0; (i < shader->passes && continue_saving_reference == true); i++)
            {
               const struct video_shader_pass *pass = &shader->pass[i];
               const struct video_shader_pass *root_pass = &root_shader->pass[i];
               const struct gfx_fbo_scale *fbo = &pass->fbo;
               const struct gfx_fbo_scale *root_fbo = &root_pass->fbo;

               if (!string_is_equal(pass->source.path, root_pass->source.path))
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u path", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && pass->filter != root_pass->filter)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u filter", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && pass->wrap != root_pass->wrap)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u wrap", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && pass->frame_count_mod != root_pass->frame_count_mod)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u frame_count", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && pass->mipmap != root_pass->mipmap)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u mipmap", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && !string_is_equal(pass->alias, root_pass->alias))
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u alias", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->type_x != root_fbo->type_x)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u type_x", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->type_y != root_fbo->type_y)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u type_y", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->scale_x != root_fbo->scale_x)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u scale_x", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->scale_y != root_fbo->scale_y)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u scale_y", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->fp_fbo != root_fbo->fp_fbo)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u fp_fbo", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->srgb_fbo != root_fbo->srgb_fbo)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u srgb_fbo", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->valid != root_fbo->valid)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u valid", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->abs_x != root_fbo->abs_x)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u abs_x", i);
                  continue_saving_reference = false;
               }

               if (continue_saving_reference && fbo->abs_y != root_fbo->abs_y)
               {
                  RARCH_WARN("[ Shaders - Save Simple Preset ]: Pass %u abs_y", i);
                  continue_saving_reference = false;
               }

               if (!continue_saving_reference)
                  RARCH_WARN(" Current Value doesn't match Referenced Value - "
                             "Full Preset Will be Saved instead of Simple Preset\n");
            }
         }

         if (continue_saving_reference)
         {
            const struct config_entry_list *entry = NULL;

            /* If the shader has parameters */
            if (shader->num_parameters)
            {
               unsigned i;
               float parameter_value_reference = 0.0f;


               for (i = 0; i < shader->num_parameters; i++)
               {
                  bool add_param_to_override = false;
                  
                  entry = config_get_entry(reference_conf, shader->parameters[i].id);

                  /* If the parameter is in the reference config */
                  if (entry)
                  {
                     /* If the current param value is different than the referenced preset's value */
                     config_get_float(reference_conf, shader->parameters[i].id, &parameter_value_reference);
                     if (shader->parameters[i].current != parameter_value_reference)
                        add_param_to_override = true;
                  }
                  /* If it's not in the reference config, but it's different than the 
                     initial value of the shader */
                  else if (shader->parameters[i].current != shader->parameters[i].initial)
                     add_param_to_override = true;

                  /* Add the parameter value to the config */ 
                  if (add_param_to_override)
                     config_set_float(conf, shader->parameters[i].id, shader->parameters[i].current);
               }
            }

            /* If the shader has textures */
            if (shader->luts)
            {
               char *shader_tex_path              = (char*)malloc(PATH_MAX_LENGTH);
               char *shader_tex_relative_path     = (char*)malloc(PATH_MAX_LENGTH);
               char *shader_tex_base_path         = (char*)malloc(PATH_MAX_LENGTH);
               char *referenced_tex_absolute_path = (char*)malloc(PATH_MAX_LENGTH);
               char *referenced_tex_path          = (char*)malloc(PATH_MAX_LENGTH);
               unsigned i;

               shader_tex_path[0]              = '\0';
               shader_tex_relative_path[0]     = '\0';
               shader_tex_base_path[0]         = '\0';
               referenced_tex_absolute_path[0] = '\0';
               referenced_tex_path[0]          = '\0';

               for (i = 0; i < shader->luts; i++)
               {
                  /* If the texture is defined in the reference config */
                  entry = config_get_entry(reference_conf, shader->lut[i].id);
                  if (entry)
                  {
                     /* Texture path from shader is already absolute */
                     strlcpy(shader_tex_path, shader->lut[i].path, PATH_MAX_LENGTH);
                     strlcpy(referenced_tex_path, entry->value, PATH_MAX_LENGTH);

                     /* Resolve the texture's path relative to the override config */
                     if (!path_is_absolute(referenced_tex_path))
                        fill_pathname_resolve_relative(referenced_tex_absolute_path, 
                                                         reference_conf->path, 
                                                         entry->value, 
                                                         PATH_MAX_LENGTH);
                     else
                        strlcpy(referenced_tex_absolute_path, referenced_tex_path, PATH_MAX_LENGTH);

                     /* If the current shader texture path is different than the referenced paths then 
                      * write the current path into the new preset */
                     if (!string_is_equal(referenced_tex_absolute_path, shader->lut[i].path))
                     {
                        /* Get the texture path relative to the new preset */
                        path_relative_to(shader_tex_relative_path, shader_tex_path, path_basename(path), PATH_MAX_LENGTH);

                        RARCH_LOG("[ Shaders - Save Simple Preset ]:  Texture override %s = %s.\n", 
                                 shader->lut[i].id, 
                                 shader->lut[i].path);
                        config_set_path(conf, shader->lut[i].id, shader->lut[i].path);
                     }
                  }
               }

               free(shader_tex_path);
               free(shader_tex_relative_path);
               free(shader_tex_base_path);
               free(referenced_tex_absolute_path);
               free(referenced_tex_path);
            }
            /* Write the file, return will be true if successful */
            RARCH_LOG("[ Shaders - Save Simple Preset ]:  Saving simple preset to: %s\n", path);
            ret = config_file_write(conf, path, false);
         }
         free(root_shader);
      }
   }

end:

   config_file_free(conf);
   config_file_free(reference_conf);
   free(abs_temp_reference_path);
   free(relative_temp_reference_path);
   free(new_preset_basedir);
   free(config_dir);
   free(path_to_reference);

   return ret;
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
      const char *shader_dir,
      const struct video_shader *shader, bool reference)
{
   /* We need to clean up paths to be able to properly process them
    * path and shader->loaded_preset_path can use '/' on Windows due to Qt being Qt */
   char preset_dir[PATH_MAX_LENGTH];
   config_file_t *conf;
   bool ret;

   if (!shader || string_is_empty(path))
      return false;

   fill_pathname_join(
      preset_dir,
      shader_dir,
      "presets",
      sizeof(preset_dir));

   /* If we should still save a referenced preset do it now */
   if (reference)
   {
      if (video_shader_write_referenced_preset(path, shader_dir, shader))
      {
         return true;
      }
      else
      {
         RARCH_WARN("[ Shaders - Save Simple Preset ]:  Failed writing Simple "
                     "Preset to %s - Full Preset Will be Saved instead.\n", path);
      }
   }

   /* If we aren't saving a referenced preset or weren't able to save one
    * then save a full preset */

   /* Note: We always create a new/blank config
      * file here. Loading and updating an existing
      * file could leave us with unwanted/invalid
      * parameters. */
   if (!(conf = config_file_new_alloc()))
      return false;

   video_shader_write_conf_preset(conf, shader, path);

   ret = config_file_write(conf, path, false);

   config_file_free(conf);

   return ret;
}

/**
 * override_config_values:
 * @conf              : Config file to be affected
 * @override_conf     : Config file who's values will be copied on top of conf
 *
 * Takes values from override_config and overrides values of conf
 * The 'parameters' value will be the combined parameter list from both configs
 *
 * Returns 0 if nothing is overridden 
 * Returns 1 if something is overridden
 **/
bool override_config_values(config_file_t *conf, config_file_t *override_conf)
{
   int return_val                = 0;
   size_t param_size             = 4096 * sizeof(char);
   const char *id                = NULL;
   char *save                    = NULL;
   size_t path_size              = PATH_MAX_LENGTH;
   char *override_texture_path   = (char*)malloc(path_size);
   char *resolved_path           = (char*)malloc(path_size);
   char *textures_in_conf        = (char*)malloc(param_size);
   size_t tmp_size               = PATH_MAX_LENGTH;
   char *tmp                     = (char*)malloc(3*tmp_size);
   char *tmp_rel                 = tmp + tmp_size;
   char *tmp_base                = tmp + 2*tmp_size;
   struct config_entry_list *override_entry    = NULL;
   
   textures_in_conf[0]           = '\0';
   strlcpy(tmp_base, conf->path, tmp_size);

   if (conf == NULL || override_conf == NULL) return 0;

   /* ---------------------------------------------------------------------------------
    * ------------- Resolve Override texture paths to absolute paths-------------------
    * --------------------------------------------------------------------------------- */

   /* ensure we use a clean base like the shader passes and texture paths do */
   path_resolve_realpath(tmp_base, tmp_size, false);
   path_basedir(tmp_base);

   /* If there are textures in the referenced config */
   if (config_get_array(conf, "textures", textures_in_conf, param_size))
   {
      for ( id = strtok_r(textures_in_conf, ";", &save);
            id;
            id = strtok_r(NULL, ";", &save))
      {
         /* Get the texture path from the override config */
         if (config_get_path(override_conf, id, override_texture_path, path_size))
         {
            /* Resolve the texture's path relative to the override config */
            if (!path_is_absolute(override_texture_path))
               fill_pathname_resolve_relative(resolved_path,
                     override_conf->path,
                     override_texture_path,
                     PATH_MAX_LENGTH);
            else
               strlcpy(resolved_path, override_texture_path, path_size);

            path_relative_to(tmp_rel, resolved_path, tmp_base, tmp_size);
            config_set_path(override_conf, id, tmp_rel);

            return_val = 1;
         }
      }
   }

   /* ---------------------------------------------------------------------------------
    * ------------- Update entries to match the override entries ----------------------
    * --------------------------------------------------------------------------------- */

   for (override_entry = override_conf->entries; override_entry; override_entry = override_entry->next)
      /* Only override an entry if the its key is not "textures" */
      if (!string_is_empty(override_entry->key) && !string_is_equal(override_entry->key, "textures"))
      {
         RARCH_LOG("[ Shaders - Read Simple Preset ]:    Parameter/Entry overridden %s = %s.\n",
                     override_entry->key, override_entry->value);
         config_set_string(conf, override_entry->key, override_entry->value);
         return_val = 1;
      }

   free(tmp);
   free(resolved_path);
   free(override_texture_path);
   free(textures_in_conf);

   return return_val;
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
   conf = config_file_new_from_path_to_string(path);

   if (conf != NULL)
   {
      int reference_depth = 1;

      if (conf->reference)
         RARCH_LOG("[ Shaders - Read Preset ]:  Start reading SIMPLE Preset: %s\n", path);
      else
         RARCH_LOG("[ Shaders - Read Preset ]:  Start reading FULL Preset: %s\n", path);
      
      /* If the config has a reference then it is really an override config. 
       * We now load a new config from the reference then override it's values 
       * with the override config */
      while (conf->reference)
      {
         char* reference_preset_path = (char*)malloc(PATH_MAX_LENGTH);
         /* Set override_conf to refer to the original config */
         config_file_t *override_conf = conf;

         /* Stop attempting to read the next reference when we have visited many links 
          * in the reference chain because we are likely in a self referential loop. 
          * 16 references deep seems like more than enough depth for expected usage */
         if (reference_depth > 16)
         {
            RARCH_ERR("[ Shaders - Read Simple Preset ]:  Exceeded maximum reference "
                        "depth(16) without finding a full preset\n");
            free(reference_preset_path);
            config_file_free(conf);
            conf = NULL;
            return NULL;
         }

         /* Resolve the reference path relative to the config */
         if (path_is_absolute(conf->reference))
            strlcpy(reference_preset_path, conf->reference, PATH_MAX_LENGTH);
         else
            fill_pathname_resolve_relative(reference_preset_path,
                                             conf->path,
                                             conf->reference,
                                             PATH_MAX_LENGTH);

         RARCH_LOG("[ Shaders - Read Simple Preset ]:  #reference preset read "
                  "(Depth %u): %s\n", reference_depth, reference_preset_path);

         /* Create a new config from the root preset */
         conf = config_file_new_from_path_to_string(reference_preset_path);
         
         /* Only try to override values if the config is not NULL
          * If it is NULL there is no shader*/
         if (conf != NULL)
            /* override_conf is from the initial file we loaded which
             * has the #reference directive*/
            override_config_values(conf, override_conf);
         else
         {
            RARCH_WARN("[ Shaders - Read Simple Preset ]:  Could not read shader preset "
                        "in #reference line: %s\n", reference_preset_path);
            free(reference_preset_path);
            config_file_free(override_conf);
            break;
         }

         reference_depth += 1;

         free(reference_preset_path);
         config_file_free(override_conf);
      }

      /* Set Path for originally loaded preset */
      config_set_path(conf, "loaded_preset_path", path);
   }
   else
      RARCH_WARN("[ Shaders - Read Preset ]:  Could not read preset: %s \n", path);

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
   unsigned shaders                 = 0;
   settings_t *settings             = config_get_ptr();
   bool watch_files                 = settings->bools.video_shader_watch_files;
   char loaded_preset_path[PATH_MAX_LENGTH];

   memset(shader, 0, sizeof(*shader));

   if (!config_get_uint(conf, "shaders", &shaders))
   {
      RARCH_ERR("[ Shaders - Read Preset ]:  Cannot find \"shaders\" param. "
                  "Preset is invalid.\n");
      return false;
   }

   if (!shaders)
   {
      RARCH_ERR("[ Shaders - Read Preset ]:  Need to define at least 1 shader pass. "
                  "Preset is invalid.\n");
      return false;
   }

   if (!config_get_int(conf, "feedback_pass",
            &shader->feedback_pass))
      shader->feedback_pass = -1;

   shader->passes = MIN(shaders, GFX_MAX_SHADERS);

   /* Set the path of the root preset for this shader */
   strlcpy(shader->path, conf->path, sizeof(shader->path));
   
   /* Set the path of the original preset which was loaded, this would be 
    * different than the root preset in the case preset of use of the #reference directive
    * in the original preset loaded */
   config_get_path(conf, "loaded_preset_path", loaded_preset_path, PATH_MAX_LENGTH);
   strlcpy( shader->loaded_preset_path, 
            loaded_preset_path,
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
         frontend_driver_watch_path_for_changes(NULL,
               0, &file_change_data);

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

         string_list_append(&file_list,
               shader->pass[i].source.path, attr);
      }

      frontend_driver_watch_path_for_changes(&file_list,
            flags, &file_change_data);
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

   return video_shader_parse_textures(conf, shader);
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

   RARCH_LOG("[ Shaders - Save Full Preset ]:  Saving full preset to: %s\n", preset_path);

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
#ifdef _WIN32
         if (!path_is_absolute(tmp_rel))
            make_relative_path_portable(tmp_rel);
#endif

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

   /* Write shader parameters which are different than the default 
    * shader values */
   if (shader->num_parameters)
      for (i = 0; i < shader->num_parameters; i++)
         if (shader->parameters[i].current != shader->parameters[i].initial)
            config_set_float(conf, shader->parameters[i].id,
                  shader->parameters[i].current);

   if (shader->luts)
   {
      char textures[4096];

      textures[0] = '\0';

      strlcpy(textures, shader->lut[0].id, sizeof(textures));

      for (i = 1; i < shader->luts; i++)
      {
         /* O(n^2), but number of textures is very limited. */
         strlcat(textures, ";", sizeof(textures));
         strlcat(textures, shader->lut[i].id, sizeof(textures));
      }

      config_set_string(conf, "textures", textures);

      for (i = 0; i < shader->luts; i++)
      {
         if (preset_path)
         {
            strlcpy(tmp, shader->lut[i].path, tmp_size);
            path_relative_to(tmp_rel, tmp, tmp_base, tmp_size);
#ifdef _WIN32
            if (!path_is_absolute(tmp_rel))
               make_relative_path_portable(tmp_rel);
#endif

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

bool video_shader_check_for_changes(void)
{
   if (!file_change_data)
      return false;

   return frontend_driver_check_for_path_changes(file_change_data);
}
