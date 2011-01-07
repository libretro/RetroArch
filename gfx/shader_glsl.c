/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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

//
// GLSL code here is mostly copypasted from bSNES.
//

#include <stdbool.h>
#include <string.h>
#include "general.h"

#define NO_SDL_GLEXT
#include <GL/gl.h>
#include "SDL.h"
#include "SDL_opengl.h"
#include <stdlib.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <GL/glext.h>

static PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
static PFNGLUSEPROGRAMPROC glUseProgram = NULL;
static PFNGLCREATESHADERPROC glCreateShader = NULL;
static PFNGLDELETESHADERPROC glDeleteShader = NULL;
static PFNGLSHADERSOURCEPROC glShaderSource = NULL;
static PFNGLCOMPILESHADERPROC glCompileShader = NULL;
static PFNGLATTACHSHADERPROC glAttachShader = NULL;
static PFNGLDETACHSHADERPROC glDetachShader = NULL;
static PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
static PFNGLUNIFORM1IPROC glUniform1i = NULL;
static PFNGLUNIFORM2FVPROC glUniform2fv = NULL;
static PFNGLUNIFORM4FVPROC glUniform4fv = NULL;

static bool glsl_enable = false;
static GLuint gl_program;
static GLuint fragment_shader;
static GLuint vertex_shader;

static bool get_xml_shaders(const char *path, char **vertex_shader, char **fragment_shader)
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
         if ((attr = xmlGetProp(cur, (const xmlChar*)"language")) && strcmp((const char*)attr, "GLSL") == 0)
            break;
      }
   }

   if (!cur) // We couldn't find any GLSL shader :(
      goto error;

   bool vertex_found = false;
   bool fragment_found = false;
   // Iterate to check if we find fragment and/or vertex shaders.
   for (cur = cur->children; cur; cur = cur->next)
   {
      if (cur->type != XML_ELEMENT_NODE)
         continue;

      xmlChar *content = xmlNodeGetContent(cur);
      if (!content)
         continue;

      if (strcmp((const char*)cur->name, "vertex") == 0 && !vertex_found)
      {
         *vertex_shader = malloc(xmlStrlen(content) + 1);
         strcpy(*vertex_shader, (const char*)content);
         vertex_found = true;
      }
      else if (strcmp((const char*)cur->name, "fragment") == 0 && !fragment_found)
      {
         *fragment_shader = malloc(xmlStrlen(content) + 1);
         strcpy(*fragment_shader, (const char*)content);
         fragment_found = true;
      }
   }

   if (!vertex_found && !fragment_found)
   {
      SSNES_ERR("Couldn't find vertex shader nor fragment shader in XML file.\n");
      goto error;
   }


   xmlFreeDoc(doc);
   xmlFreeParserCtxt(ctx);
   return true;

error:
   if (doc)
      xmlFreeDoc(doc);
   xmlFreeParserCtxt(ctx);
   return false;
}

bool gl_glsl_init(const char *path)
{
   // Load shader functions.
   glCreateProgram = SDL_GL_GetProcAddress("glCreateProgram");
   glUseProgram = SDL_GL_GetProcAddress("glUseProgram");
   glCreateShader = SDL_GL_GetProcAddress("glCreateShader");
   glDeleteShader = SDL_GL_GetProcAddress("glDeleteShader");
   glShaderSource = SDL_GL_GetProcAddress("glShaderSource");
   glCompileShader = SDL_GL_GetProcAddress("glCompileShader");
   glAttachShader = SDL_GL_GetProcAddress("glAttachShader");
   glDetachShader = SDL_GL_GetProcAddress("glDetachShader");
   glLinkProgram = SDL_GL_GetProcAddress("glLinkProgram");
   glGetUniformLocation = SDL_GL_GetProcAddress("glGetUniformLocation");
   glUniform1i = SDL_GL_GetProcAddress("glUniform1i");
   glUniform2fv = SDL_GL_GetProcAddress("glUniform2fv");
   glUniform4fv = SDL_GL_GetProcAddress("glUniform4fv");

   SSNES_LOG("Checking GLSL shader support ...\n");
   bool shader_support = glCreateProgram && glUseProgram && glCreateShader
      && glDeleteShader && glShaderSource && glCompileShader && glAttachShader
      && glDetachShader && glLinkProgram && glGetUniformLocation
      && glUniform1i && glUniform2fv && glUniform4fv;

   if (!shader_support)
   {
      SSNES_ERR("GLSL shaders aren't supported by your GL driver.\n");
      return false;
   }

   gl_program = glCreateProgram();

   char *vertex_prog = NULL;
   char *fragment_prog = NULL;
   if (!get_xml_shaders(path, &vertex_prog, &fragment_prog))
      return false;

   if (vertex_prog)
   {
      vertex_shader = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(vertex_shader, 1, (const char**)&vertex_prog, 0);
      glCompileShader(vertex_shader);
      glAttachShader(gl_program, vertex_shader);
      free(vertex_prog);
   }
   if (fragment_prog)
   {
      fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(fragment_shader, 1, (const char**)&fragment_prog, 0);
      glCompileShader(fragment_shader);
      glAttachShader(gl_program, fragment_shader);
      free(fragment_prog);
   }

   if (vertex_prog || fragment_prog)
   {
      glLinkProgram(gl_program);
      glUseProgram(gl_program);
   }

   glsl_enable = true;
   return true;
}

void gl_glsl_deinit(void)
{}

void gl_glsl_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height)
{
   if (glsl_enable)
   {
      GLint location;

      float inputSize[2] = {width, height};
      location = glGetUniformLocation(gl_program, "rubyInputSize");
      glUniform2fv(location, 1, inputSize);

      float outputSize[2] = {out_width, out_height};
      location = glGetUniformLocation(gl_program, "rubyOutputSize");
      glUniform2fv(location, 1, outputSize);

      float textureSize[2] = {tex_width, tex_height};
      location = glGetUniformLocation(gl_program, "rubyTextureSize");
      glUniform2fv(location, 1, textureSize);
   }
}

void gl_glsl_set_proj_matrix(void)
{}
