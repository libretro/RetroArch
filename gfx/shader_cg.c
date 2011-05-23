/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
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

#include "shader_cg.h"
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include "general.h"
#include <string.h>
#include "strl.h"
#include "conf/config_file.h"
#include "image.h"

// Used when we call deactivate() since just unbinding the program didn't seem to work... :(
static const char* stock_cg_program =
      "void main_vertex"
      "("
      "	float4 position	: POSITION,"
      "	float4 color	: COLOR,"
      "	float2 texCoord : TEXCOORD0,"
      ""
      "  uniform float4x4 modelViewProj,"
      ""
      "	out float4 oPosition : POSITION,"
      "	out float4 oColor    : COLOR,"
      "	out float2 otexCoord : TEXCOORD0"
      ")"
      "{"
      "	oPosition = mul(modelViewProj, position);"
      "	oColor = color;"
      "	otexCoord = texCoord;"
      "}"
      ""
      "float4 main_fragment(float2 tex : TEXCOORD0, uniform sampler2D s0 : TEXUNIT0) : COLOR"
      "{"
      "   return tex2D(s0, tex);"
      "}";

#ifdef SSNES_CG_DEBUG
static void cg_error_handler(CGcontext ctx, CGerror error, void *data)
{
   (void)ctx;
   (void)data;

   SSNES_ERR("CG error!: \"%s\".\n", cgGetErrorString(error));
}
#endif

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

#define MAX_SHADERS 16
#define MAX_TEXTURES 8

struct cg_program
{
   CGprogram vprg;
   CGprogram fprg;
   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter out_size_f;
   CGparameter frame_cnt_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter out_size_v;
   CGparameter frame_cnt_v;
   CGparameter mvp;

   struct cg_fbo_params fbo[MAX_SHADERS];
   struct cg_fbo_params orig;
};

#define FILTER_UNSPEC 0
#define FILTER_LINEAR 1
#define FILTER_NEAREST 2

static struct cg_program prg[MAX_SHADERS];
static bool cg_active = false;
static CGprofile cgVProf, cgFProf;
static unsigned active_index = 0;
static unsigned cg_shader_num = 0;
static struct gl_fbo_scale cg_scale[MAX_SHADERS];
static unsigned fbo_smooth[MAX_SHADERS];

static unsigned lut_textures[MAX_TEXTURES];
static unsigned lut_textures_num = 0;
static char lut_textures_uniform[MAX_TEXTURES][64];

void gl_cg_set_proj_matrix(void)
{
   if (cg_active && prg[active_index].mvp)
   {
      cgGLSetStateMatrixParameter(prg[active_index].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);
   }
}

#define set_param_2f(param, x, y) \
   if (param) cgGLSetParameter2f(param, x, y)
#define set_param_1f(param, x) \
   if (param) cgGLSetParameter1f(param, x)

void gl_cg_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info,
      const struct gl_tex_info *fbo_info,
      unsigned fbo_info_cnt)
{
   if (cg_active)
   {
      // Set frame.
      set_param_2f(prg[active_index].vid_size_f, width, height);
      set_param_2f(prg[active_index].tex_size_f, tex_width, tex_height);
      set_param_2f(prg[active_index].out_size_f, out_width, out_height);
      set_param_1f(prg[active_index].frame_cnt_f, (float)frame_count);

      set_param_2f(prg[active_index].vid_size_v, width, height);
      set_param_2f(prg[active_index].tex_size_v, tex_width, tex_height);
      set_param_2f(prg[active_index].out_size_v, out_width, out_height);
      set_param_1f(prg[active_index].frame_cnt_v, (float)frame_count);

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

      // Set orig texture.
      if (active_index > 1)
      {
         if (prg[active_index].orig.tex)
         {
            cgGLSetTextureParameter(prg[active_index].orig.tex, info->tex);
            cgGLEnableTextureParameter(prg[active_index].orig.tex);
         }

         set_param_2f(prg[active_index].orig.vid_size_v, info->input_size[0], info->input_size[1]);
         set_param_2f(prg[active_index].orig.vid_size_f, info->input_size[0], info->input_size[1]);
         set_param_2f(prg[active_index].orig.tex_size_v, info->tex_size[0], info->tex_size[1]);
         set_param_2f(prg[active_index].orig.tex_size_f, info->tex_size[0], info->tex_size[1]);

         if (prg[active_index].orig.coord)
         {
            cgGLSetParameterPointer(prg[active_index].orig.coord, 2, GL_FLOAT, 0, info->coord);
            cgGLEnableClientState(prg[active_index].orig.coord);
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

            set_param_2f(prg[active_index].fbo[i].vid_size_v, 
                  fbo_info[i].input_size[0], fbo_info[i].input_size[1]);
            set_param_2f(prg[active_index].fbo[i].vid_size_f, 
                  fbo_info[i].input_size[0], fbo_info[i].input_size[1]);

            set_param_2f(prg[active_index].fbo[i].tex_size_v, 
                  fbo_info[i].tex_size[0], fbo_info[i].tex_size[1]);
            set_param_2f(prg[active_index].fbo[i].tex_size_f, 
                  fbo_info[i].tex_size[0], fbo_info[i].tex_size[1]);

            if (prg[active_index].fbo[i].coord)
            {
               cgGLSetParameterPointer(prg[active_index].fbo[i].coord, 2, GL_FLOAT, 0, fbo_info[i].coord);
               cgGLEnableClientState(prg[active_index].fbo[i].coord);
            }
         }
      }
   }
}

void gl_cg_deinit(void)
{
   if (cg_active)
      cgDestroyContext(cgCtx);
   cg_active = false;
   cg_shader_num = 0;
   memset(prg, 0, sizeof(prg));
   memset(cg_scale, 0, sizeof(cg_scale));
   memset(fbo_smooth, 0, sizeof(fbo_smooth));

   glDeleteTextures(lut_textures_num, lut_textures);
   lut_textures_num = 0;
}

static bool load_plain(const char *path)
{
   SSNES_LOG("Loading Cg file: %s\n", path);
   if (strlen(g_settings.video.second_pass_shader) > 0)
      SSNES_LOG("Loading 2nd pass: %s\n", g_settings.video.second_pass_shader);

   prg[0].fprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_program, cgFProf, "main_fragment", 0);
   prg[0].vprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_program, cgVProf, "main_vertex", 0);

   prg[1].fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, path, cgFProf, "main_fragment", 0);
   prg[1].vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, path, cgVProf, "main_vertex", 0);

   if (strlen(g_settings.video.second_pass_shader) > 0)
   {
      prg[2].fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, g_settings.video.second_pass_shader, cgFProf, "main_fragment", 0);
      prg[2].vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, g_settings.video.second_pass_shader, cgVProf, "main_vertex", 0);
      cg_shader_num = 2;
   }
   else
   {
      prg[2] = prg[0];
      cg_shader_num = 1;
   }

   for (int i = 0; i < cg_shader_num + 1; i++)
   {
      if (!prg[i].fprg || !prg[i].vprg)
      {
         CGerror err = cgGetError();
         SSNES_ERR("CG error: %s\n", cgGetErrorString(err));
         return false;
      }

      cgGLLoadProgram(prg[i].fprg);
      cgGLLoadProgram(prg[i].vprg);
   }

   return true;
}

static bool load_textures(const char *dir_path, config_file_t *conf)
{
   char *textures;
   if (!config_get_string(conf, "textures", &textures)) // No textures here ...
      return true;

   const char *id = strtok(textures, ";");;
   while (id && lut_textures_num < MAX_TEXTURES)
   {
      char *path;
      if (!config_get_string(conf, id, &path))
      {
         SSNES_ERR("Cannot find path to texture \"%s\" ...\n", id);
         goto error;
      }

      char id_filter[64];
      snprintf(id_filter, sizeof(id_filter), "%s_linear", id);
      bool smooth;
      if (!config_get_bool(conf, id_filter, &smooth))
         smooth = true;

      char image_path[512];
      snprintf(image_path, sizeof(image_path), "%s%s", dir_path, path);

      SSNES_LOG("Loading image from: \"%s\".\n", image_path);
      struct texture_image img;
      if (!texture_image_load(image_path, &img))
      {
         SSNES_ERR("Failed to load picture ...\n");
         free(path);
         goto error;
      }

      strlcpy(lut_textures_uniform[lut_textures_num], id, sizeof(lut_textures_uniform[lut_textures_num]));

      glGenTextures(1, &lut_textures[lut_textures_num]);
      glBindTexture(GL_TEXTURE_2D, lut_textures[lut_textures_num]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);

      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, img.width);
      glTexImage2D(GL_TEXTURE_2D,
            0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, img.pixels);

      lut_textures_num++;

      free(img.pixels);
      free(path);

      id = strtok(NULL, ";");;
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   free(textures);
   return true;

error:
   if (textures)
      free(textures);

   pglActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, 0);
   return false;
}

static bool load_preset(const char *path)
{
   // Create passthrough shader.
   prg[0].fprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_program, cgFProf, "main_fragment", 0);
   prg[0].vprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_program, cgVProf, "main_vertex", 0);
   if (!prg[0].fprg || !prg[0].vprg)
   {
      SSNES_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }
   cgGLLoadProgram(prg[0].fprg);
   cgGLLoadProgram(prg[0].vprg);

   SSNES_LOG("Loading Cg meta-shader: %s\n", path);
   config_file_t *conf = config_file_new(path);
   if (!conf)
   {
      SSNES_ERR("Failed to load preset.\n");
      goto error;
   }

   int shaders;
   if (!config_get_int(conf, "shaders", &shaders))
   {
      SSNES_ERR("Cannot find \"shaders\" param.\n");
      goto error;
   }

   if (shaders < 1)
   {
      SSNES_ERR("Need to define at least 1 shader!\n");
      goto error;
   }

   cg_shader_num = shaders;
   if (shaders > MAX_SHADERS - 1)
   {
      SSNES_WARN("Too many shaders ... Capping shader amount to %d.\n", MAX_SHADERS - 1);
      cg_shader_num = shaders = MAX_SHADERS - 1;
   }
   prg[shaders] = prg[0];

   // Check filter params.
   for (unsigned i = 0; i < shaders; i++)
   {
      bool smooth;
      char filter_name_buf[64];
      snprintf(filter_name_buf, sizeof(filter_name_buf), "filter_linear%u", i);
      if (config_get_bool(conf, filter_name_buf, &smooth))
         fbo_smooth[i + 1] = smooth ? FILTER_LINEAR : FILTER_NEAREST;
   }

   // Bigass for-loop ftw. Check scaling params.
   for (unsigned i = 0; i < shaders; i++)
   {
      char *scale_type;
      char scale_name_buf[64];
      snprintf(scale_name_buf, sizeof(scale_name_buf), "scale_type%u", i);
      if (config_get_string(conf, scale_name_buf, &scale_type))
      {
         char attr_name_buf[64];
         double fattr;
         int iattr;
         struct gl_fbo_scale *scale = &cg_scale[i + 1]; // Shader 0 is passthrough shader. Start at 1.

         scale->valid = true;
         scale->type_x = SSNES_SCALE_INPUT;
         scale->type_y = SSNES_SCALE_INPUT;
         scale->scale_x = SSNES_SCALE_INPUT;
         scale->scale_y = SSNES_SCALE_INPUT;

         // Check source scale.
         if (strcmp(scale_type, "source") == 0)
         {
            snprintf(attr_name_buf, sizeof(attr_name_buf), "scale%u", i);
            if (config_get_double(conf, attr_name_buf, &fattr))
            {
               scale->scale_x = fattr;
               scale->scale_y = fattr;
            }
            else
            {
               snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_x%u", i);
               if (config_get_double(conf, attr_name_buf, &fattr))
                  scale->scale_x = fattr;

               snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_y%u", i);
               if (config_get_double(conf, attr_name_buf, &fattr))
                  scale->scale_y = fattr;
            }
         }
         // Viewport scale.
         else if (strcmp(scale_type, "viewport") == 0)
         {
            snprintf(attr_name_buf, sizeof(attr_name_buf), "scale%u", i);
            if (config_get_double(conf, attr_name_buf, &fattr))
            {
               scale->scale_x = fattr;
               scale->scale_y = fattr;
               scale->type_x = SSNES_SCALE_VIEWPORT;
               scale->type_y = SSNES_SCALE_VIEWPORT;
            }
            else
            {
               snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_x%u", i);
               if (config_get_double(conf, attr_name_buf, &fattr))
               {
                  scale->scale_x = fattr;
                  scale->type_x = SSNES_SCALE_VIEWPORT;
               }

               snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_y%u", i);
               if (config_get_double(conf, attr_name_buf, &fattr))
               {
                  scale->scale_y = fattr;
                  scale->type_y = SSNES_SCALE_VIEWPORT;
               }
            }
         }
         // Absolute pixel scale.
         else if (strcmp(scale_type, "absolute") == 0)
         {
            snprintf(attr_name_buf, sizeof(attr_name_buf), "scale%u", i);
            if (config_get_int(conf, attr_name_buf, &iattr))
            {
               scale->abs_x = iattr;
               scale->abs_y = iattr;
               scale->type_x = SSNES_SCALE_ABSOLUTE;
               scale->type_y = SSNES_SCALE_ABSOLUTE;
            }
            else
            {
               snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_x%u", i);
               if (config_get_int(conf, attr_name_buf, &iattr))
               {
                  scale->abs_x = iattr;
                  scale->type_x = SSNES_SCALE_ABSOLUTE;
               }

               snprintf(attr_name_buf, sizeof(attr_name_buf), "scale_y%u", i);
               if (config_get_int(conf, attr_name_buf, &iattr))
               {
                  scale->abs_y = iattr;
                  scale->type_y = SSNES_SCALE_ABSOLUTE;
               }
            }
         }
         else
         {
            SSNES_ERR("Invalid attribute: \"%s\"\n", scale_type);
            free(scale_type);
            goto error;
         }

         free(scale_type);
      }
   }

   // Basedir.
   char dir_path[256];
   strlcpy(dir_path, path, sizeof(dir_path));
   char *ptr = strrchr(dir_path, '/');
   if (!ptr) ptr = strrchr(dir_path, '\\');
   if (ptr) 
      ptr[1] = '\0';
   else // No directory.
      dir_path[0] = '\0';

   // Finally load shaders :)
   for (unsigned i = 0; i < shaders; i++)
   {
      char *shader_path;
      char attr_buf[64];
      char path_buf[512];

      snprintf(attr_buf, sizeof(attr_buf), "shader%u", i);
      if (config_get_string(conf, attr_buf, &shader_path))
      {
         strlcpy(path_buf, dir_path, sizeof(path_buf));
         strlcat(path_buf, shader_path, sizeof(path_buf));
         free(shader_path);
      }
      else
      {
         SSNES_ERR("Didn't find shader path in config ...\n");
         goto error;
      }

      SSNES_LOG("Loading Cg shader: \"%s\".\n", path_buf);

      struct cg_program *prog = &prg[i + 1];
      prog->fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, path_buf, cgFProf, "main_fragment", 0);
      prog->vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, path_buf, cgVProf, "main_vertex", 0);

      if (!prog->fprg || !prog->vprg)
      {
         CGerror err = cgGetError();
         SSNES_ERR("CG error: %s\n", cgGetErrorString(err));
         goto error;
      }

      cgGLLoadProgram(prog->fprg);
      cgGLLoadProgram(prog->vprg);
   }

   if (!load_textures(dir_path, conf))
   {
      SSNES_ERR("Failed to load lookup textures ...\n");
      goto error;
   }

   config_file_free(conf);
   return true;

error:
   if (conf)
      config_file_free(conf);
   return false;
}

bool gl_cg_init(const char *path)
{
   cgCtx = cgCreateContext();
   if (cgCtx == NULL)
   {
      SSNES_ERR("Failed to create Cg context\n");
      return false;
   }

#ifdef SSNES_CG_DEBUG
   cgGLSetDebugMode(CG_TRUE);
   cgSetErrorHandler(cg_error_handler, NULL);
#endif

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

   prg[0].mvp = cgGetNamedParameter(prg[0].vprg, "modelViewProj");
   if (prg[0].mvp)
      cgGLSetStateMatrixParameter(prg[0].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);

   for (unsigned i = 1; i < cg_shader_num + 1; i++)
   {
      cgGLBindProgram(prg[i].fprg);
      cgGLBindProgram(prg[i].vprg);

      prg[i].vid_size_f = cgGetNamedParameter(prg[i].fprg, "IN.video_size");
      prg[i].tex_size_f = cgGetNamedParameter(prg[i].fprg, "IN.texture_size");
      prg[i].out_size_f = cgGetNamedParameter(prg[i].fprg, "IN.output_size");
      prg[i].frame_cnt_f = cgGetNamedParameter(prg[i].fprg, "IN.frame_count");
      prg[i].vid_size_v = cgGetNamedParameter(prg[i].vprg, "IN.video_size");
      prg[i].tex_size_v = cgGetNamedParameter(prg[i].vprg, "IN.texture_size");
      prg[i].out_size_v = cgGetNamedParameter(prg[i].vprg, "IN.output_size");
      prg[i].frame_cnt_v = cgGetNamedParameter(prg[i].vprg, "IN.frame_count");
      prg[i].mvp = cgGetNamedParameter(prg[i].vprg, "modelViewProj");
      if (prg[i].mvp)
         cgGLSetStateMatrixParameter(prg[i].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);

      prg[i].orig.tex = cgGetNamedParameter(prg[i].fprg, "ORIG.texture");
      prg[i].orig.vid_size_v = cgGetNamedParameter(prg[i].vprg, "ORIG.video_size");
      prg[i].orig.vid_size_f = cgGetNamedParameter(prg[i].fprg, "ORIG.video_size");
      prg[i].orig.tex_size_v = cgGetNamedParameter(prg[i].vprg, "ORIG.texture_size");
      prg[i].orig.tex_size_f = cgGetNamedParameter(prg[i].fprg, "ORIG.texture_size");
      prg[i].orig.coord = cgGetNamedParameter(prg[i].vprg, "ORIG.tex_coord");

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

   cgGLBindProgram(prg[1].fprg);
   cgGLBindProgram(prg[1].vprg);

   cg_active = true;
   return true;
}

void gl_cg_use(unsigned index)
{
   if (cg_active && prg[index].vprg && prg[index].fprg)
   {
      active_index = index;
      cgGLBindProgram(prg[index].vprg);
      cgGLBindProgram(prg[index].fprg);
   }
}

unsigned gl_cg_num(void)
{
   if (cg_active)
      return cg_shader_num;
   else
      return 0;
}

bool gl_cg_filter_type(unsigned index, bool *smooth)
{
   if (cg_active)
   {
      if (fbo_smooth[index] == FILTER_UNSPEC)
         return false;
      *smooth = (fbo_smooth[index] == FILTER_LINEAR);
      return true;
   }
   else
      return false;
}

void gl_cg_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
   if (cg_active)
      *scale = cg_scale[index];
   else
      scale->valid = false;
}
