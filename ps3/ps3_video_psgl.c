/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "../driver.h"

#include "ps3_video_psgl.h"

#include <stdint.h>
#include "../libsnes.hpp"
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <math.h>

#include <sys/spu_initialize.h>

#include "../gfx/snes_state.h"
#include "../general.h"
#include "../compat/strl.h"
#include "shared.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../compat/strl.h"

#define BLUE		0xffff0000u
#define WHITE		0xffffffffu

// Used for the last pass when rendering to the back buffer.
static const GLfloat vertexes_flipped[] = {
   0, 0,
   0, 1,
   1, 1,
   1, 0
};

// Other vertex orientations
static const GLfloat vertexes_90[] = {
   0, 1,
   1, 1,
   1, 0,
   0, 0
};

static const GLfloat vertexes_180[] = {
   1, 1,
   1, 0,
   0, 0,
   0, 1
};

static const GLfloat vertexes_270[] = {
   1, 0,
   0, 0,
   0, 1,
   1, 1
};

static const GLfloat *vertex_ptr = vertexes_flipped;

// Used when rendering to an FBO.
// Texture coords have to be aligned with vertex coordinates.
static const GLfloat vertexes[] = {
   0, 1,
   0, 0,
   1, 0,
   1, 1
};

static const GLfloat tex_coords[] = {
   0, 1,
   0, 0,
   1, 0,
   1, 1
};

static const GLfloat white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

bool g_quitting;
unsigned g_frame_count;
void *g_gl;

/*============================================================
	CG IMPLEMENTATION
============================================================ */

#define print_buf(buf, ...) snprintf(buf, sizeof(buf), __VA_ARGS__)

// Used when we call deactivate() since just unbinding the program didn't seem to work... :(
static const char *stock_cg_program =
      "void main_vertex"
      "("
      "	float4 position : POSITION,"
      "	float2 texCoord : TEXCOORD0,"
      "  float4 color : COLOR,"
      ""
      "  uniform float4x4 modelViewProj,"
      ""
      "	out float4 oPosition : POSITION,"
      "	out float2 otexCoord : TEXCOORD0,"
      "  out float4 oColor : COLOR"
      ")"
      "{"
      "	oPosition = mul(modelViewProj, position);"
      "	otexCoord = texCoord;"
      "  oColor = color;"
      "}"
      ""
      "float4 main_fragment(in float4 color : COLOR, float2 tex : TEXCOORD0, uniform sampler2D s0 : TEXUNIT0) : COLOR"
      "{"
      "   return color * tex2D(s0, tex);"
      "}";

static char *menu_cg_program;

static CGcontext cgCtx;

struct cg_fbo_params
{
   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter tex;
   CGparameter coord;
};

#define MAX_TEXTURES 8
#define MAX_VARIABLES 64
#define PREV_TEXTURES 7

struct cg_program
{
   CGprogram vprg;
   CGprogram fprg;
   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter out_size_f;
   CGparameter frame_cnt_f;
   CGparameter frame_dir_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter out_size_v;
   CGparameter frame_cnt_v;
   CGparameter frame_dir_v;
   CGparameter mvp;

   struct cg_fbo_params fbo[SSNES_CG_MAX_SHADERS];
   struct cg_fbo_params orig;
   struct cg_fbo_params prev[PREV_TEXTURES];
};

#define FILTER_UNSPEC 0
#define FILTER_LINEAR 1
#define FILTER_NEAREST 2

static struct cg_program prg[SSNES_CG_MAX_SHADERS];
static const char **cg_arguments;
static CGprofile cgVProf, cgFProf;
static unsigned active_index = 0;
static unsigned cg_shader_num = 0;
static struct gl_fbo_scale cg_scale[SSNES_CG_MAX_SHADERS];
static unsigned fbo_smooth[SSNES_CG_MAX_SHADERS];

static GLuint lut_textures[MAX_TEXTURES];
static unsigned lut_textures_num = 0;
static char lut_textures_uniform[MAX_TEXTURES][64];

static CGparameter cg_attribs[PREV_TEXTURES + 1 + SSNES_CG_MAX_SHADERS];
static unsigned cg_attrib_index;

static snes_tracker_t *snes_tracker = NULL;

static void gl_cg_reset_attrib(void)
{
   for (unsigned i = 0; i < cg_attrib_index; i++)
      cgGLDisableClientState(cg_attribs[i]);
   cg_attrib_index = 0;
}

#define set_param_2f(param, x, y) \
   if (param) cgGLSetParameter2f(param, x, y)
#define set_param_1f(param, x) \
   if (param) cgGLSetParameter1f(param, x)

static inline void gl_cg_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info,
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info,
      unsigned fbo_info_cnt)
{
   if (active_index == 0)
      return;

   // Set frame.
   set_param_2f(prg[active_index].vid_size_f, width, height);
   set_param_2f(prg[active_index].tex_size_f, tex_width, tex_height);
   set_param_2f(prg[active_index].out_size_f, out_width, out_height);
   set_param_1f(prg[active_index].frame_cnt_f, (float)frame_count);
   set_param_1f(prg[active_index].frame_dir_f, g_extern.frame_is_reverse ? -1.0 : 1.0);

   set_param_2f(prg[active_index].vid_size_v, width, height);
   set_param_2f(prg[active_index].tex_size_v, tex_width, tex_height);
   set_param_2f(prg[active_index].out_size_v, out_width, out_height);
   set_param_1f(prg[active_index].frame_cnt_v, (float)frame_count);
   set_param_1f(prg[active_index].frame_dir_v, g_extern.frame_is_reverse ? -1.0 : 1.0);

   if (active_index == SSNES_CG_MENU_SHADER_INDEX)
      return;

   // Set orig texture.
   CGparameter param = prg[active_index].orig.tex;
   if (param)
   {
      cgGLSetTextureParameter(param, info->tex);
      cgGLEnableTextureParameter(param);
   }

   set_param_2f(prg[active_index].orig.vid_size_v, info->input_size[0], info->input_size[1]);
   set_param_2f(prg[active_index].orig.vid_size_f, info->input_size[0], info->input_size[1]);
   set_param_2f(prg[active_index].orig.tex_size_v, info->tex_size[0],   info->tex_size[1]);
   set_param_2f(prg[active_index].orig.tex_size_f, info->tex_size[0],   info->tex_size[1]);
   if (prg[active_index].orig.coord)
   {
      cgGLSetParameterPointer(prg[active_index].orig.coord, 2, GL_FLOAT, 0, info->coord);
      cgGLEnableClientState(prg[active_index].orig.coord);
      cg_attribs[cg_attrib_index++] = prg[active_index].orig.coord;
   }

   // Set prev textures.
   for (unsigned i = 0; i < PREV_TEXTURES; i++)
   {
      param = prg[active_index].prev[i].tex;
      if (param)
      {
         cgGLSetTextureParameter(param, prev_info[i].tex);
         cgGLEnableTextureParameter(param);
      }

      set_param_2f(prg[active_index].prev[i].vid_size_v, prev_info[i].input_size[0], prev_info[i].input_size[1]);
      set_param_2f(prg[active_index].prev[i].vid_size_f, prev_info[i].input_size[0], prev_info[i].input_size[1]);
      set_param_2f(prg[active_index].prev[i].tex_size_v, prev_info[i].tex_size[0],   prev_info[i].tex_size[1]);
      set_param_2f(prg[active_index].prev[i].tex_size_f, prev_info[i].tex_size[0],   prev_info[i].tex_size[1]);

      if (prg[active_index].prev[i].coord)
      {
         cgGLSetParameterPointer(prg[active_index].prev[i].coord, 2, GL_FLOAT, 0, prev_info[i].coord);
         cgGLEnableClientState(prg[active_index].prev[i].coord);
         cg_attribs[cg_attrib_index++] = prg[active_index].prev[i].coord;
      }
   }

   // Set lookup textures.
   for (unsigned i = 0; i < lut_textures_num; i++)
   {
      CGparameter param = cgGetNamedParameter(prg[active_index].fprg, lut_textures_uniform[i]);
      if (param)
      {
         cgGLSetTextureParameter(param, lut_textures[i]);
         cgGLEnableTextureParameter(param);
      }
   }

   // Set FBO textures.
   if (active_index > 2)
   {
      for (unsigned i = 0; i < fbo_info_cnt; i++)
      {
         if (prg[active_index].fbo[i].tex)
         {
            cgGLSetTextureParameter(prg[active_index].fbo[i].tex, fbo_info[i].tex);
            cgGLEnableTextureParameter(prg[active_index].fbo[i].tex);
         }

         set_param_2f(prg[active_index].fbo[i].vid_size_v, fbo_info[i].input_size[0], fbo_info[i].input_size[1]);
         set_param_2f(prg[active_index].fbo[i].vid_size_f, fbo_info[i].input_size[0], fbo_info[i].input_size[1]);

         set_param_2f(prg[active_index].fbo[i].tex_size_v, fbo_info[i].tex_size[0], fbo_info[i].tex_size[1]);
         set_param_2f(prg[active_index].fbo[i].tex_size_f, fbo_info[i].tex_size[0], fbo_info[i].tex_size[1]);

         if (prg[active_index].fbo[i].coord)
         {
            cgGLSetParameterPointer(prg[active_index].fbo[i].coord, 2, GL_FLOAT, 0, fbo_info[i].coord);
            cgGLEnableClientState(prg[active_index].fbo[i].coord);
            cg_attribs[cg_attrib_index++] = prg[active_index].fbo[i].coord;
         }
      }
   }

   // Set state parameters
   if (snes_tracker)
   {
      static struct snes_tracker_uniform info[MAX_VARIABLES];
      static unsigned cnt = 0;

      if (active_index == 1)
         cnt = snes_get_uniform(snes_tracker, info, MAX_VARIABLES, frame_count);

      for (unsigned i = 0; i < cnt; i++)
      {
         CGparameter param_v = cgGetNamedParameter(prg[active_index].vprg, info[i].id);
         CGparameter param_f = cgGetNamedParameter(prg[active_index].fprg, info[i].id);
         set_param_1f(param_v, info[i].value);
         set_param_1f(param_f, info[i].value);
      }
   }
}

static void gl_cg_deinit_progs(void)
{
   cgGLUnbindProgram(cgFProf);
   cgGLUnbindProgram(cgVProf);

   // Programs may alias [0].
   for (unsigned i = 1; i < SSNES_CG_MAX_SHADERS; i++)
   {
      if (prg[i].fprg != prg[0].fprg)
         cgDestroyProgram(prg[i].fprg);
      if (prg[i].vprg != prg[0].vprg)
         cgDestroyProgram(prg[i].vprg);
   }

   if (prg[0].fprg)
      cgDestroyProgram(prg[0].fprg);
   if (prg[0].vprg)
      cgDestroyProgram(prg[0].vprg);

   memset(prg, 0, sizeof(prg));
}

static void gl_cg_deinit_state(void)
{
   gl_cg_reset_attrib();

   cg_shader_num = 0;

   gl_cg_deinit_progs();

   memset(cg_scale, 0, sizeof(cg_scale));
   memset(fbo_smooth, 0, sizeof(fbo_smooth));

   glDeleteTextures(lut_textures_num, lut_textures);
   lut_textures_num = 0;

   if (snes_tracker)
   {
      snes_tracker_free(snes_tracker);
      snes_tracker = NULL;
   }
}

// Final deinit.
static void gl_cg_deinit_context_state(void)
{
   if (menu_cg_program)
   {
      free(menu_cg_program);
      menu_cg_program = NULL;
   }
}

// Full deinit.
static void gl_cg_deinit(void)
{
   gl_cg_deinit_state();
   gl_cg_deinit_context_state();
}

#define SET_LISTING(type) \
{ \
   const char *list = cgGetLastListing(cgCtx); \
   if (list) \
      listing_##type = strdup(list); \
}

static bool load_program(unsigned index, const char *prog, bool path_is_file)
{
   bool ret = true;
   char *listing_f = NULL;
   char *listing_v = NULL;

   if (path_is_file)
   {
      prg[index].fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, prog, cgFProf, "main_fragment", cg_arguments);
      SET_LISTING(f);
      prg[index].vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, prog, cgVProf, "main_vertex", cg_arguments);
      SET_LISTING(v);
   }
   else
   {
      prg[index].fprg = cgCreateProgram(cgCtx, CG_SOURCE, prog, cgFProf, "main_fragment", cg_arguments);
      SET_LISTING(f);
      prg[index].vprg = cgCreateProgram(cgCtx, CG_SOURCE, prog, cgVProf, "main_vertex", cg_arguments);
      SET_LISTING(v);
   }

   if (!prg[index].fprg || !prg[index].vprg)
   {
      SSNES_ERR("CG error: %s\n", cgGetErrorString(cgGetError()));
      if (listing_f)
         SSNES_ERR("Fragment:\n%s\n", listing_f);
      else if (listing_v)
         SSNES_ERR("Vertex:\n%s\n", listing_v);

      ret = false;
      goto end;
   }

   cgGLLoadProgram(prg[index].fprg);
   cgGLLoadProgram(prg[index].vprg);

end:
   free(listing_f);
   free(listing_v);
   return ret;
}

static bool load_stock(void)
{
   if (!load_program(0, stock_cg_program, false))
   {
      SSNES_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }

   return true;
}

static bool load_shader_params(unsigned i, config_file_t *conf)
{
   bool ret = true;
   char *scale_type = NULL;
   char *scale_type_x = NULL;
   char *scale_type_y = NULL;
   bool has_scale_type;
   bool has_scale_type_x;
   bool has_scale_type_y;

   char scale_name_buf[64];
   print_buf(scale_name_buf, "scale_type%u", i);
   has_scale_type = config_get_string(conf, scale_name_buf, &scale_type);
   print_buf(scale_name_buf, "scale_type_x%u", i);
   has_scale_type_x = config_get_string(conf, scale_name_buf, &scale_type_x);
   print_buf(scale_name_buf, "scale_type_y%u", i);
   has_scale_type_y = config_get_string(conf, scale_name_buf, &scale_type_y);

   if (!has_scale_type && !has_scale_type_x && !has_scale_type_y)
      return true;

   if (has_scale_type)
   {
      free(scale_type_x);
      free(scale_type_y);

      scale_type_x = strdup(scale_type);
      scale_type_y = strdup(scale_type);

      free(scale_type);
      scale_type = NULL;
   }

   char attr_name_buf[64];
   float fattr = 0.0f;
   int iattr = 0;
   struct gl_fbo_scale *scale = &cg_scale[i + 1]; // Shader 0 is passthrough shader. Start at 1.

   scale->valid = true;
   scale->type_x = SSNES_SCALE_INPUT;
   scale->type_y = SSNES_SCALE_INPUT;
   scale->scale_x = 1.0;
   scale->scale_y = 1.0;
   scale->abs_x = g_extern.system.geom.base_width;
   scale->abs_y = g_extern.system.geom.base_height;

   if (strcmp(scale_type_x, "source") == 0)
      scale->type_x = SSNES_SCALE_INPUT;
   else if (strcmp(scale_type_x, "viewport") == 0)
      scale->type_x = SSNES_SCALE_VIEWPORT;
   else if (strcmp(scale_type_x, "absolute") == 0)
      scale->type_x = SSNES_SCALE_ABSOLUTE;
   else
   {
      SSNES_ERR("Invalid attribute.\n");
      ret = false;
      goto end;
   }

   if (strcmp(scale_type_y, "source") == 0)
      scale->type_y = SSNES_SCALE_INPUT;
   else if (strcmp(scale_type_y, "viewport") == 0)
      scale->type_y = SSNES_SCALE_VIEWPORT;
   else if (strcmp(scale_type_y, "absolute") == 0)
      scale->type_y = SSNES_SCALE_ABSOLUTE;
   else
   {
      SSNES_ERR("Invalid attribute.\n");
      ret = false;
      goto end;
   }

   if (scale->type_x == SSNES_SCALE_ABSOLUTE)
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

   if (scale->type_y == SSNES_SCALE_ABSOLUTE)
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

end:
   free(scale_type);
   free(scale_type_x);
   free(scale_type_y);
   return ret;
}

static bool load_shader(const char *dir_path, unsigned i, config_file_t *conf)
{
   char *shader_path = NULL;
   char attr_buf[64];
   char path_buf[PATH_MAX];

   print_buf(attr_buf, "shader%u", i);
   if (config_get_string(conf, attr_buf, &shader_path))
   {
      strlcpy(path_buf, dir_path, sizeof(path_buf));
      strlcat(path_buf, shader_path, sizeof(path_buf));
      free(shader_path);
   }
   else
   {
      SSNES_ERR("Didn't find shader path in config ...\n");
      return false;
   }

   SSNES_LOG("Loading Cg shader: \"%s\".\n", path_buf);

   if (!load_program(i + 1, path_buf, true))
      return false;

   return true;
}

static void load_texture_data(GLuint *obj, const struct texture_image *img, bool smooth)
{
   glGenTextures(1, obj);
   glBindTexture(GL_TEXTURE_2D, *obj);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);

   glTexImage2D(GL_TEXTURE_2D,
         0, GL_ARGB_SCE, img->width, img->height,
         0, GL_ARGB_SCE, GL_UNSIGNED_INT_8_8_8_8, img->pixels);

   free(img->pixels);
}

static bool load_textures(const char *dir_path, config_file_t *conf)
{
   bool ret = true;
   char *textures = NULL;
   if (!config_get_string(conf, "textures", &textures)) // No textures here ...
      return true;

   const char *id = strtok(textures, ";");;
   while (id && lut_textures_num < MAX_TEXTURES)
   {
      char path[PATH_MAX];
      if (!config_get_array(conf, id, path, sizeof(path)))
      {
         SSNES_ERR("Cannot find path to texture \"%s\" ...\n", id);
         ret = false;
         goto end;
      }

      char id_filter[64];
      print_buf(id_filter, "%s_linear", id);

      bool smooth = true;
      if (!config_get_bool(conf, id_filter, &smooth))
         smooth = true;

      char id_absolute[64];
      print_buf(id_absolute, "%s_absolute", id);

      bool absolute = false;
      if (!config_get_bool(conf, id_absolute, &absolute))
         absolute = false;

      char image_path[512];
      if (absolute)
         print_buf(image_path, "%s", path);
      else
         print_buf(image_path, "%s%s", dir_path, path);

      SSNES_LOG("Loading image from: \"%s\".\n", image_path);
      struct texture_image img;
      if (!texture_image_load(image_path, &img))
      {
         SSNES_ERR("Failed to load picture ...\n");
         ret = false;
         goto end;
      }

      strlcpy(lut_textures_uniform[lut_textures_num],
            id, sizeof(lut_textures_uniform[lut_textures_num]));

      load_texture_data(&lut_textures[lut_textures_num], &img, smooth);
      lut_textures_num++;

      id = strtok(NULL, ";");
   }

end:
   free(textures);
   glBindTexture(GL_TEXTURE_2D, 0);
   return ret;
}

static bool load_imports(const char *dir_path, config_file_t *conf)
{
   bool ret = true;
   char *imports = NULL;

   if (!config_get_string(conf, "imports", &imports))
      return true;

   struct snes_tracker_uniform_info info[MAX_VARIABLES];
   unsigned info_cnt = 0;
   struct snes_tracker_info tracker_info = {0};

   const char *id = strtok(imports, ";");
   while (id && info_cnt < MAX_VARIABLES)
   {
      char semantic_buf[64];
      char wram_buf[64];
      char input_slot_buf[64];
      char apuram_buf[64];
      char oam_buf[64];
      char cgram_buf[64];
      char vram_buf[64];
      char mask_buf[64];
      char equal_buf[64];

      print_buf(semantic_buf, "%s_semantic", id);
      print_buf(wram_buf, "%s_wram", id);
      print_buf(input_slot_buf, "%s_input_slot", id);
      print_buf(apuram_buf, "%s_apuram", id);
      print_buf(oam_buf, "%s_oam", id);
      print_buf(cgram_buf, "%s_cgram", id);
      print_buf(vram_buf, "%s_vram", id);
      print_buf(mask_buf, "%s_mask", id);
      print_buf(equal_buf, "%s_equal", id);

      char *semantic = NULL;

      config_get_string(conf, semantic_buf, &semantic);
   
      if (!semantic)
      {
         SSNES_ERR("No semantic for import variable.\n");
         ret = false;
         goto end;
      }

      enum snes_tracker_type tracker_type;
      enum snes_ram_type ram_type = SSNES_STATE_NONE;

      if (strcmp(semantic, "capture") == 0)
         tracker_type = SSNES_STATE_CAPTURE;
      else if (strcmp(semantic, "transition") == 0)
         tracker_type = SSNES_STATE_TRANSITION;
      else if (strcmp(semantic, "transition_count") == 0)
         tracker_type = SSNES_STATE_TRANSITION_COUNT;
      else if (strcmp(semantic, "capture_previous") == 0)
         tracker_type = SSNES_STATE_CAPTURE_PREV;
      else if (strcmp(semantic, "transition_previous") == 0)
         tracker_type = SSNES_STATE_TRANSITION_PREV;
      else
      {
         SSNES_ERR("Invalid semantic.\n");
         ret = false;
         goto end;
      }

      unsigned addr = 0;
      unsigned input_slot = 0;
      if (config_get_hex(conf, input_slot_buf, &input_slot))
      {
	      switch (input_slot)
	      {
		      case 1:
			      ram_type = SSNES_STATE_INPUT_SLOT1;
			      break;

		      case 2:
			      ram_type = SSNES_STATE_INPUT_SLOT2;
			      break;

		      default:
			      SSNES_ERR("Invalid input slot for import.\n");
			      ret = false;
			      goto end;
	      }
      }
      else if (config_get_hex(conf, wram_buf, &addr))
	      ram_type = SSNES_STATE_WRAM;
      else if (config_get_hex(conf, apuram_buf, &addr))
	      ram_type = SSNES_STATE_APURAM;
      else if (config_get_hex(conf, oam_buf, &addr))
	      ram_type = SSNES_STATE_OAM;
      else if (config_get_hex(conf, cgram_buf, &addr))
	      ram_type = SSNES_STATE_CGRAM;
      else if (config_get_hex(conf, vram_buf, &addr))
	      ram_type = SSNES_STATE_VRAM;
      else
      {
	      SSNES_ERR("No address assigned to semantic.\n");
	      ret = false;
	      goto end;
      }

      unsigned memtype;
      switch (ram_type)
      {
         case SSNES_STATE_WRAM:
            memtype = SNES_MEMORY_WRAM;
            break;
         case SSNES_STATE_APURAM:
            memtype = SNES_MEMORY_APURAM;
            break;
         case SSNES_STATE_VRAM:
            memtype = SNES_MEMORY_VRAM;
            break;
         case SSNES_STATE_OAM:
            memtype = SNES_MEMORY_OAM;
            break;
         case SSNES_STATE_CGRAM:
            memtype = SNES_MEMORY_CGRAM;
            break;

         default:
            memtype = -1u;
      }

      if ((memtype != -1u) && (addr >= psnes_get_memory_size(memtype)))
      {
         SSNES_ERR("Address out of bounds.\n");
         ret = false;
         goto end;
      }

      unsigned bitmask = 0;
      if (!config_get_hex(conf, mask_buf, &bitmask))
         bitmask = 0;
      unsigned bitequal = 0;
      if (!config_get_hex(conf, equal_buf, &bitequal))
         bitequal = 0;

      strlcpy(info[info_cnt].id, id, sizeof(info[info_cnt].id));
      info[info_cnt].addr = addr;
      info[info_cnt].type = tracker_type;
      info[info_cnt].ram_type = ram_type;
      info[info_cnt].mask = bitmask;
      info[info_cnt].equal = bitequal;

      info_cnt++;
      free(semantic);

      id = strtok(NULL, ";");
   }

   tracker_info.wram = psnes_get_memory_data(SNES_MEMORY_WRAM);
   tracker_info.vram = psnes_get_memory_data(SNES_MEMORY_VRAM);
   tracker_info.cgram = psnes_get_memory_data(SNES_MEMORY_CGRAM);
   tracker_info.oam = psnes_get_memory_data(SNES_MEMORY_OAM);
   tracker_info.apuram = psnes_get_memory_data(SNES_MEMORY_APURAM);
   tracker_info.info = info;
   tracker_info.info_elem = info_cnt;

   snes_tracker = snes_tracker_init(&tracker_info);
   if (!snes_tracker)
      SSNES_WARN("Failed to init SNES tracker.\n");

end:
   free(imports);
   return ret;
}

static bool load_preset(const char *path)
{
   bool ret = true;

   if (!load_stock())
      return false;

   int shaders = 0;
   // Basedir.
   char dir_path[PATH_MAX];
   char *ptr = NULL;

   SSNES_LOG("Loading Cg meta-shader: %s\n", path);
   config_file_t *conf = config_file_new(path);
   if (!conf)
   {
      SSNES_ERR("Failed to load preset.\n");
      ret = false;
      goto end;
   }

   if (!config_get_int(conf, "shaders", &shaders))
   {
      SSNES_ERR("Cannot find \"shaders\" param.\n");
      ret = false;
      goto end;
   }

   if (shaders < 1)
   {
      SSNES_ERR("Need to define at least 1 shader.\n");
      ret = false;
      goto end;
   }

   cg_shader_num = shaders;
   if (shaders > SSNES_CG_MAX_SHADERS - 3)
   {
      SSNES_WARN("Too many shaders ... Capping shader amount to %d.\n", SSNES_CG_MAX_SHADERS - 3);
      cg_shader_num = shaders = SSNES_CG_MAX_SHADERS - 3;
   }
   // If we aren't using last pass non-FBO shader, 
   // this shader will be assumed to be "fixed-function".
   // Just use prg[0] for that pass, which will be
   // pass-through.
   prg[shaders + 1] = prg[0]; 

   // Check filter params.
   for (int i = 0; i < shaders; i++)
   {
      bool smooth = false;
      char filter_name_buf[64];
      print_buf(filter_name_buf, "filter_linear%u", i);
      if (config_get_bool(conf, filter_name_buf, &smooth))
         fbo_smooth[i + 1] = smooth ? FILTER_LINEAR : FILTER_NEAREST;
   }

   strlcpy(dir_path, path, sizeof(dir_path));
   ptr = strrchr(dir_path, '/');
   if (!ptr) ptr = strrchr(dir_path, '\\');
   if (ptr) 
      ptr[1] = '\0';
   else // No directory.
      dir_path[0] = '\0';

   for (int i = 0; i < shaders; i++)
   {
      if (!load_shader_params(i, conf))
      {
         SSNES_ERR("Failed to load shader params ...\n");
         ret = false;
         goto end;
      }

      if (!load_shader(dir_path, i, conf))
      {
         SSNES_ERR("Failed to load shaders ...\n");
         ret = false;
         goto end;
      }
   }

   if (!load_textures(dir_path, conf))
   {
      SSNES_ERR("Failed to load lookup textures ...\n");
      ret = false;
      goto end;
   }

   if (!load_imports(dir_path, conf))
   {
      SSNES_ERR("Failed to load imports ...\n");
      ret = false;
      goto end;
   }

end:
   if (conf)
      config_file_free(conf);
   return ret;

}

static bool load_plain(const char *path)
{
   if (!load_stock())
      return false;

   SSNES_LOG("Loading Cg file: %s\n", path);

   if (!load_program(1, path, true))
      return false;

   if (*g_settings.video.second_pass_shader && g_settings.video.render_to_texture)
   {
      if (!load_program(2, g_settings.video.second_pass_shader, true))
         return false;

      cg_shader_num = 2;
   }
   else
   {
      prg[2] = prg[0];
      cg_shader_num = 1;
   }

   return true;
}

static bool load_menu_shader(void)
{
   return load_program(SSNES_CG_MENU_SHADER_INDEX, menu_cg_program, true);
}

static void set_program_attributes(unsigned i)
{
   cgGLBindProgram(prg[i].fprg);
   cgGLBindProgram(prg[i].vprg);

   prg[i].vid_size_f = cgGetNamedParameter(prg[i].fprg, "IN.video_size");
   prg[i].tex_size_f = cgGetNamedParameter(prg[i].fprg, "IN.texture_size");
   prg[i].out_size_f = cgGetNamedParameter(prg[i].fprg, "IN.output_size");
   prg[i].frame_cnt_f = cgGetNamedParameter(prg[i].fprg, "IN.frame_count");
   prg[i].frame_dir_f = cgGetNamedParameter(prg[i].fprg, "IN.frame_direction");
   prg[i].vid_size_v = cgGetNamedParameter(prg[i].vprg, "IN.video_size");
   prg[i].tex_size_v = cgGetNamedParameter(prg[i].vprg, "IN.texture_size");
   prg[i].out_size_v = cgGetNamedParameter(prg[i].vprg, "IN.output_size");
   prg[i].frame_cnt_v = cgGetNamedParameter(prg[i].vprg, "IN.frame_count");
   prg[i].frame_dir_v = cgGetNamedParameter(prg[i].vprg, "IN.frame_direction");
   prg[i].mvp = cgGetNamedParameter(prg[i].vprg, "modelViewProj");
   if (prg[i].mvp)
      cgGLSetStateMatrixParameter(prg[i].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);

   if (i == SSNES_CG_MENU_SHADER_INDEX)
      return;

   prg[i].orig.tex = cgGetNamedParameter(prg[i].fprg, "ORIG.texture");
   prg[i].orig.vid_size_v = cgGetNamedParameter(prg[i].vprg, "ORIG.video_size");
   prg[i].orig.vid_size_f = cgGetNamedParameter(prg[i].fprg, "ORIG.video_size");
   prg[i].orig.tex_size_v = cgGetNamedParameter(prg[i].vprg, "ORIG.texture_size");
   prg[i].orig.tex_size_f = cgGetNamedParameter(prg[i].fprg, "ORIG.texture_size");
   prg[i].orig.coord = cgGetNamedParameter(prg[i].vprg, "ORIG.tex_coord");

   for (unsigned j = 0; j < PREV_TEXTURES; j++)
   {
      char attr_buf_tex[64];
      char attr_buf_vid_size[64];
      char attr_buf_tex_size[64];
      char attr_buf_coord[64];
      static const char *prev_names[PREV_TEXTURES] = {
         "PREV",
         "PREV1",
         "PREV2",
         "PREV3",
         "PREV4",
         "PREV5",
         "PREV6",
      };

      snprintf(attr_buf_tex,      sizeof(attr_buf_tex),      "%s.texture", prev_names[j]);
      snprintf(attr_buf_vid_size, sizeof(attr_buf_vid_size), "%s.video_size", prev_names[j]);
      snprintf(attr_buf_tex_size, sizeof(attr_buf_tex_size), "%s.texture_size", prev_names[j]);
      snprintf(attr_buf_coord,    sizeof(attr_buf_coord),    "%s.tex_coord", prev_names[j]);

      prg[i].prev[j].tex = cgGetNamedParameter(prg[i].fprg, attr_buf_tex);

      prg[i].prev[j].vid_size_v = cgGetNamedParameter(prg[i].vprg, attr_buf_vid_size);
      prg[i].prev[j].vid_size_f = cgGetNamedParameter(prg[i].fprg, attr_buf_vid_size);

      prg[i].prev[j].tex_size_v = cgGetNamedParameter(prg[i].vprg, attr_buf_tex_size);
      prg[i].prev[j].tex_size_f = cgGetNamedParameter(prg[i].fprg, attr_buf_tex_size);

      prg[i].prev[j].coord = cgGetNamedParameter(prg[i].vprg, attr_buf_coord);
   }

   for (unsigned j = 0; j < i - 1; j++)
   {
      char attr_buf[64];

      snprintf(attr_buf, sizeof(attr_buf), "PASS%u.texture", j + 1);
      prg[i].fbo[j].tex = cgGetNamedParameter(prg[i].fprg, attr_buf);

      snprintf(attr_buf, sizeof(attr_buf), "PASS%u.video_size", j + 1);
      prg[i].fbo[j].vid_size_v = cgGetNamedParameter(prg[i].vprg, attr_buf);
      prg[i].fbo[j].vid_size_f = cgGetNamedParameter(prg[i].fprg, attr_buf);

      snprintf(attr_buf, sizeof(attr_buf), "PASS%u.texture_size", j + 1);
      prg[i].fbo[j].tex_size_v = cgGetNamedParameter(prg[i].vprg, attr_buf);
      prg[i].fbo[j].tex_size_f = cgGetNamedParameter(prg[i].fprg, attr_buf);

      snprintf(attr_buf, sizeof(attr_buf), "PASS%u.tex_coord", j + 1);
      prg[i].fbo[j].coord = cgGetNamedParameter(prg[i].vprg, attr_buf);
   }
}

static bool gl_cg_init(const char *path)
{
   cgRTCgcInit();

   if (!cgCtx)
      cgCtx = cgCreateContext();

   if (cgCtx == NULL)
   {
      SSNES_ERR("Failed to create Cg context\n");
      return false;
   }

   cgFProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
   cgVProf = cgGLGetLatestProfile(CG_GL_VERTEX);
   if (cgFProf == CG_PROFILE_UNKNOWN || cgVProf == CG_PROFILE_UNKNOWN)
   {
      SSNES_ERR("Invalid profile type\n");
      return false;
   }
   cgGLSetOptimalOptions(cgFProf);
   cgGLSetOptimalOptions(cgVProf);
   cgGLEnableProfile(cgFProf);
   cgGLEnableProfile(cgVProf);

   if (strstr(path, ".cgp"))
   {
      if (!load_preset(path))
         return false;
   }
   else
   {
      if (!load_plain(path))
         return false;
   }

   if (menu_cg_program && !load_menu_shader())
      return false;

   prg[0].mvp = cgGetNamedParameter(prg[0].vprg, "modelViewProj");
   if (prg[0].mvp)
      cgGLSetStateMatrixParameter(prg[0].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);

   for (unsigned i = 1; i <= cg_shader_num; i++)
      set_program_attributes(i);

   if (menu_cg_program)
      set_program_attributes(SSNES_CG_MENU_SHADER_INDEX);

   cgGLBindProgram(prg[1].fprg);
   cgGLBindProgram(prg[1].vprg);

   return true;
}

// Deinit as much as possible without resetting context (broken on PS3),
// and reinit cleanly.
// If this fails, we're kinda screwed without resetting everything on PS3.
bool gl_cg_reinit(const char *path)
{
   gl_cg_deinit_state();

   return gl_cg_init(path);
}


static void gl_cg_use(unsigned index)
{
   if (prg[index].vprg && prg[index].fprg)
   {
      gl_cg_reset_attrib();

      active_index = index;
      cgGLBindProgram(prg[index].vprg);
      cgGLBindProgram(prg[index].fprg);
   }
}

static unsigned gl_cg_num(void)
{
   return cg_shader_num;
}

static bool gl_cg_filter_type(unsigned index, bool *smooth)
{
   if (fbo_smooth[index] == FILTER_UNSPEC)
      return false;
   *smooth = (fbo_smooth[index] == FILTER_LINEAR);
   return true;
}

static void gl_cg_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
   *scale = cg_scale[index];
}

static void gl_cg_set_menu_shader(const char *path)
{
   if (menu_cg_program)
      free(menu_cg_program);

   menu_cg_program = strdup(path);
}

static void gl_cg_set_compiler_args(const char **argv)
{
   cg_arguments = argv;
}

bool gl_cg_load_shader(unsigned index, const char *path)
{
   if (index == 0)
      return false;

   if (prg[index].fprg)
   {
      cgGLUnbindProgram(cgFProf);

      if (prg[0].fprg != prg[index].fprg)
         cgDestroyProgram(prg[index].fprg);
   }

   if (prg[index].vprg)
   {
      cgGLUnbindProgram(cgVProf);

      if (prg[0].vprg != prg[index].vprg)
         cgDestroyProgram(prg[index].vprg);
   }

   memset(&prg[index], 0, sizeof(prg[index]));

   if (load_program(index, path, true))
   {
      set_program_attributes(index);
      return true;
   }
   else
   {
      // Always make sure we have a valid shader.
      memcpy(&prg[index], &prg[0], sizeof(prg[0]));
      return false;
   }
}

bool gl_cg_save_cgp(const char *path, const struct gl_cg_cgp_info *info)
{
   if (!info->shader[0] || !*info->shader[0])
      return false;

   FILE *file = fopen(path, "w");
   if (!file)
      return false;

   unsigned shaders = info->shader[1] && *info->shader[1] ? 2 : 1;
   fprintf(file, "shaders = %u\n", shaders);

   fprintf(file, "shader0 = \"%s\"\n", info->shader[0]);
   if (shaders == 2)
      fprintf(file, "shader1 = \"%s\"\n", info->shader[1]);

   fprintf(file, "filter_linear0 = %s\n", info->filter_linear[0] ? "true" : "false");

   if (info->render_to_texture)
   {
      fprintf(file, "filter_linear1 = %s\n", info->filter_linear[1] ? "true" : "false");
      fprintf(file, "scale_type0 = source\n");
      fprintf(file, "scale0 = %.1f\n", info->fbo_scale);
   }

   if (info->lut_texture_path && info->lut_texture_id)
   {
      fprintf(file, "textures = %s\n", info->lut_texture_id);
      fprintf(file, "%s = \"%s\"\n",
            info->lut_texture_id, info->lut_texture_path);

      fprintf(file, "%s_absolute = %s\n",
            info->lut_texture_id,
            info->lut_texture_absolute ? "true" : "false");
   }

   fclose(file);
   return true;
}

static void gl_cg_invalidate_context(void)
{
   cgCtx = NULL;
}

unsigned gl_cg_get_lut_info(struct gl_cg_lut_info *info, unsigned elems)
{
   elems = elems > lut_textures_num ? lut_textures_num : elems;

   for (unsigned i = 0; i < elems; i++)
   {
      strlcpy(info[i].id, lut_textures_uniform[i], sizeof(info[i].id));
      info[i].tex = lut_textures[i];
   }

   return elems;
}

/*============================================================
	GL IMPLEMENTATION
============================================================ */

static bool gl_shader_init(void)
{
	switch (g_settings.video.shader_type)
	{
		case SSNES_SHADER_AUTO:
			{
				if (strlen(g_settings.video.cg_shader_path) > 0 && strlen(g_settings.video.bsnes_shader_path) > 0)
					SSNES_WARN("Both Cg and bSNES XML shader are defined in config file. Cg shader will be selected by default.\n");

				if (strlen(g_settings.video.cg_shader_path) > 0)
					return gl_cg_init(g_settings.video.cg_shader_path);
				break;
			}

		case SSNES_SHADER_CG:
			{
				if (strlen(g_settings.video.cg_shader_path) > 0)
					return gl_cg_init(g_settings.video.cg_shader_path);
				break;
			}

		default:
			break;
	}

	return true;
}

static unsigned gl_shader_num(void)
{
	unsigned num = 0;
	unsigned cg_num = gl_cg_num();
	if (cg_num > num)
		num = cg_num;

	return num;
}

static bool gl_shader_filter_type(unsigned index, bool *smooth)
{
	bool valid = false;
	if (!valid)
		valid = gl_cg_filter_type(index, smooth);

	return valid;
}

static void gl_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
	scale->valid = false;
	if (!scale->valid)
		gl_cg_shader_scale(index, scale);
}

static void gl_create_fbo_textures(gl_t *gl)
{
	glGenTextures(gl->fbo_pass, gl->fbo_texture);

	GLuint base_filt = g_settings.video.second_pass_smooth ? GL_LINEAR : GL_NEAREST;
	for (int i = 0; i < gl->fbo_pass; i++)
	{
		glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

		GLuint filter_type = base_filt;
		bool smooth = false;
		if (gl_shader_filter_type(i + 2, &smooth))
			filter_type = smooth ? GL_LINEAR : GL_NEAREST;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_type);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_type);

		glTexImage2D(GL_TEXTURE_2D,
				0, GL_ARGB_SCE, gl->fbo_rect[i].width, gl->fbo_rect[i].height, 0, GL_ARGB_SCE,
				GL_UNSIGNED_INT_8_8_8_8, NULL);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

void gl_deinit_fbo(gl_t *gl)
{
	if (gl->fbo_inited)
	{
		glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
		glDeleteFramebuffersOES(gl->fbo_pass, gl->fbo);
		memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
		memset(gl->fbo, 0, sizeof(gl->fbo));
		gl->fbo_inited = false;
		gl->render_to_tex = false;
		gl->fbo_pass = 0;
	}
}

// Horribly long and complex FBO init :D
void gl_init_fbo(gl_t *gl, unsigned width, unsigned height)
{
	if (!g_settings.video.render_to_texture && gl_shader_num() == 0)
		return;

	struct gl_fbo_scale scale, scale_last;
	gl_shader_scale(1, &scale);
	gl_shader_scale(gl_shader_num(), &scale_last);

	// No need to use FBOs.
	if (gl_shader_num() == 1 && !scale.valid && !g_settings.video.render_to_texture)
		return;

	gl->fbo_pass = gl_shader_num() - 1;
	if (scale_last.valid)
		gl->fbo_pass++;

	if (gl->fbo_pass <= 0)
		gl->fbo_pass = 1;

	if (!scale.valid)
	{
		scale.scale_x = g_settings.video.fbo_scale_x;
		scale.scale_y = g_settings.video.fbo_scale_y;
		scale.type_x = scale.type_y = SSNES_SCALE_INPUT;
	}

	switch (scale.type_x)
	{
		case SSNES_SCALE_INPUT:
			gl->fbo_rect[0].width = width * next_pow2(ceil(scale.scale_x));
			break;

		case SSNES_SCALE_ABSOLUTE:
			gl->fbo_rect[0].width = next_pow2(scale.abs_x);
			break;

		case SSNES_SCALE_VIEWPORT:
			gl->fbo_rect[0].width = next_pow2(gl->win_width);
			break;

		default:
			break;
	}

	switch (scale.type_y)
	{
		case SSNES_SCALE_INPUT:
			gl->fbo_rect[0].height = height * next_pow2(ceil(scale.scale_y));
			break;

		case SSNES_SCALE_ABSOLUTE:
			gl->fbo_rect[0].height = next_pow2(scale.abs_y);
			break;

		case SSNES_SCALE_VIEWPORT:
			gl->fbo_rect[0].height = next_pow2(gl->win_height);
			break;

		default:
			break;
	}

	unsigned last_width = gl->fbo_rect[0].width, last_height = gl->fbo_rect[0].height;
	gl->fbo_scale[0] = scale;

	SSNES_LOG("Creating FBO 0 @ %ux%u\n", gl->fbo_rect[0].width, gl->fbo_rect[0].height);

	for (int i = 1; i < gl->fbo_pass; i++)
	{
		gl_shader_scale(i + 1, &gl->fbo_scale[i]);
		if (gl->fbo_scale[i].valid)
		{
			switch (gl->fbo_scale[i].type_x)
			{
				case SSNES_SCALE_INPUT:
					gl->fbo_rect[i].width = last_width * next_pow2(ceil(gl->fbo_scale[i].scale_x));
					break;

				case SSNES_SCALE_ABSOLUTE:
					gl->fbo_rect[i].width = next_pow2(gl->fbo_scale[i].abs_x);
					break;

				case SSNES_SCALE_VIEWPORT:
					gl->fbo_rect[i].width = next_pow2(gl->win_width);
					break;

				default:
					break;
			}

			switch (gl->fbo_scale[i].type_y)
			{
				case SSNES_SCALE_INPUT:
					gl->fbo_rect[i].height = last_height * next_pow2(ceil(gl->fbo_scale[i].scale_y));
					break;

				case SSNES_SCALE_ABSOLUTE:
					gl->fbo_rect[i].height = next_pow2(gl->fbo_scale[i].abs_y);
					break;

				case SSNES_SCALE_VIEWPORT:
					gl->fbo_rect[i].height = next_pow2(gl->win_height);
					break;

				default:
					break;
			}

			last_width = gl->fbo_rect[i].width;
			last_height = gl->fbo_rect[i].height;
		}
		else
		{
			// Use previous values, essentially a 1x scale compared to last shader in chain.
			gl->fbo_rect[i] = gl->fbo_rect[i - 1];
			gl->fbo_scale[i].scale_x = gl->fbo_scale[i].scale_y = 1.0;
			gl->fbo_scale[i].type_x = gl->fbo_scale[i].type_y = SSNES_SCALE_INPUT;
		}

		SSNES_LOG("Creating FBO %d @ %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
	}

	gl_create_fbo_textures(gl);

	glGenFramebuffersOES(gl->fbo_pass, gl->fbo);
	for (int i = 0; i < gl->fbo_pass; i++)
	{
		glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[i]);
		glFramebufferTexture2DOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

		GLenum status = glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES);
		if (status != GL_FRAMEBUFFER_COMPLETE_OES)
			goto error;
	}

	gl->fbo_inited = true;
	glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
	return;

error:
	glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
	glDeleteFramebuffersOES(gl->fbo_pass, gl->fbo);
	SSNES_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
}

static inline void gl_compute_fbo_geometry(gl_t *gl, unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
	unsigned last_width = width;
	unsigned last_height = height;
	unsigned last_max_width = gl->tex_w;
	unsigned last_max_height = gl->tex_h;
	// Calculate viewports for FBOs.
	for (int i = 0; i < gl->fbo_pass; i++)
	{
		switch (gl->fbo_scale[i].type_x)
		{
			case SSNES_SCALE_INPUT:
				gl->fbo_rect[i].img_width = last_width * gl->fbo_scale[i].scale_x;
				gl->fbo_rect[i].max_img_width = last_max_width * gl->fbo_scale[i].scale_x;
				break;

			case SSNES_SCALE_ABSOLUTE:
				gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].abs_x;
				break;

			case SSNES_SCALE_VIEWPORT:
				gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].scale_x * gl->vp_out_width;
				break;

			default:
				break;
		}

		switch (gl->fbo_scale[i].type_y)
		{
			case SSNES_SCALE_INPUT:
				gl->fbo_rect[i].img_height = last_height * gl->fbo_scale[i].scale_y;
				gl->fbo_rect[i].max_img_height = last_max_height * gl->fbo_scale[i].scale_y;
				break;

			case SSNES_SCALE_ABSOLUTE:
				gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].abs_y;
				break;

			case SSNES_SCALE_VIEWPORT:
				gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].scale_y * gl->vp_out_height;
				break;

			default:
				break;
		}

		last_width = gl->fbo_rect[i].img_width;
		last_height = gl->fbo_rect[i].img_height;
		last_max_width = gl->fbo_rect[i].max_img_width;
		last_max_height = gl->fbo_rect[i].max_img_height;
	}
}

static void set_viewport(gl_t *gl, unsigned width, unsigned height, bool force_full)
{

	uint32_t m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp;
	GLfloat m_left, m_right, m_bottom, m_top, m_zNear, m_zFar;

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	m_viewport_x_temp = 0;
	m_viewport_y_temp = 0;
	m_viewport_width_temp = width;
	m_viewport_height_temp = height;

	m_left = 0.0f;
	m_right = 1.0f;
	m_bottom = 0.0f;
	m_top = 1.0f;
	m_zNear = -1.0f;
	m_zFar = 1.0f;

	if (gl->keep_aspect && !force_full)
	{
		float desired_aspect = g_settings.video.aspect_ratio;
		float device_aspect = (float)width / height;
		float delta;

		// If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
		if(g_console.aspect_ratio_index == ASPECT_RATIO_CUSTOM)
		{
			delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
			m_viewport_x_temp = g_console.custom_viewport_x;
			m_viewport_y_temp = g_console.custom_viewport_y;
			m_viewport_width_temp = g_console.custom_viewport_width;
			m_viewport_height_temp = g_console.custom_viewport_height;
		}
		else if (device_aspect > desired_aspect)
		{
			delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
			m_viewport_x_temp = (GLint)(width * (0.5 - delta));
			m_viewport_width_temp = (GLint)(2.0 * width * delta);
			width = (unsigned)(2.0 * width * delta);
		}
		else
		{
			delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
			m_viewport_y_temp = (GLint)(height * (0.5 - delta));
			m_viewport_height_temp = (GLint)(2.0 * height * delta);
			height = (unsigned)(2.0 * height * delta);
		}
	}

	glViewport(m_viewport_x_temp, m_viewport_y_temp, m_viewport_width_temp, m_viewport_height_temp);

	if(gl->overscan_enable && !force_full)
	{
		m_left = -gl->overscan_amount/2;
		m_right = 1 + gl->overscan_amount/2;
		m_bottom = -gl->overscan_amount/2;
	}

	glOrthof(m_left, m_right, m_bottom, m_top, m_zNear, m_zFar);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	if (prg[active_index].mvp)
		cgGLSetStateMatrixParameter(prg[active_index].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);

	gl->vp_width = width;
	gl->vp_height = height;

	// Set last backbuffer viewport.
	if (!force_full)
	{
		gl->vp_out_width = width;
		gl->vp_out_height = height;
	}
}

static void set_lut_texture_coords(const GLfloat *coords)
{
	// For texture images.
	pglClientActiveTexture(GL_TEXTURE1);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, coords);
	pglClientActiveTexture(GL_TEXTURE0);
}

#define set_texture_coords(coords, xamt, yamt) \
   coords[1] = yamt; \
   coords[4] = xamt; \
   coords[6] = xamt; \
   coords[7] = yamt;

void gl_frame_menu (void)
{
	gl_t *gl = g_gl;

	g_frame_count++;

	if(!gl)
		return;


	gl_cg_use(SSNES_CG_MENU_SHADER_INDEX);

	gl_cg_set_params(gl->win_width, gl->win_height, gl->win_width, 
			gl->win_height, gl->win_width, gl->win_height, g_frame_count,
			NULL, NULL, NULL, 0);

	set_viewport(gl, gl->win_width, gl->win_height, true);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl->menu_texture_id);

	glDrawArrays(GL_QUADS, 0, 4); 

	glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

static void ps3graphics_set_orientation(void * data, uint32_t orientation)
{
	(void)data;
	switch (orientation)
	{
		case ORIENTATION_NORMAL:
			vertex_ptr = vertexes_flipped;
			break;

		case ORIENTATION_VERTICAL:
			vertex_ptr = vertexes_90;
			break;

		case ORIENTATION_FLIPPED:
			vertex_ptr = vertexes_180;
			break;

		case ORIENTATION_FLIPPED_ROTATED:
			vertex_ptr = vertexes_270;
			break;
	}

	glVertexPointer(2, GL_FLOAT, 0, vertex_ptr);
}

static bool gl_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
	gl_t *gl = data;

	gl_cg_use(1);
	g_frame_count++;

	glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

	// Render to texture in first pass.
	if (gl->fbo_inited)
	{
		gl_compute_fbo_geometry(gl, width, height, gl->vp_out_width, gl->vp_out_height);
		glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
		glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[0]);
		gl->render_to_tex = true;
		set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true);
	}


	if ((width != gl->last_width[gl->tex_index] || height != gl->last_height[gl->tex_index]) && gl->empty_buf) // Res change. need to clear out texture.
	{
		gl->last_width[gl->tex_index] = width;
		gl->last_height[gl->tex_index] = height;

		glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
				gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
				gl->tex_w * gl->tex_h * gl->base_size,
				gl->empty_buf);

		GLfloat xamt = (GLfloat)width / gl->tex_w;
		GLfloat yamt = (GLfloat)height / gl->tex_h;

		set_texture_coords(gl->tex_coords, xamt, yamt);
	}
	// We might have used different texture coordinates last frame. Edge case if resolution changes very rapidly.
	else if (width != gl->last_width[(gl->tex_index - 1) & TEXTURES_MASK] || height != gl->last_height[(gl->tex_index - 1) & TEXTURES_MASK])
	{
		GLfloat xamt = (GLfloat)width / gl->tex_w;
		GLfloat yamt = (GLfloat)height / gl->tex_h;
		set_texture_coords(gl->tex_coords, xamt, yamt);
	}

	// Need to preserve the "flipped" state when in FBO as well to have 
	// consistent texture coordinates.
	if (gl->render_to_tex)
		glVertexPointer(2, GL_FLOAT, 0, vertexes);

	{
		size_t buffer_addr = gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size;
		size_t buffer_stride = gl->tex_w * gl->base_size;
		const uint8_t *frame_copy = frame;
		size_t frame_copy_size = width * gl->base_size;
		for (unsigned h = 0; h < height; h++)
		{
			glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE, 
					buffer_addr,
					frame_copy_size,
					frame_copy);

			frame_copy += pitch;
			buffer_addr += buffer_stride;
		}
	}

	struct gl_tex_info tex_info = {
		.tex = gl->texture[gl->tex_index],
		.input_size = {width, height},
		.tex_size = {gl->tex_w, gl->tex_h}
	};
	struct gl_tex_info fbo_tex_info[MAX_SHADERS];
	unsigned fbo_tex_info_cnt = 0;
	memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

	glClear(GL_COLOR_BUFFER_BIT);
	gl_cg_set_params(width, height, 
			gl->tex_w, gl->tex_h, 
			gl->vp_width, gl->vp_height, 
			g_frame_count, &tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

	glDrawArrays(GL_QUADS, 0, 4);

	if (gl->fbo_inited)
	{
		// Render the rest of our passes.
		glTexCoordPointer(2, GL_FLOAT, 0, gl->fbo_tex_coords);

		// It's kinda handy ... :)
		const struct gl_fbo_rect *prev_rect;
		const struct gl_fbo_rect *rect;
		struct gl_tex_info *fbo_info;

		// Calculate viewports, texture coordinates etc, and render all passes from FBOs, to another FBO.
		for (int i = 1; i < gl->fbo_pass; i++)
		{
			prev_rect = &gl->fbo_rect[i - 1];
			rect = &gl->fbo_rect[i];
			fbo_info = &fbo_tex_info[i - 1];

			GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
			GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

			set_texture_coords(gl->fbo_tex_coords, xamt, yamt);

			fbo_info->tex = gl->fbo_texture[i - 1];
			fbo_info->input_size[0] = prev_rect->img_width;
			fbo_info->input_size[1] = prev_rect->img_height;
			fbo_info->tex_size[0] = prev_rect->width;
			fbo_info->tex_size[1] = prev_rect->height;
			memcpy(fbo_info->coord, gl->fbo_tex_coords, sizeof(gl->fbo_tex_coords));

			glBindFramebufferOES(GL_FRAMEBUFFER_OES, gl->fbo[i]);
			gl_cg_use(i + 1);
			glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

			glClear(GL_COLOR_BUFFER_BIT);

			// Render to FBO with certain size.
			set_viewport(gl, rect->img_width, rect->img_height, true);
			gl_cg_set_params(prev_rect->img_width, prev_rect->img_height, 
					prev_rect->width, prev_rect->height, 
					gl->vp_width, gl->vp_height, g_frame_count, 
					&tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

			glDrawArrays(GL_QUADS, 0, 4);

			fbo_tex_info_cnt++;
		}

		// Render our last FBO texture directly to screen.
		prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
		GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
		GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

		set_texture_coords(gl->fbo_tex_coords, xamt, yamt);

		// Render our FBO texture to back buffer.
		glBindFramebufferOES(GL_FRAMEBUFFER_OES, 0);
		gl_cg_use(gl->fbo_pass + 1);

		glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

		glClear(GL_COLOR_BUFFER_BIT);
		gl->render_to_tex = false;
		set_viewport(gl, gl->win_width, gl->win_height, false);
		gl_cg_set_params(prev_rect->img_width, prev_rect->img_height, 
				prev_rect->width, prev_rect->height, 
				gl->vp_width, gl->vp_height, g_frame_count, 
				&tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

		glVertexPointer(2, GL_FLOAT, 0, vertex_ptr);
		glDrawArrays(GL_QUADS, 0, 4);

		glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);
	}

	memmove(gl->prev_info + 1, gl->prev_info, sizeof(tex_info) * (TEXTURES - 1));
	memcpy(&gl->prev_info[0], &tex_info, sizeof(tex_info));
	gl->tex_index = (gl->tex_index + 1) & TEXTURES_MASK;

	if (msg)
	{
		cellDbgFontPrintf(g_settings.video.msg_pos_x, g_settings.video.msg_pos_y, 1.11f, BLUE,	msg);
		cellDbgFontPrintf(g_settings.video.msg_pos_x, g_settings.video.msg_pos_y, 1.10f, WHITE, msg);
		cellDbgFontDraw();
	}

	if(!gl->block_swap)
		psglSwap();
	return true;
}

static void psgl_deinit(gl_t *gl)
{
	glFinish();
	cellDbgFontExit();

	psglDestroyContext(gl->gl_context);
	psglDestroyDevice(gl->gl_device);

	psglExit();
}

static void gl_free(void *data)
{
	if (g_gl)
		return;

	gl_t *gl = data;

	gl_cg_deinit();

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDeleteTextures(TEXTURES, gl->texture);
	glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
	glDeleteBuffers(1, &gl->pbo);

	gl_deinit_fbo(gl);
	psgl_deinit(gl);

	if (gl->empty_buf)
		free(gl->empty_buf);

	free(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
	gl_t *gl = data;
	if (gl->vsync)
	{
		SSNES_LOG("GL VSync => %s\n", state ? "off" : "on");
		if(state)
			glDisable(GL_VSYNC_SCE);
		else
			glEnable(GL_VSYNC_SCE);
	}
}

static bool psgl_init_device(gl_t *gl, const video_info_t *video, uint32_t resolution_id)
{
	PSGLinitOptions options = {
		.enable = PSGL_INIT_MAX_SPUS | PSGL_INIT_INITIALIZE_SPUS,
		.maxSPUs = 1,
		.initializeSPUs = GL_FALSE,
	};
#if CELL_SDK_VERSION < 0x340000
	options.enable |=	PSGL_INIT_HOST_MEMORY_SIZE;
#endif

	// Initialize 6 SPUs but reserve 1 SPU as a raw SPU for PSGL
	sys_spu_initialize(6, 1);
	psglInit(&options);

	PSGLdeviceParameters params;

	params.enable = PSGL_DEVICE_PARAMETERS_COLOR_FORMAT | \
			PSGL_DEVICE_PARAMETERS_DEPTH_FORMAT | \
			PSGL_DEVICE_PARAMETERS_MULTISAMPLING_MODE;
	params.colorFormat = GL_ARGB_SCE;
	params.depthFormat = GL_NONE;
	params.multisamplingMode = GL_MULTISAMPLING_NONE_SCE;

	if(g_console.triple_buffering_enable)
	{
		params.enable |= PSGL_DEVICE_PARAMETERS_BUFFERING_MODE;
		params.bufferingMode = PSGL_BUFFERING_MODE_TRIPLE;
	}

	if(resolution_id)
	{
		CellVideoOutResolution resolution;
		cellVideoOutGetResolution(resolution_id, &resolution);

		params.enable |= PSGL_DEVICE_PARAMETERS_WIDTH_HEIGHT;
		params.width = resolution.width;
		params.height = resolution.height;
	}

	gl->gl_device = psglCreateDeviceExtended(&params);
	psglGetDeviceDimensions(gl->gl_device, &gl->win_width, &gl->win_height); 

	if(g_console.custom_viewport_width == 0)
		g_console.custom_viewport_width = gl->win_width;
	if(g_console.custom_viewport_height == 0)
		g_console.custom_viewport_height = gl->win_height;

	gl->gl_context = psglCreateContext();
	psglMakeCurrent(gl->gl_context, gl->gl_device);
	psglResetCurrentContext();

	return true;
}

static void psgl_init_dbgfont(gl_t *gl)
{
	CellDbgFontConfig cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.bufSize = 512;
	cfg.screenWidth = gl->win_width;
	cfg.screenHeight = gl->win_height;
	cellDbgFontInit(&cfg);
}

static void *gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
	if (g_gl)
		return g_gl;

	gl_t *gl = calloc(1, sizeof(gl_t));
	if (!gl)
		return NULL;

	if (!psgl_init_device(gl, video, g_console.current_resolution_id))
		return NULL;


	SSNES_LOG("Detecting resolution %ux%u.\n", gl->win_width, gl->win_height);

	video->vsync ? glEnable(GL_VSYNC_SCE) : glDisable(GL_VSYNC_SCE);

	gl->vsync = video->vsync;

	SSNES_LOG("GL: Using resolution %ux%u.\n", gl->win_width, gl->win_height);

	SSNES_LOG("GL: Initializing debug fonts...\n");
	psgl_init_dbgfont(gl);

	SSNES_LOG("Initializing menu shader...\n");
	gl_cg_set_menu_shader(DEFAULT_MENU_SHADER_FILE);

	if (!gl_shader_init())
	{
		SSNES_ERR("Menu shader initialization failed.\n");
		psgl_deinit(gl);
		free(gl);
		return NULL;
	}

	SSNES_LOG("GL: Loaded %u program(s).\n", gl_shader_num());

	// Set up render to texture.
	gl_init_fbo(gl, SSNES_SCALE_BASE * video->input_scale,
			SSNES_SCALE_BASE * video->input_scale);


	gl->keep_aspect = video->force_aspect;

	// Apparently need to set viewport for passes when we aren't using FBOs.
	gl_cg_use(0);
	set_viewport(gl, gl->win_width, gl->win_height, false);
	gl_cg_use(1);
	set_viewport(gl, gl->win_width, gl->win_height, false);

	bool force_smooth = false;
	if (gl_shader_filter_type(1, &force_smooth))
		gl->tex_filter = force_smooth ? GL_LINEAR : GL_NEAREST;
	else
		gl->tex_filter = video->smooth ? GL_LINEAR : GL_NEAREST;

	gl->texture_type = GL_BGRA;
	gl->texture_fmt = video->rgb32 ? GL_ARGB_SCE : GL_RGB5_A1;
	gl->base_size = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

	glEnable(GL_TEXTURE_2D);
	glClearColor(0, 0, 0, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gl->tex_w = SSNES_SCALE_BASE * video->input_scale;
	gl->tex_h = SSNES_SCALE_BASE * video->input_scale;
	glGenBuffers(1, &gl->pbo);
	glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
	glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->tex_w * gl->tex_h * gl->base_size * TEXTURES, NULL, GL_STREAM_DRAW);

	glGenTextures(TEXTURES, gl->texture);

	for (unsigned i = 0; i < TEXTURES; i++)
	{
		glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vertex_ptr);

	memcpy(gl->tex_coords, tex_coords, sizeof(tex_coords));
	glTexCoordPointer(2, GL_FLOAT, 0, gl->tex_coords);

	glColorPointer(4, GL_FLOAT, 0, white_color);

	set_lut_texture_coords(tex_coords);

	// Empty buffer that we use to clear out the texture with on res change.
	gl->empty_buf = calloc(gl->tex_w * gl->tex_h, gl->base_size);

	for (unsigned i = 0; i < TEXTURES; i++)
	{
		glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
		glTextureReferenceSCE(GL_TEXTURE_2D, 1,
				gl->tex_w, gl->tex_h, 0, 
				gl->texture_fmt,
				gl->tex_w * gl->base_size,
				gl->tex_w * gl->tex_h * i * gl->base_size);
	}
	glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

	for (unsigned i = 0; i < TEXTURES; i++)
	{
		gl->last_width[i] = gl->tex_w;
		gl->last_height[i] = gl->tex_h;
	}

	for (unsigned i = 0; i < TEXTURES; i++)
	{
		gl->prev_info[i].tex = gl->texture[(gl->tex_index - (i + 1)) & TEXTURES_MASK];
		gl->prev_info[i].input_size[0] = gl->tex_w;
		gl->prev_info[i].tex_size[0] = gl->tex_w;
		gl->prev_info[i].input_size[1] = gl->tex_h;
		gl->prev_info[i].tex_size[1] = gl->tex_h;
		memcpy(gl->prev_info[i].coord, tex_coords, sizeof(tex_coords)); 
	}

	if (!gl_check_error())
	{
		psgl_deinit(gl);
		free(gl);
		return NULL;
	}

	if (input)
		*input = NULL;
	if (input_data)
		*input_data = NULL;

	return gl;
}

static bool gl_alive(void *data)
{
	(void)data;
	cellSysutilCheckCallback();
	return !g_quitting;
}

static bool gl_focus(void *data)
{
	(void)data;
	return true;
}

static void ps3graphics_set_swap_block_swap(void * data, bool toggle)
{
	(void)data;
	gl_t *gl = g_gl;
	gl->block_swap = toggle;
}

static void ps3graphics_swap(void * data)
{
	(void)data;
	psglSwap();
	cellSysutilCheckCallback();
}

static void ps3graphics_set_aspect_ratio(void * data, uint32_t aspectratio_index)
{
	(void)data;
	gl_t * gl = g_gl;

	switch(aspectratio_index)
	{
		case ASPECT_RATIO_4_3:
			g_settings.video.aspect_ratio = 1.33333333333;
			strlcpy(g_console.aspect_ratio_name, "4:3", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_4_4:
			g_settings.video.aspect_ratio = 1.0;
			strlcpy(g_console.aspect_ratio_name, "4:4", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_4_1:
			g_settings.video.aspect_ratio = 4.0;
			strlcpy(g_console.aspect_ratio_name, "4:1", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_5_4:
			g_settings.video.aspect_ratio = 1.25;
			strlcpy(g_console.aspect_ratio_name, "5:4", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_6_5:
			g_settings.video.aspect_ratio = 1.2;
			strlcpy(g_console.aspect_ratio_name, "6:5", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_7_9:
			g_settings.video.aspect_ratio = 0.77777777777;
			strlcpy(g_console.aspect_ratio_name, "7:9", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_8_3:
			g_settings.video.aspect_ratio = 2.66666666666;
			strlcpy(g_console.aspect_ratio_name, "8:3", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_8_7:
			g_settings.video.aspect_ratio = 1.14287142857;
			strlcpy(g_console.aspect_ratio_name, "8:7", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_16_9:
			g_settings.video.aspect_ratio = 1.777778;
			strlcpy(g_console.aspect_ratio_name, "16:9", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_16_10:
			g_settings.video.aspect_ratio = 1.6;
			strlcpy(g_console.aspect_ratio_name, "16:10", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_16_15:
			g_settings.video.aspect_ratio = 3.2;
			strlcpy(g_console.aspect_ratio_name, "16:15", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_19_12:
			g_settings.video.aspect_ratio = 1.58333333333;
			strlcpy(g_console.aspect_ratio_name, "19:12", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_19_14:
			g_settings.video.aspect_ratio = 1.35714285714;
			strlcpy(g_console.aspect_ratio_name, "19:14", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_30_17:
			g_settings.video.aspect_ratio = 1.76470588235;
			strlcpy(g_console.aspect_ratio_name, "30:17", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_32_9:
			g_settings.video.aspect_ratio = 3.55555555555;
			strlcpy(g_console.aspect_ratio_name, "32:9", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_2_1:
			g_settings.video.aspect_ratio = 2.0;
			strlcpy(g_console.aspect_ratio_name, "2:1", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_3_2:
			g_settings.video.aspect_ratio = 1.5;
			strlcpy(g_console.aspect_ratio_name, "3:2", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_3_4:
			g_settings.video.aspect_ratio = 0.75;
			strlcpy(g_console.aspect_ratio_name, "3:4", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_1_1:
			g_settings.video.aspect_ratio = 1.0;
			strlcpy(g_console.aspect_ratio_name, "1:1", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_AUTO:
			strlcpy(g_console.aspect_ratio_name, "(Auto)", sizeof(g_console.aspect_ratio_name));
			break;
		case ASPECT_RATIO_CUSTOM:
			strlcpy(g_console.aspect_ratio_name, "(Custom)", sizeof(g_console.aspect_ratio_name));
			break;
	}
	g_settings.video.force_aspect = false;
	gl->keep_aspect = true;
	set_viewport(gl, gl->win_width, gl->win_height, false);
}

const video_driver_t video_gl = 
{
	.init = gl_init,
	.frame = gl_frame,
	.alive = gl_alive,
	.set_nonblock_state = gl_set_nonblock_state,
	.focus = gl_focus,
	.free = gl_free,
	.ident = "gl",
	.set_swap_block_state = ps3graphics_set_swap_block_swap,
	.set_rotation = ps3graphics_set_orientation,
	.set_aspect_ratio = ps3graphics_set_aspect_ratio,
	.swap = ps3graphics_swap
};

static void get_all_available_resolutions (void)
{
	bool defaultresolution;
	uint32_t i, resolution_count;
	uint16_t num_videomodes;

	defaultresolution = true;

	uint32_t videomode[] = {
		CELL_VIDEO_OUT_RESOLUTION_480, CELL_VIDEO_OUT_RESOLUTION_576,
		CELL_VIDEO_OUT_RESOLUTION_960x1080, CELL_VIDEO_OUT_RESOLUTION_720,
		CELL_VIDEO_OUT_RESOLUTION_1280x1080, CELL_VIDEO_OUT_RESOLUTION_1440x1080,
		CELL_VIDEO_OUT_RESOLUTION_1600x1080, CELL_VIDEO_OUT_RESOLUTION_1080};

	num_videomodes = sizeof(videomode)/sizeof(uint32_t);

	resolution_count = 0;
	for (i = 0; i < num_videomodes; i++)
		if (cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, videomode[i], CELL_VIDEO_OUT_ASPECT_AUTO,0))
			resolution_count++;
	
	g_console.supported_resolutions = (uint32_t*)malloc(resolution_count * sizeof(uint32_t));

	g_console.supported_resolutions_count = 0;
	for (i = 0; i < num_videomodes; i++)
	{
		if (cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, videomode[i], CELL_VIDEO_OUT_ASPECT_AUTO,0))
		{
			g_console.supported_resolutions[g_console.supported_resolutions_count++] = videomode[i];
			g_console.initial_resolution_id = videomode[i];

			if (g_console.current_resolution_id == videomode[i])
			{
				defaultresolution = false;
				g_console.current_resolution_index = g_console.supported_resolutions_count-1;
			}
		}
	}

	/* In case we didn't specify a resolution - make the last resolution
	that was added to the list (the highest resolution) the default resolution*/
	if (g_console.current_resolution_id > num_videomodes || defaultresolution)
		g_console.current_resolution_index = g_console.supported_resolutions_count-1;
}

void ps3_set_resolution (void)
{
	gl_t *gl = g_gl;
	cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &gl->g_video_state);
}

void ps3_next_resolution (void)
{
	if(g_console.current_resolution_index+1 < g_console.supported_resolutions_count)
	{
		g_console.current_resolution_index++;
		g_console.current_resolution_id = g_console.supported_resolutions[g_console.current_resolution_index];
	}
}

void ps3_previous_resolution (void)
{
	if(g_console.current_resolution_index)
	{
		g_console.current_resolution_index--;
		g_console.current_resolution_id = g_console.supported_resolutions[g_console.current_resolution_index];
	}
}

int ps3_check_resolution(uint32_t resolution_id)
{
	return cellVideoOutGetResolutionAvailability(CELL_VIDEO_OUT_PRIMARY, resolution_id, \
	CELL_VIDEO_OUT_ASPECT_AUTO,0);
}

const char * ps3_get_resolution_label(uint32_t resolution)
{
	switch(resolution)
	{
		case CELL_VIDEO_OUT_RESOLUTION_480:
			return  "720x480 (480p)";
		case CELL_VIDEO_OUT_RESOLUTION_576:
			return "720x576 (576p)"; 
		case CELL_VIDEO_OUT_RESOLUTION_720:
			return "1280x720 (720p)";
		case CELL_VIDEO_OUT_RESOLUTION_960x1080:
			return "960x1080";
		case CELL_VIDEO_OUT_RESOLUTION_1280x1080:
			return "1280x1080";
		case CELL_VIDEO_OUT_RESOLUTION_1440x1080:
			return "1440x1080";
		case CELL_VIDEO_OUT_RESOLUTION_1600x1080:
			return "1600x1080";
		case CELL_VIDEO_OUT_RESOLUTION_1080:
			return "1920x1080 (1080p)";
		default:
			return "Unknown";
	}
}


void ps3graphics_set_vsync(uint32_t vsync)
{
	if(vsync)
		glEnable(GL_VSYNC_SCE);
	else
		glDisable(GL_VSYNC_SCE);
}

bool ps3_setup_texture(void)
{
	gl_t *gl = g_gl;

	if (!gl)
		return false;

	glGenTextures(1, &gl->menu_texture_id);

	SSNES_LOG("Loading texture image for menu...\n");
	if(!texture_image_load(DEFAULT_MENU_BORDER_FILE, &gl->menu_texture))
	{
		SSNES_ERR("Failed to load texture image for menu.\n");
		return false;
	}

	glBindTexture(GL_TEXTURE_2D, gl->menu_texture_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_ARGB_SCE, gl->menu_texture.width, gl->menu_texture.height, 0,
			GL_ARGB_SCE, GL_UNSIGNED_INT_8_8_8_8, gl->menu_texture.pixels);

	glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

	free(gl->menu_texture.pixels);
	
	return true;
}

void ps3_set_filtering(unsigned index, bool set_smooth)
{
	gl_t *gl = g_gl;

	if (!gl)
		return;

	if (index == 1)
	{
		// Apply to all PREV textures.
		for (unsigned i = 0; i < TEXTURES; i++)
		{
			glBindTexture(GL_TEXTURE_2D, gl->texture[i]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
		}
	}
	else if (index >= 2 && gl->fbo_inited)
	{
		glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[index - 2]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, set_smooth ? GL_LINEAR : GL_NEAREST);
	}

	glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}

void ps3graphics_set_overscan(bool overscan_enable, float amount, bool recalculate_viewport)
{
	gl_t * gl = g_gl;
	if(!gl)
		return;

	gl->overscan_enable = overscan_enable;
	gl->overscan_amount = amount;

	if(recalculate_viewport)
		set_viewport(gl, gl->win_width, gl->win_height, false);
}


/* PS3 needs a working graphics stack before SSNES even starts.

   To deal with this main.c, the top level module owns the instance, 
   and is created beforehand. When SSNES gets around to init it, it 
   is already allocated.
   
   When SSNES wants to free it, it is ignored. */

void ps3graphics_video_init(bool get_all_resolutions)
{
	video_info_t video_info = {0};
	// Might have to supply correct values here.
	video_info.vsync = g_settings.video.vsync;
	video_info.force_aspect = false;
	video_info.smooth = g_settings.video.smooth;
	video_info.input_scale = 2;
	g_gl = gl_init(&video_info, NULL, NULL);

	gl_t * gl = g_gl;

	gl->overscan_enable = g_console.overscan_enable;
	gl->overscan_amount = g_console.overscan_amount;

	if(get_all_resolutions)
		get_all_available_resolutions();
	ps3_set_resolution();
	ps3_setup_texture();
	ps3graphics_set_overscan(gl->overscan_enable, gl->overscan_amount, 0);
}

void ps3graphics_video_reinit(void)
{
	gl_t * gl = g_gl;

	if(!gl)
		return;

	ps3_video_deinit();
	gl_cg_invalidate_context();
	ps3graphics_video_init(false);
}

void ps3_video_deinit(void)
{
	void *data = g_gl;
	g_gl = NULL;
	gl_free(data);
}

