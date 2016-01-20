/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <compat/posix_string.h>
#include <compat/msvc.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <rhash.h>
#include <string/stdstring.h>

#include "../general.h"
#include "../verbosity.h"
#include "video_shader_parse.h"

#define WRAP_MODE_CLAMP_TO_BORDER      0x3676ed11U
#define WRAP_MODE_CLAMP_TO_EDGE        0x9427a608U
#define WRAP_MODE_REPEAT               0x192dec66U
#define WRAP_MODE_MIRRORED_REPEAT      0x117ac9a9U

#define SCALE_TYPE_SOURCE              0x1c3aff76U
#define SCALE_TYPE_VIEWPORT            0xe8f01225U
#define SCALE_TYPE_ABSOLUTE            0x8cc74f64U


#define SEMANTIC_CAPTURE               0xb2f5d639U
#define SEMANTIC_CAPTURE_PREVIOUS      0x64d6d495U
#define SEMANTIC_TRANSITION            0x96486f70U
#define SEMANTIC_TRANSITION_PREVIOUS   0x536abbacU
#define SEMANTIC_TRANSITION_COUNT      0x3ef2af78U
#define SEMANTIC_PYTHON                0x15efc547U

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
   uint32_t wrap_mode_hash = djb2_calculate(wrap_mode);

   switch (wrap_mode_hash)
   {
      case WRAP_MODE_CLAMP_TO_BORDER:
         return RARCH_WRAP_BORDER;
      case WRAP_MODE_CLAMP_TO_EDGE:
         return RARCH_WRAP_EDGE;
      case WRAP_MODE_REPEAT:
         return RARCH_WRAP_REPEAT;
      case WRAP_MODE_MIRRORED_REPEAT:
         return RARCH_WRAP_MIRRORED_REPEAT;
   }

   RARCH_WARN("Invalid wrapping type %s. Valid ones are: clamp_to_border (default), clamp_to_edge, repeat and mirrored_repeat. Falling back to default.\n",
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
static bool video_shader_parse_pass(config_file_t *conf, struct video_shader_pass *pass, unsigned i)
{
   char shader_name[64]         = {0};
   char filter_name_buf[64]     = {0};
   char wrap_name_buf[64]       = {0};
   char wrap_mode[64]           = {0};
   char frame_count_mod_buf[64] = {0};
   char srgb_output_buf[64]     = {0};
   char fp_fbo_buf[64]          = {0};
   char mipmap_buf[64]          = {0};
   char alias_buf[64]           = {0};
   char scale_name_buf[64]      = {0};
   char attr_name_buf[64]       = {0};
   char scale_type[64]          = {0};
   char scale_type_x[64]        = {0};
   char scale_type_y[64]        = {0};
   char frame_count_mod[64]     = {0};
   struct gfx_fbo_scale *scale  = NULL;
   bool smooth                  = false;
   float fattr                  = 0.0f;
   int iattr                    = 0;

   /* Source */
   snprintf(shader_name, sizeof(shader_name), "shader%u", i);
   if (!config_get_path(conf, shader_name, pass->source.path, sizeof(pass->source.path)))
   {
      RARCH_ERR("Couldn't parse shader source (%s).\n", shader_name);
      return false;
   }
   
   /* Smooth */
   snprintf(filter_name_buf, sizeof(filter_name_buf), "filter_linear%u", i);
   if (config_get_bool(conf, filter_name_buf, &smooth))
      pass->filter = smooth ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
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
      pass->frame_count_mod = strtoul(frame_count_mod, NULL, 0);

   /* FBO types and mipmapping */
   snprintf(srgb_output_buf, sizeof(srgb_output_buf), "srgb_framebuffer%u", i);
   config_get_bool(conf, srgb_output_buf, &pass->fbo.srgb_fbo);

   snprintf(fp_fbo_buf, sizeof(fp_fbo_buf), "float_framebuffer%u", i);
   config_get_bool(conf, fp_fbo_buf, &pass->fbo.fp_fbo);

   snprintf(mipmap_buf, sizeof(mipmap_buf), "mipmap_input%u", i);
   config_get_bool(conf, mipmap_buf, &pass->mipmap);

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
      uint32_t scale_type_x_hash = djb2_calculate(scale_type_x);

      switch (scale_type_x_hash)
      {
         case SCALE_TYPE_SOURCE:
            scale->type_x = RARCH_SCALE_INPUT;
            break;
         case SCALE_TYPE_VIEWPORT:
            scale->type_x = RARCH_SCALE_VIEWPORT;
            break;
         case SCALE_TYPE_ABSOLUTE:
            scale->type_x = RARCH_SCALE_ABSOLUTE;
            break;
         default:
            RARCH_ERR("Invalid attribute.\n");
            return false;
      }
   }

   if (*scale_type_y)
   {
      uint32_t scale_type_y_hash = djb2_calculate(scale_type_y);

      switch (scale_type_y_hash)
      {
         case SCALE_TYPE_SOURCE:
            scale->type_y = RARCH_SCALE_INPUT;
            break;
         case SCALE_TYPE_VIEWPORT:
            scale->type_y = RARCH_SCALE_VIEWPORT;
            break;
         case SCALE_TYPE_ABSOLUTE:
            scale->type_y = RARCH_SCALE_ABSOLUTE;
            break;
         default:
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
   const char *id       = NULL;
   char *save           = NULL;
   char textures[1024]  = {0};

   if (!config_get_array(conf, "textures", textures, sizeof(textures)))
      return true;

   for (id = strtok_r(textures, ";", &save);
         id && shader->luts < GFX_MAX_TEXTURES;
         shader->luts++, id = strtok_r(NULL, ";", &save))
   {
      char id_filter[64]  = {0};
      char id_wrap[64]    = {0};
      char wrap_mode[64]  = {0};
      char id_mipmap[64]  = {0};
      bool mipmap         = false;
      bool smooth         = false;

      if (!config_get_array(conf, id, shader->lut[shader->luts].path,
               sizeof(shader->lut[shader->luts].path)))
      {
         RARCH_ERR("Cannot find path to texture \"%s\" ...\n", id);
         return false;
      }

      strlcpy(shader->lut[shader->luts].id, id,
            sizeof(shader->lut[shader->luts].id));

      snprintf(id_filter, sizeof(id_filter), "%s_linear", id);
      if (config_get_bool(conf, id_filter, &smooth))
         shader->lut[shader->luts].filter = smooth ? 
            RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
      else
         shader->lut[shader->luts].filter = RARCH_FILTER_UNSPEC;

      snprintf(id_wrap, sizeof(id_wrap), "%s_wrap_mode", id);
      if (config_get_array(conf, id_wrap, wrap_mode, sizeof(wrap_mode)))
         shader->lut[shader->luts].wrap = wrap_str_to_mode(wrap_mode);

      snprintf(id_mipmap, sizeof(id_mipmap), "%s_mipmap", id);
      if (config_get_bool(conf, id_mipmap, &mipmap))
         shader->lut[shader->luts].mipmap = mipmap;
      else
         shader->lut[shader->luts].mipmap = false;
   }

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
      struct video_shader_parameter *params, unsigned num_params, const char *id)
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
 * Resolves all shader parameters belonging to shaders. 
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_resolve_parameters(config_file_t *conf,
      struct video_shader *shader)
{
   unsigned i;
   struct video_shader_parameter *param = 
      &shader->parameters[shader->num_parameters];

   shader->num_parameters = 0;

   /* Find all parameters in our shaders. */

   for (i = 0; i < shader->passes; i++)
   {
      char line[4096] = {0};
      FILE *file = fopen(shader->pass[i].source.path, "r");

      if (!file)
         continue;

      while (shader->num_parameters < ARRAY_SIZE(shader->parameters)
            && fgets(line, sizeof(line), file))
      {
         int ret = sscanf(line,
               "#pragma parameter %63s \"%63[^\"]\" %f %f %f %f",
               param->id, param->desc, &param->initial,
               &param->minimum, &param->maximum, &param->step);

         if (ret < 5)
            continue;

         param->id[63]   = '\0';
         param->desc[63] = '\0';

         if (ret == 5)
            param->step = 0.1f * (param->maximum - param->minimum);

         RARCH_LOG("Found #pragma parameter %s (%s) %f %f %f %f\n",
               param->desc, param->id, param->initial,
               param->minimum, param->maximum, param->step);
         param->current = param->initial;

         shader->num_parameters++;
         param++;
      }

      fclose(file);
   }

   if (conf)
   {
      /* Read in parameters which override the defaults. */
      char parameters[4096] = {0};
      const char *id        = NULL;
      char *save            = NULL;

      if (!config_get_array(conf, "parameters",
               parameters, sizeof(parameters)))
         return true;

      for (id = strtok_r(parameters, ";", &save); id; 
            id = strtok_r(NULL, ";", &save))
      {
         struct video_shader_parameter *parameter = (struct video_shader_parameter*)
            video_shader_parse_find_parameter(shader->parameters, shader->num_parameters, id);

         if (!parameter)
         {
            RARCH_WARN("[CGP/GLSLP]: Parameter %s is set in the preset, but no shader uses this parameter, ignoring.\n", id);
            continue;
         }

         if (!config_get_float(conf, id, &parameter->current))
            RARCH_WARN("[CGP/GLSLP]: Parameter %s is not set in preset.\n", id);
      }
   }

   return true;
}

/** 
 * video_shader_parse_imports:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Resolves import parameters belonging to shaders. 
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool video_shader_parse_imports(config_file_t *conf,
      struct video_shader *shader)
{
   const char *id     = NULL;
   char *save         = NULL;
   char imports[1024] = {0};

   if (!config_get_array(conf, "imports", imports, sizeof(imports)))
      return true;

   for (id = strtok_r(imports, ";", &save);
         id && shader->variables < GFX_MAX_VARIABLES;
         shader->variables++, id = strtok_r(NULL, ";", &save))
   {
      uint32_t semantic_hash;
      char semantic_buf[64]   = {0};
      char wram_buf[64]       = {0};
      char input_slot_buf[64] = {0};
      char mask_buf[64]       = {0};
      char equal_buf[64]      = {0};
      char semantic[64]       = {0};
      unsigned addr           = 0;
      unsigned mask           = 0;
      unsigned equal          = 0;
      struct state_tracker_uniform_info *var = 
         &shader->variable[shader->variables];

      strlcpy(var->id, id, sizeof(var->id));

      snprintf(semantic_buf, sizeof(semantic_buf), "%s_semantic", id);
      snprintf(wram_buf, sizeof(wram_buf), "%s_wram", id);
      snprintf(input_slot_buf, sizeof(input_slot_buf), "%s_input_slot", id);
      snprintf(mask_buf, sizeof(mask_buf), "%s_mask", id);
      snprintf(equal_buf, sizeof(equal_buf), "%s_equal", id);

      if (!config_get_array(conf, semantic_buf, semantic, sizeof(semantic)))
      {
         RARCH_ERR("No semantic for import variable.\n");
         return false;
      }

      semantic_hash = djb2_calculate(semantic);

      switch (semantic_hash)
      {
         case SEMANTIC_CAPTURE:
            var->type = RARCH_STATE_CAPTURE;
            break;
         case SEMANTIC_TRANSITION:
            var->type = RARCH_STATE_TRANSITION;
            break;
         case SEMANTIC_TRANSITION_COUNT:
            var->type = RARCH_STATE_TRANSITION_COUNT;
            break;
         case SEMANTIC_CAPTURE_PREVIOUS:
            var->type = RARCH_STATE_CAPTURE_PREV;
            break;
         case SEMANTIC_TRANSITION_PREVIOUS:
            var->type = RARCH_STATE_TRANSITION_PREV;
            break;
         case SEMANTIC_PYTHON:
            var->type = RARCH_STATE_PYTHON;
            break;
         default:
            RARCH_ERR("Invalid semantic.\n");
            return false;
      }


      if (var->type != RARCH_STATE_PYTHON)
      {
         unsigned input_slot = 0;

         if (config_get_uint(conf, input_slot_buf, &input_slot))
         {
            switch (input_slot)
            {
               case 1:
                  var->ram_type = RARCH_STATE_INPUT_SLOT1;
                  break;

               case 2:
                  var->ram_type = RARCH_STATE_INPUT_SLOT2;
                  break;

               default:
                  RARCH_ERR("Invalid input slot for import.\n");
                  return false;
            }
         }
         else if (config_get_hex(conf, wram_buf, &addr))
         {
            var->ram_type = RARCH_STATE_WRAM;
            var->addr = addr;
         }
         else
         {
            RARCH_ERR("No address assigned to semantic.\n");
            return false;
         }
      }

      if (config_get_hex(conf, mask_buf, &mask))
         var->mask = mask;
      if (config_get_hex(conf, equal_buf, &equal))
         var->equal = equal;
   }

   config_get_path(conf, "import_script",
         shader->script_path, sizeof(shader->script_path));
   config_get_array(conf, "import_script_class",
         shader->script_class, sizeof(shader->script_class));

   return true;
}

/** 
 * video_shader_read_conf_cgp:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Loads preset file and all associated state (passes,
 * textures, imports, etc). 
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_shader_read_conf_cgp(config_file_t *conf, struct video_shader *shader)
{
   unsigned shaders, i;

   memset(shader, 0, sizeof(*shader));
   shader->type = RARCH_SHADER_CG;

   shaders = 0;
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

   if (!config_get_int(conf, "feedback_pass", &shader->feedback_pass))
      shader->feedback_pass = -1;

   shader->passes = min(shaders, GFX_MAX_SHADERS);
   for (i = 0; i < shader->passes; i++)
   {
      if (!video_shader_parse_pass(conf, &shader->pass[i], i))
         return false;
   }

   if (!video_shader_parse_textures(conf, shader))
      return false;

   if (!video_shader_parse_imports(conf, shader))
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

static void shader_write_scale_dim(config_file_t *conf, const char *dim,
      enum gfx_scale_type type, float scale, unsigned absolute, unsigned i)
{
   char key[64] = {0};

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
   char key[64] = {0};

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
 * import_semantic_to_string:
 * @type              : Import semantic type from state tracker.
 *
 * Translates import semantic to human-readable string identifier.
 *
 * Returns: human-readable string identifier of import semantic.
 **/
static const char *import_semantic_to_str(enum state_tracker_type type)
{
   switch (type)
   {
      case RARCH_STATE_CAPTURE:
         return "capture";
      case RARCH_STATE_TRANSITION:
         return "transition";
      case RARCH_STATE_TRANSITION_COUNT:
         return "transition_count";
      case RARCH_STATE_CAPTURE_PREV:
         return "capture_previous";
      case RARCH_STATE_TRANSITION_PREV:
         return "transition_previous";
      case RARCH_STATE_PYTHON:
         return "python";
      default:
         break;
   }

   return "?";
}

/**
 * shader_write_variable:
 * @conf              : Preset file to read from.
 * @info              : State tracker uniform info handle.
 *
 * Writes variable to shader preset file.
 **/
static void shader_write_variable(config_file_t *conf,
      const struct state_tracker_uniform_info *info)
{
   char semantic_buf[64]   = {0};
   char wram_buf[64]       = {0};
   char input_slot_buf[64] = {0};
   char mask_buf[64]       = {0};
   char equal_buf[64]      = {0};
   const char *id          = info->id;

   snprintf(semantic_buf, sizeof(semantic_buf), "%s_semantic", id);
   snprintf(wram_buf, sizeof(wram_buf), "%s_wram", id);
   snprintf(input_slot_buf, sizeof(input_slot_buf), "%s_input_slot", id);
   snprintf(mask_buf, sizeof(mask_buf), "%s_mask", id);
   snprintf(equal_buf, sizeof(equal_buf), "%s_equal", id);

   config_set_string(conf, semantic_buf,
         import_semantic_to_str(info->type));
   config_set_hex(conf, mask_buf, info->mask);
   config_set_hex(conf, equal_buf, info->equal);

   switch (info->ram_type)
   {
      case RARCH_STATE_INPUT_SLOT1:
         config_set_int(conf, input_slot_buf, 1);
         break;

      case RARCH_STATE_INPUT_SLOT2:
         config_set_int(conf, input_slot_buf, 2);
         break;

      case RARCH_STATE_WRAM:
         config_set_hex(conf, wram_buf, info->addr);
         break;

      case RARCH_STATE_NONE:
         break;
   }
}

/** 
 * video_shader_write_conf_cgp:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Saves preset and all associated state (passes,
 * textures, imports, etc) to disk. 
 **/
void video_shader_write_conf_cgp(config_file_t *conf,
      struct video_shader *shader)
{
   unsigned i;

   config_set_int(conf, "shaders", shader->passes);
   if (shader->feedback_pass >= 0)
      config_set_int(conf, "feedback_pass", shader->feedback_pass);

   for (i = 0; i < shader->passes; i++)
   {
      char key[64] = {0};
      const struct video_shader_pass *pass = &shader->pass[i];

      snprintf(key, sizeof(key), "shader%u", i);
      config_set_string(conf, key, pass->source.path);

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
      char parameters[4096] = {0};

      strlcpy(parameters, shader->parameters[0].id, sizeof(parameters));

      for (i = 1; i < shader->num_parameters; i++)
      {
         /* O(n^2), but number of parameters is very limited. */
         strlcat(parameters, ";", sizeof(parameters));
         strlcat(parameters, shader->parameters[i].id, sizeof(parameters));
      }

      config_set_string(conf, "parameters", parameters);
      
      for (i = 0; i < shader->num_parameters; i++)
         config_set_float(conf, shader->parameters[i].id,
               shader->parameters[i].current);
   }

   if (shader->luts)
   {
      char textures[4096] = {0};

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
         char key[64] = {0};

         config_set_string(conf, shader->lut[i].id, shader->lut[i].path);

         if (shader->lut[i].filter != RARCH_FILTER_UNSPEC)
         {
            snprintf(key, sizeof(key), "%s_linear", shader->lut[i].id);
            config_set_bool(conf, key, 
                  shader->lut[i].filter == RARCH_FILTER_LINEAR);
         }

         snprintf(key, sizeof(key), "%s_wrap_mode", shader->lut[i].id);
         config_set_string(conf, key, wrap_mode_to_str(shader->lut[i].wrap));

         snprintf(key, sizeof(key), "%s_mipmap", shader->lut[i].id);
         config_set_bool(conf, key, shader->lut[i].mipmap);
      }
   }

   if (*shader->script_path)
      config_set_string(conf, "import_script", shader->script_path);
   if (*shader->script_class)
      config_set_string(conf, "import_script_class", shader->script_class);

   if (shader->variables)
   {
      char variables[4096] = {0};

      strlcpy(variables, shader->variable[0].id, sizeof(variables));

      for (i = 1; i < shader->variables; i++)
      {
         strlcat(variables, ";", sizeof(variables));
         strlcat(variables, shader->variable[i].id, sizeof(variables));
      }

      config_set_string(conf, "imports", variables);

      for (i = 0; i < shader->variables; i++)
         shader_write_variable(conf, &shader->variable[i]);
   }
}

/**
 * video_shader_parse_type:
 * @path              : Shader path.
 * @fallback          : Fallback shader type in case no
 *                      type could be found.
 *
 * Parses type of shader.
 *
 * Returns: value of shader type on success, otherwise will return
 * user-supplied @fallback value. 
 **/
enum rarch_shader_type video_shader_parse_type(const char *path,
      enum rarch_shader_type fallback)
{
   const char *ext = NULL;

   if (!path)
      return fallback;

   ext = path_get_extension(path);

   if (string_is_equal(ext, "cg") || string_is_equal(ext, "cgp"))
      return RARCH_SHADER_CG;
   else if (string_is_equal(ext, "glslp") || string_is_equal(ext, "glsl"))
      return RARCH_SHADER_GLSL;

   return fallback;
}

/**
 * video_shader_resolve_relative:
 * @shader            : Shader pass handle.
 * @ref_path          : Relative shader path.
 *
 * Resolves relative shader path (@ref_path) into absolute
 * shader paths.
 **/
void video_shader_resolve_relative(struct video_shader *shader,
      const char *ref_path)
{
   unsigned i;
   char tmp_path[4096] = {0};

   for (i = 0; i < shader->passes; i++)
   {
      if (!*shader->pass[i].source.path)
         continue;

      strlcpy(tmp_path, shader->pass[i].source.path, sizeof(tmp_path));
      fill_pathname_resolve_relative(shader->pass[i].source.path,
            ref_path, tmp_path, sizeof(shader->pass[i].source.path));
   }

   for (i = 0; i < shader->luts; i++)
   {
      strlcpy(tmp_path, shader->lut[i].path, sizeof(tmp_path));
      fill_pathname_resolve_relative(shader->lut[i].path,
            ref_path, tmp_path, sizeof(shader->lut[i].path));
   }

   if (*shader->script_path)
   {
      strlcpy(tmp_path, shader->script_path, sizeof(tmp_path));
      fill_pathname_resolve_relative(shader->script_path,
            ref_path, tmp_path, sizeof(shader->script_path));
   }
}

