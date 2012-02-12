/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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
#include "../general.h"
#include <string.h>
#include "../strl.h"
#include "../conf/config_file.h"
#include "image.h"
#include "../dynamic.h"
#include "../posix_string.h"

#ifdef HAVE_CONFIGFILE
#include "snes_state.h"
#endif

//#define SSNES_CG_DEBUG

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

#ifdef SSNES_CG_DEBUG
static void cg_error_handler(CGcontext ctx, CGerror error, void *data)
{
   (void)ctx;
   (void)data;

   switch (error)
   {
      case CG_INVALID_PARAM_HANDLE_ERROR:
         SSNES_ERR("Invalid param handle.\n");
         break;

      case CG_INVALID_PARAMETER_ERROR:
         SSNES_ERR("Invalid parameter.\n");
         break;

      default:
         break;
   }

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
static bool cg_active = false;
static CGprofile cgVProf, cgFProf;
static unsigned active_index = 0;
static unsigned cg_shader_num = 0;
static struct gl_fbo_scale cg_scale[SSNES_CG_MAX_SHADERS];
static unsigned fbo_smooth[SSNES_CG_MAX_SHADERS];

static unsigned lut_textures[MAX_TEXTURES];
static unsigned lut_textures_num = 0;
static char lut_textures_uniform[MAX_TEXTURES][64];

static CGparameter cg_attribs[PREV_TEXTURES + 1 + SSNES_CG_MAX_SHADERS];
static unsigned cg_attrib_index;

#ifdef HAVE_CONFIGFILE
static snes_tracker_t *snes_tracker = NULL;
#endif

static void gl_cg_reset_attrib(void)
{
   for (unsigned i = 0; i < cg_attrib_index; i++)
      cgGLDisableClientState(cg_attribs[i]);
   cg_attrib_index = 0;
}

void gl_cg_set_proj_matrix(void)
{
   if (cg_active && prg[active_index].mvp)
      cgGLSetStateMatrixParameter(prg[active_index].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);
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
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info,
      unsigned fbo_info_cnt)
{
   if (!cg_active || (active_index == 0))
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

#ifdef HAVE_CONFIGFILE
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
#endif
}

void gl_cg_deinit(void)
{
   if (!cg_active)
      return;

   gl_cg_reset_attrib();

   cg_active = false;
   cg_shader_num = 0;
   memset(prg, 0, sizeof(prg));
   memset(cg_scale, 0, sizeof(cg_scale));
   memset(fbo_smooth, 0, sizeof(fbo_smooth));

   glDeleteTextures(lut_textures_num, lut_textures);
   lut_textures_num = 0;

#ifdef HAVE_CONFIGFILE
   if (snes_tracker)
   {
      snes_tracker_free(snes_tracker);
      snes_tracker = NULL;
   }
#endif

   if (menu_cg_program)
   {
      free(menu_cg_program);
      menu_cg_program = NULL;
   }

#ifndef __CELLOS_LV2__
   cgDestroyContext(cgCtx);
   cgCtx = NULL;
#endif
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

#define print_buf(buf, ...) snprintf(buf, sizeof(buf), __VA_ARGS__)

#ifdef HAVE_CONFIGFILE
static void load_texture_data(GLuint *obj, const struct texture_image *img, bool smooth)
{
   glGenTextures(1, obj);
   glBindTexture(GL_TEXTURE_2D, *obj);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);

#ifdef __CELLOS_LV2__
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_ARGB_SCE, img->width, img->height,
         0, GL_ARGB_SCE, GL_UNSIGNED_INT_8_8_8_8, img->pixels);
#else
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
   glPixelStorei(GL_UNPACK_ROW_LENGTH, img->width);
   glTexImage2D(GL_TEXTURE_2D,
         0, GL_RGBA, img->width, img->height,
         0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, img->pixels);
#endif

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
      char *path;
      if (!config_get_string(conf, id, &path))
      {
         SSNES_ERR("Cannot find path to texture \"%s\" ...\n", id);
         ret = false;
         goto end;
      }

      char id_filter[64];
      print_buf(id_filter, "%s_linear", id);
      bool smooth;
      if (!config_get_bool(conf, id_filter, &smooth))
         smooth = true;

      char image_path[512];
      print_buf(image_path, "%s%s", dir_path, path);

      SSNES_LOG("Loading image from: \"%s\".\n", image_path);
      struct texture_image img;
      if (!texture_image_load(image_path, &img))
      {
         SSNES_ERR("Failed to load picture ...\n");
         free(path);
         ret = false;
         goto end;
      }

      strlcpy(lut_textures_uniform[lut_textures_num], id, sizeof(lut_textures_uniform[lut_textures_num]));

      load_texture_data(&lut_textures[lut_textures_num], &img, smooth);
      lut_textures_num++;

      free(path);

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

#ifdef HAVE_PYTHON
   char script_path[128];
   char *script = NULL;
   char *script_class = NULL; 
#endif

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
#ifdef HAVE_PYTHON
      else if (strcmp(semantic, "python") == 0)
         tracker_type = SSNES_STATE_PYTHON;
#endif
      else
      {
         SSNES_ERR("Invalid semantic.\n");
         ret = false;
         goto end;
      }

      unsigned addr = 0;
#ifdef HAVE_PYTHON
      if (tracker_type != SSNES_STATE_PYTHON)
#endif
      {
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

      unsigned bitmask;
      if (!config_get_hex(conf, mask_buf, &bitmask))
         bitmask = 0;
      unsigned bitequal;
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

#ifdef HAVE_PYTHON
   if (config_get_string(conf, "import_script", &script))
   {
      strlcpy(script_path, dir_path, sizeof(script_path));
      strlcat(script_path, script, sizeof(script_path));

      tracker_info.script = script_path;
   }
   if (config_get_string(conf, "import_script_class", &script_class))
      tracker_info.script_class = script_class;

   tracker_info.script_is_file = true;
#endif

   snes_tracker = snes_tracker_init(&tracker_info);
   if (!snes_tracker)
      SSNES_WARN("Failed to init SNES tracker.\n");

#ifdef HAVE_PYTHON
   if (script)
      free(script);
   if (script_class)
      free(script_class);
#endif

end:
   free(imports);
   return ret;
}
#endif

static bool load_shader(const char *dir_path, unsigned i, config_file_t *conf)
{
   char *shader_path;
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
   double fattr;
   int iattr;
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
      if (config_get_double(conf, attr_name_buf, &fattr))
         scale->scale_x = fattr;
      else
      {
         print_buf(attr_name_buf, "scale_x%u", i);
         if (config_get_double(conf, attr_name_buf, &fattr))
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
      if (config_get_double(conf, attr_name_buf, &fattr))
         scale->scale_y = fattr;
      else
      {
         print_buf(attr_name_buf, "scale_y%u", i);
         if (config_get_double(conf, attr_name_buf, &fattr))
            scale->scale_y = fattr;
      }
   }

end:
   free(scale_type);
   free(scale_type_x);
   free(scale_type_y);
   return ret;
}

static bool load_preset(const char *path)
{
#ifdef HAVE_CONFIGFILE
   bool ret = true;

   if (!load_stock())
      return false;

   int shaders;
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
      bool smooth;
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

#else
   (void)path;
   SSNES_ERR("No config file support compiled in.\n");
   return false;
#endif
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

bool gl_cg_init(const char *path)
{
#ifdef __CELLOS_LV2__
   cgRTCgcInit();
#endif

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

   cg_active = true;
   return true;
}

void gl_cg_use(unsigned index)
{
   if (cg_active && prg[index].vprg && prg[index].fprg)
   {
      gl_cg_reset_attrib();

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

void gl_cg_set_menu_shader(const char *path)
{
   if (menu_cg_program)
      free(menu_cg_program);

   menu_cg_program = strdup(path);
}

void gl_cg_set_compiler_args(const char **argv)
{
   cg_arguments = argv;
}

bool gl_cg_load_shader(unsigned index, const char *path)
{
   if (!cg_active)
      return false;

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

   fprintf(file, "shader0 = %s\n", info->shader[0]);
   if (shaders == 2)
      fprintf(file, "shader1 = %s\n", info->shader[1]);

   fprintf(file, "filter_linear0 = %s\n", info->filter_linear[0] ? "true" : "false");

   if (info->render_to_texture)
   {
      fprintf(file, "filter_linear1 = %s\n", info->filter_linear[1] ? "true" : "false");
      fprintf(file, "scale_type0 = source\n");
      fprintf(file, "scale0 = %.1f\n", info->fbo_scale);
   }

   fclose(file);
   return true;
}

