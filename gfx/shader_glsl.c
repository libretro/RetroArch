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

#include <stdbool.h>
#include <string.h>
#include "general.h"
#include "shader_glsl.h"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif


#define NO_SDL_GLEXT
#include "SDL.h"
#include "SDL_opengl.h"
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "gl_common.h"

#ifdef __APPLE__
#define pglCreateProgram glCreateProgram
#define pglUseProgram glUseProgram
#define pglCreateShader glCreateShader
#define pglDeleteShader glDeleteShader
#define pglShaderSource glShaderSource
#define pglCompileShader glCompileShader
#define pglAttachShader glAttachShader
#define pglDetachShader glDetachShader
#define pglLinkProgram glLinkProgram
#define pglGetUniformLocation glGetUniformLocation
#define pglUniform1i glUniform1i
#define pglUniform2fv glUniform2fv
#define pglUniform4fv glUniform4fv
#define pglGetShaderiv glGetShaderiv
#define pglGetShaderInfoLog glGetShaderInfoLog
#define pglGetProgramiv glGetProgramiv
#define pglGetProgramInfoLog glGetProgramInfoLog
#else
static PFNGLCREATEPROGRAMPROC pglCreateProgram = NULL;
static PFNGLUSEPROGRAMPROC pglUseProgram = NULL;
static PFNGLCREATESHADERPROC pglCreateShader = NULL;
static PFNGLDELETESHADERPROC pglDeleteShader = NULL;
static PFNGLSHADERSOURCEPROC pglShaderSource = NULL;
static PFNGLCOMPILESHADERPROC pglCompileShader = NULL;
static PFNGLATTACHSHADERPROC pglAttachShader = NULL;
static PFNGLDETACHSHADERPROC pglDetachShader = NULL;
static PFNGLLINKPROGRAMPROC pglLinkProgram = NULL;
static PFNGLGETUNIFORMLOCATIONPROC pglGetUniformLocation = NULL;
static PFNGLUNIFORM1IPROC pglUniform1i = NULL;
static PFNGLUNIFORM2FVPROC pglUniform2fv = NULL;
static PFNGLUNIFORM4FVPROC pglUniform4fv = NULL;
static PFNGLGETSHADERIVPROC pglGetShaderiv = NULL;
static PFNGLGETSHADERINFOLOGPROC pglGetShaderInfoLog = NULL;
static PFNGLGETPROGRAMIVPROC pglGetProgramiv = NULL;
static PFNGLGETPROGRAMINFOLOGPROC pglGetProgramInfoLog = NULL;
#endif

#define MAX_PROGRAMS 16

enum filter_type
{
   SSNES_GL_NOFORCE,
   SSNES_GL_LINEAR,
   SSNES_GL_NEAREST
};

static bool glsl_enable = false;
static GLuint gl_program[MAX_PROGRAMS] = {0};
static enum filter_type gl_filter_type[MAX_PROGRAMS] = {SSNES_GL_NOFORCE};
static struct gl_fbo_scale gl_scale[MAX_PROGRAMS];
static unsigned gl_num_programs = 0;
static unsigned active_index = 0;

struct shader_program
{
   char *vertex;
   char *fragment;
   enum filter_type filter;

   float scale_x;
   float scale_y;
   unsigned abs_x;
   unsigned abs_y;
   enum gl_scale_type type_x;
   enum gl_scale_type type_y;

   bool valid_scale;
};

static void get_xml_attrs(struct shader_program *prog, xmlNodePtr ptr)
{
   prog->scale_x = 1.0;
   prog->scale_y = 1.0;
   prog->abs_x = 512;
   prog->abs_y = 512;
   prog->type_x = prog->type_y = SSNES_SCALE_INPUT;
   prog->valid_scale = false;

   // Check if shader forces a certain texture filtering.
   xmlChar *attr = xmlGetProp(ptr, (const xmlChar*)"filter");
   if (attr)
   {
      if (strcmp((const char*)attr, "nearest") == 0)
      {
         prog->filter = SSNES_GL_NEAREST;
         SSNES_LOG("XML: Shader forces GL_NEAREST.\n");
      }
      else if (strcmp((const char*)attr, "linear") == 0)
      {
         prog->filter = SSNES_GL_LINEAR;
         SSNES_LOG("XML: Shader forces GL_LINEAR.\n");
      }
      else
         SSNES_WARN("XML: Invalid property for filter.\n");

      xmlFree(attr);
   }
   else
      prog->filter = SSNES_GL_NOFORCE;

   // Check for scaling attributes *lots of code <_<*
   xmlChar *attr_scale = xmlGetProp(ptr, (const xmlChar*)"scale");
   xmlChar *attr_scale_x = xmlGetProp(ptr, (const xmlChar*)"scale_x");
   xmlChar *attr_scale_y = xmlGetProp(ptr, (const xmlChar*)"scale_y");
   xmlChar *attr_size = xmlGetProp(ptr, (const xmlChar*)"size");
   xmlChar *attr_size_x = xmlGetProp(ptr, (const xmlChar*)"size_x");
   xmlChar *attr_size_y = xmlGetProp(ptr, (const xmlChar*)"size_y");
   xmlChar *attr_outscale = xmlGetProp(ptr, (const xmlChar*)"outscale");
   xmlChar *attr_outscale_x = xmlGetProp(ptr, (const xmlChar*)"outscale_x");
   xmlChar *attr_outscale_y = xmlGetProp(ptr, (const xmlChar*)"outscale_y");

   if (attr_scale)
   {
      float scale = strtod((const char*)attr_scale, NULL);
      prog->scale_x = scale;
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_x = prog->type_y = SSNES_SCALE_INPUT;
      SSNES_LOG("Got scale attr: %.1f\n", scale);
   }

   if (attr_scale_x)
   {
      float scale = strtod((const char*)attr_scale_x, NULL);
      prog->scale_x = scale;
      prog->valid_scale = true;
      prog->type_x = SSNES_SCALE_INPUT;
      SSNES_LOG("Got scale_x attr: %.1f\n", scale);
   }

   if (attr_scale_y)
   {
      float scale = strtod((const char*)attr_scale_y, NULL);
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_y = SSNES_SCALE_INPUT;
      SSNES_LOG("Got scale_y attr: %.1f\n", scale);
   }
   
   if (attr_size)
   {
      prog->abs_x = prog->abs_y = strtoul((const char*)attr_size, NULL, 0);
      prog->valid_scale = true;
      prog->type_x = prog->type_y = SSNES_SCALE_ABSOLUTE;
      SSNES_LOG("Got size attr: %u\n", prog->abs_x);
   }

   if (attr_size_x)
   {
      prog->abs_x = strtoul((const char*)attr_size_x, NULL, 0);
      prog->valid_scale = true;
      prog->type_x = SSNES_SCALE_ABSOLUTE;
      SSNES_LOG("Got size_x attr: %u\n", prog->abs_x);
   }

   if (attr_size_y)
   {
      prog->abs_y = strtoul((const char*)attr_size_y, NULL, 0);
      prog->valid_scale = true;
      prog->type_y = SSNES_SCALE_ABSOLUTE;
      SSNES_LOG("Got size_y attr: %u\n", prog->abs_y);
   }

   if (attr_outscale)
   {
      float scale = strtod((const char*)attr_outscale, NULL);
      prog->scale_x = scale;
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_x = prog->type_y = SSNES_SCALE_VIEWPORT;
      SSNES_LOG("Got outscale attr: %.1f\n", scale);
   }

   if (attr_outscale_x)
   {
      float scale = strtod((const char*)attr_outscale_x, NULL);
      prog->scale_x = scale;
      prog->valid_scale = true;
      prog->type_x = SSNES_SCALE_VIEWPORT;
      SSNES_LOG("Got outscale_x attr: %.1f\n", scale);
   }

   if (attr_outscale_y)
   {
      float scale = strtod((const char*)attr_outscale_y, NULL);
      prog->scale_y = scale;
      prog->valid_scale = true;
      prog->type_y = SSNES_SCALE_VIEWPORT;
      SSNES_LOG("Got outscale_y attr: %.1f\n", scale);
   }


   if (attr_scale)
      xmlFree(attr_scale);
   if (attr_scale_x)
      xmlFree(attr_scale_x);
   if (attr_scale_y)
      xmlFree(attr_scale_y);
   if (attr_size)
      xmlFree(attr_size);
   if (attr_size_x)
      xmlFree(attr_size_x);
   if (attr_size_y)
      xmlFree(attr_size_y);
   if (attr_outscale)
      xmlFree(attr_outscale);
   if (attr_outscale_x)
      xmlFree(attr_outscale_x);
   if (attr_outscale_y)
      xmlFree(attr_outscale_y);

}

static unsigned get_xml_shaders(const char *path, struct shader_program *prog, size_t size)
{
   LIBXML_TEST_VERSION;

   xmlParserCtxtPtr ctx = xmlNewParserCtxt();
   if (!ctx)
   {
      SSNES_ERR("Failed to load libxml2 context.\n");
      return false;
   }

   SSNES_LOG("Loading XML shader: %s\n", path);
   xmlDocPtr doc = xmlCtxtReadFile(ctx, path, NULL, 0);
   if (!doc)
   {
      SSNES_ERR("Failed to parse XML file: %s\n", path);
      goto error;
   }

   if (ctx->valid == 0)
   {
      SSNES_ERR("Cannot validate XML shader: %s\n", path);
      goto error;
   }

   xmlNodePtr head = xmlDocGetRootElement(doc);
   xmlNodePtr cur = NULL;
   for (cur = head; cur; cur = cur->next)
   {
      if (cur->type == XML_ELEMENT_NODE && strcmp((const char*)cur->name, "shader") == 0)
      {
         xmlChar *attr;
         attr = xmlGetProp(cur, (const xmlChar*)"language");
         if (attr && strcmp((const char*)attr, "GLSL") == 0)
         {
            xmlFree(attr);
            break;
         }

         if (attr)
            xmlFree(attr);
      }
   }

   if (!cur) // We couldn't find any GLSL shader :(
      goto error;

   unsigned num = 0;
   memset(prog, 0, sizeof(struct shader_program) * size);

   // Iterate to check if we find fragment and/or vertex shaders.
   for (cur = cur->children; cur && num < size; cur = cur->next)
   {
      if (cur->type != XML_ELEMENT_NODE)
         continue;

      xmlChar *content = xmlNodeGetContent(cur);
      if (!content)
         continue;

      if (strcmp((const char*)cur->name, "vertex") == 0)
      {
         if (prog[num].vertex)
         {
            SSNES_ERR("Cannot have more than one vertex shader in a program.\n");
            xmlFree(content);
            goto error;
         }

         prog[num].vertex = (char*)content;
      }
      else if (strcmp((const char*)cur->name, "fragment") == 0)
      {
         prog[num].fragment = (char*)content;
         get_xml_attrs(&prog[num], cur);
         num++;
      }
   }

   if (num == 0)
   {
      SSNES_ERR("Couldn't find vertex shader nor fragment shader in XML file.\n");
      goto error;
   }

   xmlFreeDoc(doc);
   xmlFreeParserCtxt(ctx);
   return num;

error:
   SSNES_ERR("Failed to load XML shader ...\n");
   if (doc)
      xmlFreeDoc(doc);
   xmlFreeParserCtxt(ctx);
   return 0;
}

static void print_shader_log(GLuint obj)
{
   int info_len = 0;
   int max_len;

   pglGetShaderiv(obj, GL_INFO_LOG_LENGTH, &max_len);

   char info_log[max_len];
   pglGetShaderInfoLog(obj, max_len, &info_len, info_log);

   if (info_len > 0)
      SSNES_LOG("Shader log: %s\n", info_log);
}

static void print_linker_log(GLuint obj)
{
   int info_len = 0;
   int max_len;

   pglGetProgramiv(obj, GL_INFO_LOG_LENGTH, &max_len);

   char info_log[max_len];
   pglGetProgramInfoLog(obj, max_len, &info_len, info_log);

   if (info_len > 0)
      SSNES_LOG("Linker log: %s\n", info_log);
}

static void compile_programs(GLuint *gl_prog, struct shader_program *progs, size_t num)
{
   for (unsigned i = 0; i < num; i++)
   {
      gl_prog[i] = pglCreateProgram();

      if (progs[i].vertex)
      {
         SSNES_LOG("Found GLSL vertex shader.\n");
         GLuint shader = pglCreateShader(GL_VERTEX_SHADER);
         pglShaderSource(shader, 1, (const char**)&progs[i].vertex, 0);
         pglCompileShader(shader);
         print_shader_log(shader);

         pglAttachShader(gl_prog[i], shader);
         free(progs[i].vertex);
      }

      if (progs[i].fragment)
      {
         SSNES_LOG("Found GLSL fragment shader.\n");
         GLuint shader = pglCreateShader(GL_FRAGMENT_SHADER);
         pglShaderSource(shader, 1, (const char**)&progs[i].fragment, 0);
         pglCompileShader(shader);
         print_shader_log(shader);

         pglAttachShader(gl_prog[i], shader);
         free(progs[i].fragment);
      }

      if (progs[i].vertex || progs[i].fragment)
      {
         SSNES_LOG("Linking GLSL program.\n");
         pglLinkProgram(gl_prog[i]);
         pglUseProgram(gl_prog[i]);
         print_linker_log(gl_prog[i]);

         GLint location = pglGetUniformLocation(gl_prog[i], "rubyTexture");
         pglUniform1i(location, 0);
         pglUseProgram(0);
      }
   }
}

bool gl_glsl_init(const char *path)
{
#ifndef __APPLE__
   // Load shader functions.
   pglCreateProgram = SDL_GL_GetProcAddress("glCreateProgram");
   pglUseProgram = SDL_GL_GetProcAddress("glUseProgram");
   pglCreateShader = SDL_GL_GetProcAddress("glCreateShader");
   pglDeleteShader = SDL_GL_GetProcAddress("glDeleteShader");
   pglShaderSource = SDL_GL_GetProcAddress("glShaderSource");
   pglCompileShader = SDL_GL_GetProcAddress("glCompileShader");
   pglAttachShader = SDL_GL_GetProcAddress("glAttachShader");
   pglDetachShader = SDL_GL_GetProcAddress("glDetachShader");
   pglLinkProgram = SDL_GL_GetProcAddress("glLinkProgram");
   pglGetUniformLocation = SDL_GL_GetProcAddress("glGetUniformLocation");
   pglUniform1i = SDL_GL_GetProcAddress("glUniform1i");
   pglUniform2fv = SDL_GL_GetProcAddress("glUniform2fv");
   pglUniform4fv = SDL_GL_GetProcAddress("glUniform4fv");
   pglGetShaderiv = SDL_GL_GetProcAddress("glGetShaderiv");
   pglGetShaderInfoLog = SDL_GL_GetProcAddress("glGetShaderInfoLog");
   pglGetProgramiv = SDL_GL_GetProcAddress("glGetProgramiv");
   pglGetProgramInfoLog = SDL_GL_GetProcAddress("glGetProgramInfoLog");
#endif

   SSNES_LOG("Checking GLSL shader support ...\n");
#ifdef __APPLE__
   const bool shader_support = true;
#else
   bool shader_support = pglCreateProgram && pglUseProgram && pglCreateShader
      && pglDeleteShader && pglShaderSource && pglCompileShader && pglAttachShader
      && pglDetachShader && pglLinkProgram && pglGetUniformLocation
      && pglUniform1i && pglUniform2fv && pglUniform4fv
      && pglGetShaderiv && pglGetShaderInfoLog && pglGetProgramiv && pglGetProgramInfoLog;
#endif

   if (!shader_support)
   {
      SSNES_ERR("GLSL shaders aren't supported by your GL driver.\n");
      return false;
   }

   struct shader_program progs[MAX_PROGRAMS];
   unsigned num_progs = get_xml_shaders(path, progs, MAX_PROGRAMS - 1);

   if (num_progs == 0)
   {
      SSNES_ERR("Couldn't find any valid shaders in XML file.\n");
      return false;
   }

   for (unsigned i = 0; i < num_progs; i++)
   {
      gl_filter_type[i + 1] = progs[i].filter;
      gl_scale[i + 1].type_x = progs[i].type_x;
      gl_scale[i + 1].type_y = progs[i].type_y;
      gl_scale[i + 1].scale_x = progs[i].scale_x;
      gl_scale[i + 1].scale_y = progs[i].scale_y;
      gl_scale[i + 1].abs_x = progs[i].abs_x;
      gl_scale[i + 1].abs_y = progs[i].abs_y;
      gl_scale[i + 1].valid = progs[i].valid_scale;
   }

   compile_programs(&gl_program[1], progs, num_progs);

   // SSNES custom two-pass with two different files.
   if (num_progs == 1 && *g_settings.video.second_pass_shader)
   {
      unsigned secondary_progs = get_xml_shaders(g_settings.video.second_pass_shader, progs, 1);
      if (secondary_progs == 1)
      {
         compile_programs(&gl_program[2], progs, 1);
         num_progs++;
      }
      else
      {
         SSNES_ERR("Did not find valid shader in secondary shader file.\n");
         return false;
      }
   }

   if (!gl_check_error())
      return false;
   
   glsl_enable = true;
   gl_num_programs = num_progs;
   return true;
}

void gl_glsl_deinit(void)
{}

void gl_glsl_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height)
{
   if (glsl_enable && gl_program[active_index] > 0)
   {
      GLint location;

      float inputSize[2] = {width, height};
      location = pglGetUniformLocation(gl_program[active_index], "rubyInputSize");
      pglUniform2fv(location, 1, inputSize);

      float outputSize[2] = {out_width, out_height};
      location = pglGetUniformLocation(gl_program[active_index], "rubyOutputSize");
      pglUniform2fv(location, 1, outputSize);

      float textureSize[2] = {tex_width, tex_height};
      location = pglGetUniformLocation(gl_program[active_index], "rubyTextureSize");
      pglUniform2fv(location, 1, textureSize);
   }
}

void gl_glsl_set_proj_matrix(void)
{}

void gl_glsl_use(unsigned index)
{
   if (glsl_enable)
   {
      active_index = index;
      pglUseProgram(gl_program[index]);
   }
}

unsigned gl_glsl_num(void)
{
   return gl_num_programs;
}

bool gl_glsl_filter_type(unsigned index, bool *smooth)
{
   if (!glsl_enable)
      return false;

   switch (gl_filter_type[index])
   {
      case SSNES_GL_NOFORCE:
         return false;

      case SSNES_GL_NEAREST:
         *smooth = false;
         return true;

      case SSNES_GL_LINEAR:
         *smooth = true;
         return true;

      default:
         return false;
   }
}

void gl_glsl_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
   if (glsl_enable)
      *scale = gl_scale[index];
   else
      scale->valid = false;
}
