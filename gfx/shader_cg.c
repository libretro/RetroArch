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
      "	out float2 otexCoord : TEXCOORD"
      ")"
      "{"
      "	oPosition = mul(modelViewProj, position);"
      "	oColor = color;"
      "	otexCoord = texCoord;"
      "}"
      ""
      ""
      "struct output "
      "{"
      "  float4 color    : COLOR;"
      "};"
      ""
      "output main_fragment(float2 texCoord : TEXCOORD0, uniform sampler2D decal : TEXUNIT0) "
      "{"
      "   output OUT;"
      "   OUT.color = tex2D(decal, texCoord);"
      "   return OUT;"
      "}";


static CGcontext cgCtx;
struct cg_program
{
   CGprogram vprg;
   CGprogram fprg;
   CGparameter vid_size_f;
   CGparameter tex_size_f;
   CGparameter out_size_f;
   CGparameter vid_size_v;
   CGparameter tex_size_v;
   CGparameter out_size_v;
   CGparameter mvp;
};

static struct cg_program prg[3];
static bool cg_active = false;
static CGprofile cgVProf, cgFProf;
static unsigned active_index = 0;

void gl_cg_set_proj_matrix(void)
{
   if (cg_active)
      cgGLSetStateMatrixParameter(prg[active_index].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);
}

void gl_cg_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height)
{
   if (cg_active)
   {
      cgGLSetParameter2f(prg[active_index].vid_size_f, width, height);
      cgGLSetParameter2f(prg[active_index].tex_size_f, tex_width, tex_height);
      cgGLSetParameter2f(prg[active_index].out_size_f, out_width, out_height);

      cgGLSetParameter2f(prg[active_index].vid_size_v, width, height);
      cgGLSetParameter2f(prg[active_index].tex_size_v, tex_width, tex_height);
      cgGLSetParameter2f(prg[active_index].out_size_v, out_width, out_height);
   }
}

void gl_cg_deinit(void)
{
   if (cg_active)
      cgDestroyContext(cgCtx);
}

bool gl_cg_init(const char *path)
{
   SSNES_LOG("Loading Cg file: %s\n", path);
   if (strlen(g_settings.video.second_pass_shader) > 0)
      SSNES_LOG("Loading 2nd pass: %s\n", g_settings.video.second_pass_shader);

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


   prg[0].fprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_program, cgFProf, "main_fragment", 0);
   prg[0].vprg = cgCreateProgram(cgCtx, CG_SOURCE, stock_cg_program, cgVProf, "main_vertex", 0);

   prg[1].fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, path, cgFProf, "main_fragment", 0);
   prg[1].vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, path, cgVProf, "main_vertex", 0);

   if (strlen(g_settings.video.second_pass_shader) > 0)
   {
      prg[2].fprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, g_settings.video.second_pass_shader, cgFProf, "main_fragment", 0);
      prg[2].vprg = cgCreateProgramFromFile(cgCtx, CG_SOURCE, g_settings.video.second_pass_shader, cgVProf, "main_vertex", 0);
   }
   else
      prg[2] = prg[0];

   for (int i = 0; i < 3; i++)
   {
      if (prg[i].fprg == NULL || prg[i].vprg == NULL)
      {
         CGerror err = cgGetError();
         SSNES_ERR("CG error: %s\n", cgGetErrorString(err));
         return false;
      }

      cgGLLoadProgram(prg[i].fprg);
      cgGLLoadProgram(prg[i].vprg);
   }

   cgGLEnableProfile(cgFProf);
   cgGLEnableProfile(cgVProf);

   for (int i = 0; i < 3; i++)
   {
      cgGLBindProgram(prg[i].fprg);
      cgGLBindProgram(prg[i].vprg);

      prg[i].vid_size_f = cgGetNamedParameter(prg[i].fprg, "IN.video_size");
      prg[i].tex_size_f = cgGetNamedParameter(prg[i].fprg, "IN.texture_size");
      prg[i].out_size_f = cgGetNamedParameter(prg[i].fprg, "IN.output_size");
      prg[i].vid_size_v = cgGetNamedParameter(prg[i].vprg, "IN.video_size");
      prg[i].tex_size_v = cgGetNamedParameter(prg[i].vprg, "IN.texture_size");
      prg[i].out_size_v = cgGetNamedParameter(prg[i].vprg, "IN.output_size");
      prg[i].mvp = cgGetNamedParameter(prg[i].vprg, "modelViewProj");
      cgGLSetStateMatrixParameter(prg[i].mvp, CG_GL_MODELVIEW_PROJECTION_MATRIX, CG_GL_MATRIX_IDENTITY);
   }

   cgGLBindProgram(prg[1].fprg);
   cgGLBindProgram(prg[1].vprg);

   cg_active = true;
   return true;
}

void gl_cg_use(unsigned index)
{
   if (cg_active)
   {
      active_index = index;
      cgGLBindProgram(prg[index].vprg);
      cgGLBindProgram(prg[index].fprg);
   }
}

unsigned gl_cg_num(void)
{
   if (cg_active)
   {
      if (prg[0].fprg == prg[2].fprg)
         return 1;
      else
         return 2;
   }
   else
      return 0;
}

bool gl_cg_filter_type(unsigned index, bool *smooth)
{
   (void)index;
   (void)smooth;
   // We don't really care since .cg doesn't have those kinds of semantics by itself ...
   return false;
}

bool gl_cg_shader_rect(unsigned index, struct gl_fbo_rect *rect)
{
   (void)index;
   (void)rect;
   // We don't really care since .cg doesn't have those kinds of semantics by itself ...
   return false;
}
