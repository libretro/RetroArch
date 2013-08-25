#!/usr/bin/env python3

"""
   License statement applies to this file (glgen.py) only.
""" 

"""
   Permission is hereby granted, free of charge,
   to any person obtaining a copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation the rights to
   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
"""

import sys
import os
import re

banned_ext = [ 'AMD', 'APPLE', 'EXT', 'NV', 'NVX', 'ATI', '3DLABS', 'SUN', 'SGI', 'SGIX', 'SGIS', 'INTEL', '3DFX', 'IBM', 'MESA', 'GREMEDY', 'OML', 'PGI', 'I3D', 'INGL', 'MTX', 'QCOM', 'IMG', 'ANGLE', 'SUNX', 'INGR' ]

def noext(sym):
   for ext in banned_ext:
      if sym.endswith(ext):
         return False
   return True

def find_gl_symbols(lines):
   typedefs = []
   syms = []
   for line in lines:
      m = re.search(r'^typedef.+PFN(\S+)PROC.+$', line)
      g = re.search(r'^.+(gl\S+)\W*\(.+\).*$', line)
      if m and noext(m.group(1)):
         typedefs.append(m.group(0).replace('PFN', 'RGLSYM').replace('GLDEBUGPROC', 'RGLGENGLDEBUGPROC'))
      if g and noext(g.group(1)):
         syms.append(g.group(1))
   return (typedefs, syms)

def generate_defines(gl_syms):
   res = []
   for line in gl_syms:
      res.append('#define {} __rglgen_{}'.format(line, line))
   return res

def generate_declarations(gl_syms):
   return ['RGLSYM' + x.upper() + 'PROC ' + '__rglgen_' + x + ';' for x in gl_syms]

def generate_macros(gl_syms):
   return ['    SYM(' + x.replace('gl', '') + '),' for x in gl_syms]

def dump(f, lines):
   f.write('\n'.join(lines))
   f.write('\n\n')

if __name__ == '__main__':

   if len(sys.argv) > 4:
      for banned in sys.argv[4:]:
         banned_ext.append(banned)

   with open(sys.argv[1], 'r') as f:
      lines = f.readlines()
      typedefs, syms = find_gl_symbols(lines)

      overrides = generate_defines(syms)
      declarations = generate_declarations(syms)
      externs = ['extern ' + x for x in declarations]

      macros = generate_macros(syms)

   with open(sys.argv[2], 'w') as f:
      f.write('#ifndef RGLGEN_DECL_H__\n')
      f.write('#define RGLGEN_DECL_H__\n')

      f.write('#ifdef __cplusplus\n')
      f.write('extern "C" {\n')
      f.write('#endif\n')

      f.write('#ifdef GL_APIENTRY\n')
      f.write('typedef void (GL_APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);\n')
      f.write('#else\n')
      f.write('#ifndef APIENTRY\n')
      f.write('#define APIENTRY\n')
      f.write('#endif\n')
      f.write('#ifndef APIENTRYP\n')
      f.write('#define APIENTRYP APIENTRY *\n')
      f.write('#endif\n')
      f.write('typedef void (APIENTRY *RGLGENGLDEBUGPROCARB)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);\n')
      f.write('typedef void (APIENTRY *RGLGENGLDEBUGPROC)(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar*, GLvoid*);\n')
      f.write('#endif\n')

      f.write('#ifndef GL_OES_EGL_image\n')
      f.write('typedef void *GLeglImageOES;\n')
      f.write('#endif\n')

      f.write('#if !defined(GL_OES_fixed_point) && !defined(HAVE_OPENGLES2)\n')
      f.write('typedef GLint GLfixed;\n')
      f.write('#endif\n')

      dump(f, typedefs)
      dump(f, overrides)
      dump(f, externs)

      f.write('struct rglgen_sym_map { const char *sym; void *ptr; };\n')
      f.write('extern const struct rglgen_sym_map rglgen_symbol_map[];\n')

      f.write('#ifdef __cplusplus\n')
      f.write('}\n')
      f.write('#endif\n')

      f.write('#endif\n')

   with open(sys.argv[3], 'w') as f:
      f.write('#include "glsym.h"\n')
      f.write('#include <stddef.h>\n')
      f.write('#define SYM(x) { "gl" #x, &(gl##x) }\n')
      f.write('const struct rglgen_sym_map rglgen_symbol_map[] = {\n')
      dump(f, macros)
      f.write('    { NULL, NULL },\n')
      f.write('};\n')
      dump(f, declarations)

