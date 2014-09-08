/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include "shader_parse.h"
#include "../compat/posix_string.h"
#include "../msvc/msvc_compat.h"
#include "../file.h"
#include "../compat/strl.h"
#include "../general.h"

#define print_buf(buf, ...) snprintf(buf, sizeof(buf), __VA_ARGS__)

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
         return "???";
   }
}

static enum gfx_wrap_type wrap_str_to_mode(const char *wrap_mode)
{
   if (strcmp(wrap_mode, "clamp_to_border") == 0)
      return RARCH_WRAP_BORDER;
   else if (strcmp(wrap_mode, "clamp_to_edge") == 0)
      return RARCH_WRAP_EDGE;
   else if (strcmp(wrap_mode, "repeat") == 0)
      return RARCH_WRAP_REPEAT;
   else if (strcmp(wrap_mode, "mirrored_repeat") == 0)
      return RARCH_WRAP_MIRRORED_REPEAT;

   RARCH_WARN("Invalid wrapping type %s. Valid ones are: clamp_to_border (default), clamp_to_edge, repeat and mirrored_repeat. Falling back to default.\n",
         wrap_mode);
   return RARCH_WRAP_DEFAULT;
}

// CGP
static bool shader_parse_pass(config_file_t *conf, struct gfx_shader_pass *pass, unsigned i)
{
   // Source
   char shader_name[64];
   print_buf(shader_name, "shader%u", i);
   if (!config_get_path(conf, shader_name, pass->source.path, sizeof(pass->source.path)))
   {
      RARCH_ERR("Couldn't parse shader source (%s).\n", shader_name);
      return false;
   }
   
   // Smooth
   char filter_name_buf[64];
   print_buf(filter_name_buf, "filter_linear%u", i);
   bool smooth = false;
   if (config_get_bool(conf, filter_name_buf, &smooth))
      pass->filter = smooth ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
   else
      pass->filter = RARCH_FILTER_UNSPEC;

   // Wrapping mode
   char wrap_name_buf[64];
   print_buf(wrap_name_buf, "wrap_mode%u", i);
   char wrap_mode[64];
   if (config_get_array(conf, wrap_name_buf, wrap_mode, sizeof(wrap_mode)))
      pass->wrap = wrap_str_to_mode(wrap_mode);

   // Frame count mod
   char frame_count_mod[64] = {0};
   char frame_count_mod_buf[64];
   print_buf(frame_count_mod_buf, "frame_count_mod%u", i);
   if (config_get_array(conf, frame_count_mod_buf,
            frame_count_mod, sizeof(frame_count_mod)))
      pass->frame_count_mod = strtoul(frame_count_mod, NULL, 0);

   // FBO types and mipmapping
   char srgb_output_buf[64];
   print_buf(srgb_output_buf, "srgb_framebuffer%u", i);
   config_get_bool(conf, srgb_output_buf, &pass->fbo.srgb_fbo);

   char fp_fbo_buf[64];
   print_buf(fp_fbo_buf, "float_framebuffer%u", i);
   config_get_bool(conf, fp_fbo_buf, &pass->fbo.fp_fbo);

   char mipmap_buf[64];
   print_buf(mipmap_buf, "mipmap_input%u", i);
   config_get_bool(conf, mipmap_buf, &pass->mipmap);

   char alias_buf[64];
   print_buf(alias_buf, "alias%u", i);
   if (!config_get_array(conf, alias_buf, pass->alias, sizeof(pass->alias)))
      *pass->alias = '\0';

   // Scale
   struct gfx_fbo_scale *scale = &pass->fbo;
   char scale_type[64] = {0};
   char scale_type_x[64] = {0};
   char scale_type_y[64] = {0};
   char scale_name_buf[64];
   print_buf(scale_name_buf, "scale_type%u", i);
   config_get_array(conf, scale_name_buf, scale_type, sizeof(scale_type));

   print_buf(scale_name_buf, "scale_type_x%u", i);
   config_get_array(conf, scale_name_buf, scale_type_x, sizeof(scale_type_x));

   print_buf(scale_name_buf, "scale_type_y%u", i);
   config_get_array(conf, scale_name_buf, scale_type_y, sizeof(scale_type_y));

   if (!*scale_type && !*scale_type_x && !*scale_type_y)
      return true;

   if (*scale_type)
   {
      strlcpy(scale_type_x, scale_type, sizeof(scale_type_x));
      strlcpy(scale_type_y, scale_type, sizeof(scale_type_y));
   }

   char attr_name_buf[64];
   float fattr = 0.0f;
   int iattr = 0;

   scale->valid = true;
   scale->type_x = RARCH_SCALE_INPUT;
   scale->type_y = RARCH_SCALE_INPUT;
   scale->scale_x = 1.0;
   scale->scale_y = 1.0;

   if (*scale_type_x)
   {
      if (strcmp(scale_type_x, "source") == 0)
         scale->type_x = RARCH_SCALE_INPUT;
      else if (strcmp(scale_type_x, "viewport") == 0)
         scale->type_x = RARCH_SCALE_VIEWPORT;
      else if (strcmp(scale_type_x, "absolute") == 0)
         scale->type_x = RARCH_SCALE_ABSOLUTE;
      else
      {
         RARCH_ERR("Invalid attribute.\n");
         return false;
      }
   }

   if (*scale_type_y)
   {
      if (strcmp(scale_type_y, "source") == 0)
         scale->type_y = RARCH_SCALE_INPUT;
      else if (strcmp(scale_type_y, "viewport") == 0)
         scale->type_y = RARCH_SCALE_VIEWPORT;
      else if (strcmp(scale_type_y, "absolute") == 0)
         scale->type_y = RARCH_SCALE_ABSOLUTE;
      else
      {
         RARCH_ERR("Invalid attribute.\n");
         return false;
      }
   }

   print_buf(attr_name_buf, "scale%u", i);
   if (scale->type_x == RARCH_SCALE_ABSOLUTE)
   {
      if (config_get_int(conf, attr_name_buf, &iattr))
         scale->abs_x = iattr;
      else
      {
         print_buf(attr_name_buf, "scale_x%u", i);
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
         print_buf(attr_name_buf, "scale_x%u", i);
         if (config_get_float(conf, attr_name_buf, &fattr))
            scale->scale_x = fattr;
      }
   }

   print_buf(attr_name_buf, "scale%u", i);
   if (scale->type_y == RARCH_SCALE_ABSOLUTE)
   {
      if (config_get_int(conf, attr_name_buf, &iattr))
         scale->abs_y = iattr;
      else
      {
         print_buf(attr_name_buf, "scale_y%u", i);
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
         print_buf(attr_name_buf, "scale_y%u", i);
         if (config_get_float(conf, attr_name_buf, &fattr))
            scale->scale_y = fattr;
      }
   }

   return true;
}

static bool shader_parse_textures(config_file_t *conf,
      struct gfx_shader *shader)
{
   const char *id;
   char *save;
   char textures[1024];

   if (!config_get_array(conf, "textures", textures, sizeof(textures)))
      return true;

   for (id = strtok_r(textures, ";", &save);
         id && shader->luts < GFX_MAX_TEXTURES;
         shader->luts++, id = strtok_r(NULL, ";", &save))
   {
      if (!config_get_array(conf, id, shader->lut[shader->luts].path,
               sizeof(shader->lut[shader->luts].path)))
      {
         RARCH_ERR("Cannot find path to texture \"%s\" ...\n", id);
         return false;
      }

      strlcpy(shader->lut[shader->luts].id, id,
            sizeof(shader->lut[shader->luts].id));

      char id_filter[64];
      print_buf(id_filter, "%s_linear", id);
      bool smooth = false;
      if (config_get_bool(conf, id_filter, &smooth))
         shader->lut[shader->luts].filter = smooth ? 
            RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
      else
         shader->lut[shader->luts].filter = RARCH_FILTER_UNSPEC;

      char id_wrap[64];
      print_buf(id_wrap, "%s_wrap_mode", id);
      char wrap_mode[64];
      if (config_get_array(conf, id_wrap, wrap_mode, sizeof(wrap_mode)))
         shader->lut[shader->luts].wrap = wrap_str_to_mode(wrap_mode);

      char id_mipmap[64];
      print_buf(id_mipmap, "%s_mipmap", id);
      bool mipmap = false;
      if (config_get_bool(conf, id_mipmap, &mipmap))
         shader->lut[shader->luts].mipmap = mipmap;
      else
         shader->lut[shader->luts].mipmap = false;
   }

   return true;
}

static struct gfx_shader_parameter *find_parameter(
      struct gfx_shader_parameter *params, unsigned num_params, const char *id)
{
   unsigned i;
   for (i = 0; i < num_params; i++)
   {
      if (!strcmp(params[i].id, id))
         return &params[i];
   }
   return NULL;
}

bool gfx_shader_resolve_parameters(config_file_t *conf,
      struct gfx_shader *shader)
{
   unsigned i;

   shader->num_parameters = 0;
   struct gfx_shader_parameter *param = (struct gfx_shader_parameter*)
      &shader->parameters[shader->num_parameters];

   /* Find all parameters in our shaders. */
   for (i = 0; i < shader->passes; i++)
   {
      char line[2048];
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

         if (ret >= 5)
         {
            param->id[63] = '\0';
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
      }

      fclose(file);
   }

   /* Read in parameters which override the defaults. */
   if (conf)
   {
      char parameters[1024];
      char *save = NULL;
      const char *id;

      if (!config_get_array(conf, "parameters",
               parameters, sizeof(parameters)))
         return true;

      for (id = strtok_r(parameters, ";", &save); id; 
            id = strtok_r(NULL, ";", &save))
      {
         struct gfx_shader_parameter *param = (struct gfx_shader_parameter*)
            find_parameter(shader->parameters, shader->num_parameters, id);

         if (!param)
         {
            RARCH_WARN("[CGP/GLSLP]: Parameter %s is set in the preset, but no shader uses this parameter, ignoring.\n", id);
            continue;
         }

         if (!config_get_float(conf, id, &param->current))
            RARCH_WARN("[CGP/GLSLP]: Parameter %s is not set in preset.\n", id);
      }
   }

   return true;
}

static bool shader_parse_imports(config_file_t *conf,
      struct gfx_shader *shader)
{
   char imports[1024];
   char *save = NULL;
   const char *id;
   if (!config_get_array(conf, "imports", imports, sizeof(imports)))
      return true;

   for (id = strtok_r(imports, ";", &save);
         id && shader->variables < GFX_MAX_VARIABLES;
         shader->variables++, id = strtok_r(NULL, ";", &save))
   {
      struct state_tracker_uniform_info *var = 
         (struct state_tracker_uniform_info*)
         &shader->variable[shader->variables];

      strlcpy(var->id, id, sizeof(var->id));

      char semantic_buf[64];
      char wram_buf[64];
      char input_slot_buf[64];
      char mask_buf[64];
      char equal_buf[64];

      print_buf(semantic_buf, "%s_semantic", id);
      print_buf(wram_buf, "%s_wram", id);
      print_buf(input_slot_buf, "%s_input_slot", id);
      print_buf(mask_buf, "%s_mask", id);
      print_buf(equal_buf, "%s_equal", id);

      char semantic[64];
      if (!config_get_array(conf, semantic_buf, semantic, sizeof(semantic)))
      {
         RARCH_ERR("No semantic for import variable.\n");
         return false;
      }

      if (strcmp(semantic, "capture") == 0)
         var->type = RARCH_STATE_CAPTURE;
      else if (strcmp(semantic, "transition") == 0)
         var->type = RARCH_STATE_TRANSITION;
      else if (strcmp(semantic, "transition_count") == 0)
         var->type = RARCH_STATE_TRANSITION_COUNT;
      else if (strcmp(semantic, "capture_previous") == 0)
         var->type = RARCH_STATE_CAPTURE_PREV;
      else if (strcmp(semantic, "transition_previous") == 0)
         var->type = RARCH_STATE_TRANSITION_PREV;
      else if (strcmp(semantic, "python") == 0)
         var->type = RARCH_STATE_PYTHON;
      else
      {
         RARCH_ERR("Invalid semantic.\n");
         return false;
      }

      unsigned addr = 0, mask = 0, equal = 0;
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

bool gfx_shader_read_conf_cgp(config_file_t *conf, struct gfx_shader *shader)
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

   shader->passes = min(shaders, GFX_MAX_SHADERS);
   for (i = 0; i < shader->passes; i++)
   {
      if (!shader_parse_pass(conf, &shader->pass[i], i))
         return false;
   }

   if (!shader_parse_textures(conf, shader))
      return false;

   if (!shader_parse_imports(conf, shader))
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
         return "?";
   }
}

static void shader_write_scale_dim(config_file_t *conf, const char *dim,
      enum gfx_scale_type type, float scale, unsigned abs, unsigned i)
{
   char key[64];
   print_buf(key, "scale_type_%s%u", dim, i);
   config_set_string(conf, key, scale_type_to_str(type));

   print_buf(key, "scale_%s%u", dim, i);
   if (type == RARCH_SCALE_ABSOLUTE)
      config_set_int(conf, key, abs);
   else
      config_set_float(conf, key, scale);
}

static void shader_write_fbo(config_file_t *conf,
      const struct gfx_fbo_scale *fbo, unsigned i)
{
   char key[64];
   print_buf(key, "float_framebuffer%u", i);
   config_set_bool(conf, key, fbo->fp_fbo);
   print_buf(key, "srgb_framebuffer%u", i);
   config_set_bool(conf, key, fbo->srgb_fbo);

   if (!fbo->valid)
      return;

   shader_write_scale_dim(conf, "x", fbo->type_x, fbo->scale_x, fbo->abs_x, i);
   shader_write_scale_dim(conf, "y", fbo->type_y, fbo->scale_y, fbo->abs_y, i);
}

static const char *import_semantic_to_string(enum state_tracker_type type)
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
         return "?";
   }
}

static void shader_write_variable(config_file_t *conf,
      const struct state_tracker_uniform_info *info)
{
   const char *id = info->id;

   char semantic_buf[64];
   char wram_buf[64];
   char input_slot_buf[64];
   char mask_buf[64];
   char equal_buf[64];

   print_buf(semantic_buf, "%s_semantic", id);
   print_buf(wram_buf, "%s_wram", id);
   print_buf(input_slot_buf, "%s_input_slot", id);
   print_buf(mask_buf, "%s_mask", id);
   print_buf(equal_buf, "%s_equal", id);

   config_set_string(conf, semantic_buf,
         import_semantic_to_string(info->type));
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

      default:
         break;
   }
}

void gfx_shader_write_conf_cgp(config_file_t *conf,
      struct gfx_shader *shader)
{
   unsigned i;
   config_set_int(conf, "shaders", shader->passes);
   for (i = 0; i < shader->passes; i++)
   {
      const struct gfx_shader_pass *pass = &shader->pass[i];

      char key[64];
      print_buf(key, "shader%u", i);
      config_set_string(conf, key, pass->source.path);

      if (pass->filter != RARCH_FILTER_UNSPEC)
      {
         print_buf(key, "filter_linear%u", i);
         config_set_bool(conf, key, pass->filter == RARCH_FILTER_LINEAR);
      }

      print_buf(key, "wrap_mode%u", i);
      config_set_string(conf, key, wrap_mode_to_str(pass->wrap));

      if (pass->frame_count_mod)
      {
         print_buf(key, "frame_count_mod%u", i);
         config_set_int(conf, key, pass->frame_count_mod);
      }

      print_buf(key, "mipmap_input%u", i);
      config_set_bool(conf, key, pass->mipmap);

      print_buf(key, "alias%u", i);
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
         char key[64];

         config_set_string(conf, shader->lut[i].id, shader->lut[i].path);

         if (shader->lut[i].filter != RARCH_FILTER_UNSPEC)
         {
            print_buf(key, "%s_linear", shader->lut[i].id);
            config_set_bool(conf, key, 
                  shader->lut[i].filter == RARCH_FILTER_LINEAR);
         }

         print_buf(key, "%s_wrap_mode", shader->lut[i].id);
         config_set_string(conf, key, wrap_mode_to_str(shader->lut[i].wrap));

         print_buf(key, "%s_mipmap", shader->lut[i].id);
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

enum rarch_shader_type gfx_shader_parse_type(const char *path,
      enum rarch_shader_type fallback)
{
   if (!path)
      return fallback;

   const char *ext = path_get_extension(path);

   if (strcmp(ext, "cg") == 0 || strcmp(ext, "cgp") == 0)
      return RARCH_SHADER_CG;
   else if (strcmp(ext, "glslp") == 0 || strcmp(ext, "glsl") == 0)
      return RARCH_SHADER_GLSL;

   return fallback;
}

void gfx_shader_resolve_relative(struct gfx_shader *shader,
      const char *ref_path)
{
   unsigned i;
   char tmp_path[PATH_MAX];

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

