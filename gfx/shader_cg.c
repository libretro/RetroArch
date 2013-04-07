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

#ifdef _MSC_VER
#pragma comment(lib, "cg")
#pragma comment(lib, "cggl")
#endif

#include "shader_cg.h"
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include "../general.h"
#include <string.h>
#include "../compat/strl.h"
#include "../conf/config_file.h"
#include "image.h"
#include "../dynamic.h"
#include "../compat/posix_string.h"
#include "../file.h"

#include "state_tracker.h"

//#define RARCH_CG_DEBUG

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

static const char *menu_cg_program =
      "struct input"
      "{"
         "float2 video_size;"
         "float2 texture_size;"
         "float2 output_size;"
         "float frame_count;"
         "float frame_direction;"
         "float frame_rotation;"
      "};"
      "void main_vertex"
      "("
         "float4 position : POSITION,"
         "out float4 oPosition : POSITION,"
         "uniform float4x4 modelViewProj,"
         "float4 color : COLOR,"
         "out float4 oColor : COLOR,"
         "float2 tex_border : TEXCOORD1,"
         "out float2 otex_border : TEXCOORD1,"
         "uniform input IN"
      ")"
      "{"
         "oPosition = mul(modelViewProj, position);"
         "oColor = color;"
         "otex_border = tex_border;"
      "}"
      "float4 main_fragment (float2 tex_border : TEXCOORD1, uniform sampler2D bg : TEXUNIT0, uniform input IN) : COLOR"
      "{"
         "float4 background = tex2D(bg, tex_border);"
         "return background;"
      "}";

#ifdef RARCH_CG_DEBUG
static void cg_error_handler(CGcontext ctx, CGerror error, void *data)
{
   (void)ctx;
   (void)data;

   switch (error)
   {
      case CG_INVALID_PARAM_HANDLE_ERROR:
         RARCH_ERR("CG: Invalid param handle.\n");
         break;

      case CG_INVALID_PARAMETER_ERROR:
         RARCH_ERR("CG: Invalid parameter.\n");
         break;

      default:
         break;
   }

   RARCH_ERR("CG error: \"%s\"\n", cgGetErrorString(error));
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

   CGparameter tex;
   CGparameter lut_tex;
   CGparameter color;
   CGparameter vertex;

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

   struct cg_fbo_params fbo[RARCH_CG_MAX_SHADERS];
   struct cg_fbo_params orig;
   struct cg_fbo_params prev[PREV_TEXTURES];
};

static struct cg_program prg[RARCH_CG_MAX_SHADERS];
static const char **cg_arguments;
static bool cg_active;
static CGprofile cgVProf, cgFProf;
static unsigned active_index;

static struct gfx_shader *cg_shader;

static state_tracker_t *state_tracker;
static GLuint lut_textures[MAX_TEXTURES];

static CGparameter cg_attribs[PREV_TEXTURES + 1 + 4 + RARCH_CG_MAX_SHADERS];
static unsigned cg_attrib_index;

static void gl_cg_reset_attrib(void)
{
   for (unsigned i = 0; i < cg_attrib_index; i++)
      cgGLDisableClientState(cg_attribs[i]);
   cg_attrib_index = 0;
}

bool gl_cg_set_mvp(const math_matrix *mat)
{
   if (cg_active && prg[active_index].mvp)
   {
      cgGLSetMatrixParameterfc(prg[active_index].mvp, mat->data);
      return true;
   }
   else
      return false;
}

#define SET_COORD(name, coords_name, len) do { \
   if (prg[active_index].name) \
   { \
      cgGLSetParameterPointer(prg[active_index].name, len, GL_FLOAT, 0, coords->coords_name); \
      cgGLEnableClientState(prg[active_index].name); \
      cg_attribs[cg_attrib_index++] = prg[active_index].name; \
   } \
} while(0)

bool gl_cg_set_coords(const struct gl_coords *coords)
{
   if (!cg_active)
      return false;

   SET_COORD(vertex, vertex, 2);
   SET_COORD(tex, tex_coord, 2);
   SET_COORD(lut_tex, lut_tex_coord, 2);
   SET_COORD(color, color, 4);

   return true;
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
   set_param_1f(prg[active_index].frame_dir_f, g_extern.frame_is_reverse ? -1.0 : 1.0);

   set_param_2f(prg[active_index].vid_size_v, width, height);
   set_param_2f(prg[active_index].tex_size_v, tex_width, tex_height);
   set_param_2f(prg[active_index].out_size_v, out_width, out_height);
   set_param_1f(prg[active_index].frame_dir_v, g_extern.frame_is_reverse ? -1.0 : 1.0);

   if (prg[active_index].frame_cnt_f || prg[active_index].frame_cnt_v)
   {
      unsigned modulo = cg_shader->pass[active_index - 1].frame_count_mod;
      if (modulo)
         frame_count %= modulo;

      set_param_1f(prg[active_index].frame_cnt_f, (float)frame_count);
      set_param_1f(prg[active_index].frame_cnt_v, (float)frame_count);
   }

   if (active_index == RARCH_CG_MENU_SHADER_INDEX)
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
   for (unsigned i = 0; i < cg_shader->luts; i++)
   {
      CGparameter param = cgGetNamedParameter(prg[active_index].fprg, cg_shader->lut[i].id);
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
   if (state_tracker)
   {
      // Only query uniforms in first pass.
      static struct state_tracker_uniform info[MAX_VARIABLES];
      static unsigned cnt = 0;

      if (active_index == 1)
         cnt = state_get_uniform(state_tracker, info, MAX_VARIABLES, frame_count);

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
   RARCH_LOG("CG: Destroying programs.\n");
   cgGLUnbindProgram(cgFProf);
   cgGLUnbindProgram(cgVProf);

   // Programs may alias [0].
   for (unsigned i = 1; i < RARCH_CG_MAX_SHADERS; i++)
   {
      if (prg[i].fprg && prg[i].fprg != prg[0].fprg)
         cgDestroyProgram(prg[i].fprg);
      if (prg[i].vprg && prg[i].vprg != prg[0].vprg)
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
   cg_active = false;

   gl_cg_deinit_progs();

   if (cg_shader && cg_shader->luts)
   {
      glDeleteTextures(cg_shader->luts, lut_textures);
      memset(lut_textures, 0, sizeof(lut_textures));
   }

   if (state_tracker)
   {
      state_tracker_free(state_tracker);
      state_tracker = NULL;
   }

   free(cg_shader);
   cg_shader = NULL;
}

// Final deinit.
static void gl_cg_deinit_context_state(void)
{
   // Destroying context breaks on PS3 for some unknown reason.
#ifndef __CELLOS_LV2__
   if (cgCtx)
   {
      RARCH_LOG("CG: Destroying context.\n");
      cgDestroyContext(cgCtx);
      cgCtx = NULL;
   }
#endif
}

// Full deinit.
void gl_cg_deinit(void)
{
   if (!cg_active)
      return;

   gl_cg_deinit_state();
   gl_cg_deinit_context_state();
}

// Deinit as much as possible without resetting context (broken on PS3),
// and reinit cleanly.
// If this fails, we're kinda screwed without resetting everything on PS3.
bool gl_cg_reinit(const char *path)
{
   if (cg_active)
      gl_cg_deinit_state();

   return gl_cg_init(path);
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
      RARCH_ERR("CG error: %s\n", cgGetErrorString(cgGetError()));
      if (listing_f)
         RARCH_ERR("Fragment:\n%s\n", listing_f);
      else if (listing_v)
         RARCH_ERR("Vertex:\n%s\n", listing_v);

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

static void set_program_base_attrib(unsigned i);

static bool load_stock(void)
{
   if (!load_program(0, stock_cg_program, false))
   {
      RARCH_ERR("Failed to compile passthrough shader, is something wrong with your environment?\n");
      return false;
   }

   set_program_base_attrib(0);

   return true;
}

static bool load_plain(const char *path)
{
   if (!load_stock())
      return false;

   cg_shader = (struct gfx_shader*)calloc(1, sizeof(*cg_shader));
   if (!cg_shader)
      return false;

   cg_shader->passes = 1;

   if (path)
   {
      RARCH_LOG("Loading Cg file: %s\n", path);
      strlcpy(cg_shader->pass[0].source.cg, path, sizeof(cg_shader->pass[0].source.cg));
      if (!load_program(1, path, true))
         return false;
   }
   else
   {
      RARCH_LOG("Loading stock Cg file.\n");
      prg[1] = prg[0];
   }

   return true;
}

static bool load_menu_shader(void)
{
   return load_program(RARCH_CG_MENU_SHADER_INDEX, menu_cg_program, false);
}

#define print_buf(buf, ...) snprintf(buf, sizeof(buf), __VA_ARGS__)

#ifdef HAVE_OPENGLES2
#define BORDER_FUNC GL_CLAMP_TO_EDGE
#else
#define BORDER_FUNC GL_CLAMP_TO_BORDER
#endif

static void load_texture_data(GLuint obj, const struct texture_image *img, bool smooth)
{
   glBindTexture(GL_TEXTURE_2D, obj);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, BORDER_FUNC);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, BORDER_FUNC);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, smooth ? GL_LINEAR : GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, smooth ? GL_LINEAR : GL_NEAREST);

#ifndef HAVE_PSGL
   glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#endif
   glTexImage2D(GL_TEXTURE_2D,
         0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_INTERNAL_FORMAT32, img->width, img->height,
         0, driver.gfx_use_rgba ? GL_RGBA : RARCH_GL_TEXTURE_TYPE32, RARCH_GL_FORMAT32, img->pixels);

   free(img->pixels);
}

static bool load_textures(const char *cgp_path)
{
   if (!cg_shader->luts)
      return true;

   glGenTextures(cg_shader->luts, lut_textures);

   for (unsigned i = 0; i < cg_shader->luts; i++)
   {
      char image_path[PATH_MAX];
      fill_pathname_resolve_relative(image_path, cgp_path,
            cg_shader->lut[i].path, sizeof(image_path));

      RARCH_LOG("Loading image from: \"%s\".\n", image_path);

      struct texture_image img;
      if (!texture_image_load(image_path, &img))
      {
         RARCH_ERR("Failed to load picture ...\n");
         return false;
      }

      load_texture_data(lut_textures[i], &img,
            cg_shader->lut[i].filter != RARCH_FILTER_NEAREST);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
   return true;
}

static bool load_imports(const char *cgp_path)
{
   struct state_tracker_info tracker_info = {0};

   for (unsigned i = 0; i < cg_shader->variables; i++)
   {
      unsigned memtype;
      switch (cg_shader->variable[i].ram_type)
      {
         case RARCH_STATE_WRAM:
            memtype = RETRO_MEMORY_SYSTEM_RAM;
            break;

         default:
            memtype = -1u;
      }

      if ((memtype != -1u) && (cg_shader->variable[i].addr >= pretro_get_memory_size(memtype)))
      {
         RARCH_ERR("Address out of bounds.\n");
         return false;
      }
   }

   tracker_info.wram = (uint8_t*)pretro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
   tracker_info.info = cg_shader->variable;
   tracker_info.info_elem = cg_shader->variables;

#ifdef HAVE_PYTHON
   char script_path[PATH_MAX];
   if (*cg_shader->script_path)
   {
      fill_pathname_resolve_relative(script_path, cgp_path,
            cg_shader->script_path, sizeof(script_path));
      tracker_info.script = script_path;
      tracker_info.script_is_file = true;
   }

   tracker_info.script_class = *cg_shader->script_class ? cg_shader->script_class : NULL;
#endif

   state_tracker = state_tracker_init(&tracker_info);
   if (!state_tracker)
      RARCH_WARN("Failed to initialize state tracker.\n");

   return true;
}

static bool load_shader(const char *cgp_path, unsigned i)
{
   char path_buf[PATH_MAX];
   fill_pathname_resolve_relative(path_buf, cgp_path,
         cg_shader->pass[i].source.cg, sizeof(path_buf));

   RARCH_LOG("Loading Cg shader: \"%s\".\n", path_buf);

   if (!load_program(i + 1, path_buf, true))
      return false;

   return true;
}

static bool load_preset(const char *path)
{
   if (!load_stock())
      return false;

   RARCH_LOG("Loading Cg meta-shader: %s\n", path);
   config_file_t *conf = config_file_new(path);
   if (!conf)
   {
      RARCH_ERR("Failed to load preset.\n");
      return false;
   }

   if (!cg_shader)
      cg_shader = (struct gfx_shader*)calloc(1, sizeof(*cg_shader));
   if (!cg_shader)
      return false;

   if (!gfx_shader_read_conf_cgp(conf, cg_shader))
   {
      RARCH_ERR("Failed to parse CGP file.\n");
      config_file_free(conf);
      return false;
   }

   config_file_free(conf);

#if 0 // Debugging
   config_file_t *save_test = config_file_new(NULL);
   gfx_shader_write_conf_cgp(conf, cg_shader);
   config_file_write(save_test, "/tmp/load.cgp");
   config_file_free(save_test);
#endif

   if (cg_shader->passes > RARCH_CG_MAX_SHADERS - 3)
   {
      RARCH_WARN("Too many shaders ... Capping shader amount to %d.\n", RARCH_CG_MAX_SHADERS - 3);
      cg_shader->passes = RARCH_CG_MAX_SHADERS - 3;
   }
   // If we aren't using last pass non-FBO shader, 
   // this shader will be assumed to be "fixed-function".
   // Just use prg[0] for that pass, which will be
   // pass-through.
   prg[cg_shader->passes + 1] = prg[0]; 

   for (unsigned i = 0; i < cg_shader->passes; i++)
   {
      if (!load_shader(path, i))
      {
         RARCH_ERR("Failed to load shaders ...\n");
         return false;
      }
   }

   if (!load_textures(path))
   {
      RARCH_ERR("Failed to load lookup textures ...\n");
      return false;
   }

   if (!load_imports(path))
   {
      RARCH_ERR("Failed to load imports ...\n");
      return false;
   }

   return true;
}

static void set_program_base_attrib(unsigned i)
{
   CGparameter param = cgGetFirstParameter(prg[i].vprg, CG_PROGRAM);
   for (; param; param = cgGetNextParameter(param))
   {
      if (cgGetParameterDirection(param) != CG_IN || cgGetParameterVariability(param) != CG_VARYING)
         continue;

      const char *semantic = cgGetParameterSemantic(param);
      if (!semantic)
         continue;

      RARCH_LOG("CG: Found semantic \"%s\" in prog #%u.\n", semantic, i);

      if (strcmp(semantic, "TEXCOORD") == 0 || strcmp(semantic, "TEXCOORD0") == 0)
         prg[i].tex = param;
      else if (strcmp(semantic, "COLOR") == 0 || strcmp(semantic, "COLOR0") == 0)
         prg[i].color = param;
      else if (strcmp(semantic, "POSITION") == 0)
         prg[i].vertex = param;
      else if (strcmp(semantic, "TEXCOORD1") == 0)
         prg[i].lut_tex = param;
   }
}

static void set_program_attributes(unsigned i)
{
   cgGLBindProgram(prg[i].fprg);
   cgGLBindProgram(prg[i].vprg);

   set_program_base_attrib(i);

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

   if (i == RARCH_CG_MENU_SHADER_INDEX)
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
#ifdef HAVE_CG_RUNTIME_COMPILER
   cgRTCgcInit();
#endif

   if (!cgCtx)
      cgCtx = cgCreateContext();

   if (cgCtx == NULL)
   {
      RARCH_ERR("Failed to create Cg context\n");
      return false;
   }

#ifdef RARCH_CG_DEBUG
   cgGLSetDebugMode(CG_TRUE);
   cgSetErrorHandler(cg_error_handler, NULL);
#endif

   cgFProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
   cgVProf = cgGLGetLatestProfile(CG_GL_VERTEX);
   if (cgFProf == CG_PROFILE_UNKNOWN || cgVProf == CG_PROFILE_UNKNOWN)
   {
      RARCH_ERR("Invalid profile type\n");
      return false;
   }
#ifndef HAVE_RGL
   RARCH_LOG("[Cg]: Vertex profile: %s\n", cgGetProfileString(cgVProf));
   RARCH_LOG("[Cg]: Fragment profile: %s\n", cgGetProfileString(cgFProf));
#endif
   cgGLSetOptimalOptions(cgFProf);
   cgGLSetOptimalOptions(cgVProf);
   cgGLEnableProfile(cgFProf);
   cgGLEnableProfile(cgVProf);

   if (path && strstr(path, ".cgp"))
   {
      if (!load_preset(path))
         return false;
   }
   else
   {
      if (!load_plain(path))
         return false;
   }

   if (!load_menu_shader())
      return false;

   prg[0].mvp = cgGetNamedParameter(prg[0].vprg, "modelViewProj");

   for (unsigned i = 1; i <= cg_shader->passes; i++)
      set_program_attributes(i);

   set_program_attributes(RARCH_CG_MENU_SHADER_INDEX);

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
      return cg_shader->passes;
   else
      return 0;
}

bool gl_cg_filter_type(unsigned index, bool *smooth)
{
   if (cg_active && index)
   {
      if (cg_shader->pass[index - 1].filter == RARCH_FILTER_UNSPEC)
         return false;
      *smooth = cg_shader->pass[index - 1].filter == RARCH_FILTER_LINEAR;
      return true;
   }
   else
      return false;
}

void gl_cg_shader_scale(unsigned index, struct gfx_fbo_scale *scale)
{
   if (cg_active && index)
      *scale = cg_shader->pass[index - 1].fbo;
   else
      scale->valid = false;
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

   if (path)
   {
      if (load_program(index, path, true))
      {
         set_program_attributes(index);
         return true;
      }
      else
      {
         // Always make sure we have a valid shader.
         prg[index] = prg[0];
         return false;
      }
   }
   else
   {
      prg[index] = prg[0];
      return true;
   }
}

void gl_cg_invalidate_context(void)
{
   cgCtx = NULL;
}

const gl_shader_backend_t gl_cg_backend = {
   gl_cg_init,
   gl_cg_deinit,
   gl_cg_set_params,
   gl_cg_use,
   gl_cg_num,
   gl_cg_filter_type,
   gl_cg_shader_scale,
   gl_cg_set_coords,
   gl_cg_set_mvp,

   gl_cg_load_shader,
   RARCH_SHADER_CG,
};

