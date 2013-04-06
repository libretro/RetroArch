/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "shader_parse.h"
#include "../compat/posix_string.h"
#include "../msvc/msvc_compat.h"
#include <stdlib.h>
#include <string.h>

#define print_buf(buf, ...) snprintf(buf, sizeof(buf), __VA_ARGS__)

static bool shader_parse_pass(config_file_t *conf, struct gfx_shader_pass *pass, unsigned i)
{
   // Source
   char shader_name[64];
   print_buf(shader_name, "shader%u", i);
   if (!config_get_path(conf, shader_name, pass->source.cg, sizeof(pass->source.cg)))
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

   // Frame count mod
   char frame_count_mod[64] = {0};
   char frame_count_mod_buf[64];
   print_buf(frame_count_mod_buf, "frame_count_mod%u", i);
   if (config_get_array(conf, frame_count_mod_buf, frame_count_mod, sizeof(frame_count_mod)))
      pass->frame_count_mod = strtoul(frame_count_mod, NULL, 0);

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

   char fp_fbo_buf[64];
   print_buf(fp_fbo_buf, "float_framebuffer%u", i);
   config_get_bool(conf, fp_fbo_buf, &scale->fp_fbo);

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

   if (scale->type_x == RARCH_SCALE_ABSOLUTE)
   {
      print_buf(attr_name_buf, "scale%u", i);
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
      print_buf(attr_name_buf, "scale%u", i);
      if (config_get_float(conf, attr_name_buf, &fattr))
         scale->scale_x = fattr;
      else
      {
         print_buf(attr_name_buf, "scale_x%u", i);
         if (config_get_float(conf, attr_name_buf, &fattr))
            scale->scale_x = fattr;
      }
   }

   if (scale->type_y == RARCH_SCALE_ABSOLUTE)
   {
      print_buf(attr_name_buf, "scale%u", i);
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
      print_buf(attr_name_buf, "scale%u", i);
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

static bool shader_parse_textures(config_file_t *conf, struct gfx_shader *shader)
{
   char textures[1024];
   if (!config_get_array(conf, "textures", textures, sizeof(textures)))
      return true;

   char *save;
   for (const char *id = strtok_r(textures, ";", &save);
         id && shader->luts < GFX_MAX_TEXTURES;
         shader->luts++, id = strtok_r(NULL, ";", &save))
   {
      if (!config_get_array(conf, id, shader->lut[shader->luts].path, sizeof(shader->lut[shader->luts].path)))
      {
         RARCH_ERR("Cannot find path to texture \"%s\" ...\n", id);
         return false;
      }

      strlcpy(shader->lut[shader->luts].id, id, sizeof(shader->lut[shader->luts].id));

      char id_filter[64];
      print_buf(id_filter, "%s_linear", id);

      bool smooth = false;
      if (config_get_bool(conf, id_filter, &smooth))
         shader->lut[shader->luts].filter = smooth ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
      else
         shader->lut[shader->luts].filter = RARCH_FILTER_UNSPEC;
   }

   return true;
}

static bool shader_parse_imports(config_file_t *conf, struct gfx_shader *shader)
{
   char imports[1024];
   if (!config_get_array(conf, "imports", imports, sizeof(imports)))
      return true;

   char *save;
   const char *id = strtok_r(imports, ";", &save);
   for (const char *id = strtok_r(imports, ";", &save);
         id && shader->variables < GFX_MAX_VARIABLES;
         shader->variables++, id = strtok_r(NULL, ";", &save))
   {
      struct state_tracker_uniform_info *var = &shader->variable[shader->variables];

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

      enum state_ram_type ram_type = RARCH_STATE_NONE;

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

   config_get_path(conf, "import_script", shader->script_path, sizeof(shader->script_path));
   config_get_array(conf, "import_script_class", shader->script_class, sizeof(shader->script_class));

   return true;
}

bool gfx_shader_read_conf_cgp(config_file_t *conf, struct gfx_shader *shader)
{
   memset(shader, 0, sizeof(*shader));

   unsigned shaders = 0;
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
   for (unsigned i = 0; i < shader->passes; i++)
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

static void shader_write_fbo(config_file_t *conf, const struct gfx_fbo_scale *fbo, unsigned i)
{
   char key[64];
   print_buf(key, "float_framebuffer%u", i);
   config_set_bool(conf, key, fbo->fp_fbo);

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

static void shader_write_variable(config_file_t *conf, const struct state_tracker_uniform_info *info)
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

   config_set_string(conf, semantic_buf, import_semantic_to_string(info->type));
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

void gfx_shader_write_conf_cgp(config_file_t *conf, const struct gfx_shader *shader)
{
   config_set_int(conf, "shaders", shader->passes);
   for (unsigned i = 0; i < shader->passes; i++)
   {
      const struct gfx_shader_pass *pass = &shader->pass[i];

      char key[64];
      print_buf(key, "shader%u", i);
      config_set_string(conf, key, pass->source.cg);

      if (pass->filter != RARCH_FILTER_UNSPEC)
      {
         print_buf(key, "filter_linear%u", i);
         config_set_bool(conf, key, pass->filter == RARCH_FILTER_LINEAR);
      }

      print_buf(key, "frame_count_mod%u", i);
      config_set_int(conf, key, pass->frame_count_mod);

      shader_write_fbo(conf, &pass->fbo, i);
   }

   if (shader->luts)
   {
      char textures[4096] = {0};
      strlcpy(textures, shader->lut[0].id, sizeof(textures));
      for (unsigned i = 1; i < shader->luts; i++)
      {
         // O(n^2), but number of textures is very limited.
         strlcat(textures, ";", sizeof(textures));
         strlcat(textures, shader->lut[i].id, sizeof(textures));
      }

      config_set_string(conf, "textures", textures);

      for (unsigned i = 0; i < shader->luts; i++)
      {
         char key[64];

         if (shader->lut[i].filter != RARCH_FILTER_UNSPEC)
         {
            print_buf(key, "%s_linear", shader->lut[i].id);
            config_set_bool(conf, key, shader->lut[i].filter != RARCH_FILTER_LINEAR);
         }
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
      for (unsigned i = 1; i < shader->variables; i++)
      {
         strlcat(variables, ";", sizeof(variables));
         strlcat(variables, shader->variable[i].id, sizeof(variables));
      }

      config_set_string(conf, "imports", variables);

      for (unsigned i = 0; i < shader->variables; i++)
         shader_write_variable(conf, &shader->variable[i]);
   }
}

