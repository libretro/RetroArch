#!/usr/bin/env python3

"""
Python 3 script which converts simple RetroArch Cg shaders to modern XML/GLSL format.
Author: Hans-Kristian Arntzen (Themaister)
License: Public domain
"""

import sys
import os
import errno
import subprocess

batch_mode = False

def log(*arg):
   if not batch_mode:
      print(*arg)

def remove_comments(source_lines):
   ret = []
   killed_comments = [line.split('//')[0] for line in source_lines]
   for i in filter(lambda line: len(line) > 0, killed_comments):
      ret.append(i)
   return ret

def keep_line_if(func, lines):
   ret = []
   for i in filter(func, lines):
      ret.append(i)
   return ret

def replace_global_vertex(source):
   replace_table = [
         ('POSITION', 'rubyVertexCoord'),
         ('TEXCOORD0', 'rubyTexCoord'),
         ('TEXCOORD', 'rubyTexCoord'),
         ('uniform vec4 _modelViewProj1[4]', 'uniform mat4 rubyMVPMatrix'),
         ('_modelViewProj1', 'rubyMVPMatrix'),
         ('rubyMVPMatrix[0]', 'rubyMVPMatrix_[0]'),
         ('rubyMVPMatrix[1]', 'rubyMVPMatrix_[1]'),
         ('rubyMVPMatrix[2]', 'rubyMVPMatrix_[2]'),
         ('rubyMVPMatrix[3]', 'rubyMVPMatrix_[3]'),
         ('_IN1._video_size', 'rubyInputSize'),
         ('_IN1._texture_size', 'rubyTextureSize'),
         ('_IN1._output_size', 'rubyOutputSize'),
         ('_IN1._frame_count', 'rubyFrameCount'),
         ('rubyFrameCount', 'float(rubyFrameCount)'),
         ('input', 'input_dummy'), # 'input' is reserved in GLSL.
         ('output', 'output_dummy'), # 'output' is reserved in GLSL.
   ]

   for replacement in replace_table:
      source = source.replace(replacement[0], replacement[1])

   return source

def translate_varyings(varyings, source):
   dictionary = {}
   for varying in varyings:
      for line in source:
         if (varying in line) and ('//var' in line):
            log('Found line for', varying + ':', line)
            dictionary[varying] = line.split(':')[0].split('.')[-1].strip()
            break

   return dictionary

def destructify_varyings(source):
   # We have to change varying structs that Cg support to single varyings for GL. Varying structs aren't supported until later versions
   # of GLSL.
   struct_types = []
   for line in source:
      if 'struct' in line:
         struct_types.append(line.split(' ')[1])

   log('Struct types:', struct_types)

   last_struct_decl_line = 0
   varyings = []
   varyings_name = []
   # Find all varyings in structs and make them "global" varyings.
   for struct in struct_types:
      for i, line in enumerate(source):
         if 'struct ' + struct in line:
            j = i + 1
            while (j < len(source)) and ('};' not in source[j]):
               j += 1
            varyings.extend(['varying ' + string for string in source[i + 1 : j]])
            names = [string.strip().split(' ')[1].split(';')[0].strip() for string in source[i + 1 : j]]
            varyings_name.extend(names)
            log('Found elements in struct', struct + ':', names)
            last_struct_decl_line = j

   varyings_tmp = varyings
   varyings = []
   variables = []

   # Don't include useless varyings like IN.video_size, IN.texture_size, etc as they are not actual varyings ...
   for i in filter(lambda v: ('_video_size' not in v) and ('_texture_size' not in v) and '_output_size' not in v, varyings_tmp):
      varyings.append(i)

   # Find any global variable struct that is supposed to be the output varying, and redirect all references to it to
   # the actual varyings we just declared ...
   # Globals only come before main() ...
   # Make sure to only look after all struct declarations as there might be overlap.
   for line in source[last_struct_decl_line:]:
      if 'void main()' in line:
         break

      for struct in struct_types:
         if struct in line:
            variable = line.split(' ')[1].split(';')[0]
            log('Found struct variable for', struct + ':', variable)
            variables.append(variable)

   varyings_dict = translate_varyings(varyings_name, source)
   log('Varyings dict:', varyings_dict)

   # Append all varyings. Keep the structs as they might be used as regular values.
   for varying in varyings:
      source.insert(1, varying)

   # Replace struct access with global access, e.g. (_co1._c00 => _c00)
   # Also replace mangled Cg name with 'real' name.
   for index, _ in enumerate(source):
      for variable in variables:
         source[index] = source[index].replace(variable + '.', '');

   for index, _ in enumerate(source):
      for varying_name in varyings_name:
         if varying_name in varyings_dict:
            source[index] = source[index].replace(varying_name, varyings_dict[varying_name])

   return source


def hack_source_vertex(source):
   transpose_index = 2
   code_index = 0
   for index, line in enumerate(source):
      if 'void main()' in line:
         source.insert(index + 2, '    mat4 rubyMVPMatrix_ = transpose_(rubyMVPMatrix);') # transpose() is GLSL 1.20+, doesn't exist in GLSL ES 1.0
         source.insert(index, '#endif')
         source.insert(index, 'uniform vec2 rubyInputSize;')
         source.insert(index, 'uniform vec2 rubyTextureSize;')
         source.insert(index, 'uniform vec2 rubyOutputSize;')
         source.insert(index, '#else')
         source.insert(index, 'uniform mediump vec2 rubyInputSize;')
         source.insert(index, 'uniform mediump vec2 rubyTextureSize;')
         source.insert(index, 'uniform mediump vec2 rubyOutputSize;')
         source.insert(index, '#ifdef GL_ES')
         source.insert(index, 'uniform int rubyFrameCount;')

         source.insert(index, """
         mat4 transpose_(mat4 matrix)
         {
            mat4 ret;
            for (int i = 0; i < 4; i++)
               for (int j = 0; j < 4; j++)
                  ret[i][j] = matrix[j][i];

            return ret;
         }
         """)
         break

   source = destructify_varyings(source)
   return source

def replace_global_fragment(source):
   replace_table = [
         ('_IN1._video_size', 'rubyInputSize'),
         ('_IN1._texture_size', 'rubyTextureSize'),
         ('_IN1._output_size', 'rubyOutputSize'),
         ('_IN1._frame_count', 'rubyFrameCount'),
         ('rubyFrameCount', 'float(rubyFrameCount)'),
         ('input', 'input_dummy'),
         ('output', 'output_dummy'), # 'output' is reserved in GLSL.
   ]

   for replacement in replace_table:
      source = source.replace(replacement[0], replacement[1])

   return source

def hack_source_fragment(source):
   for index, line in enumerate(source):
      if 'void main()' in line:
         source.insert(index, '#endif')
         source.insert(index, 'uniform vec2 rubyInputSize;')
         source.insert(index, 'uniform vec2 rubyTextureSize;')
         source.insert(index, 'uniform vec2 rubyOutputSize;')
         source.insert(index, '#else')
         source.insert(index, 'uniform mediump vec2 rubyInputSize;')
         source.insert(index, 'uniform mediump vec2 rubyTextureSize;')
         source.insert(index, 'uniform mediump vec2 rubyOutputSize;')
         source.insert(index, '#ifdef GL_ES')
         source.insert(index, 'uniform int rubyFrameCount;')
         break

   for line in source:
      if ('TEXUNIT0' in line) and ('semantic' not in line):
         sampler = line.split(':')[2].split(' ')[1]
         log('Fragment: Sampler:', sampler)
         break

   ret = []
   for line in source:
      ret.append(line.replace(sampler, 'rubyTexture'))

   ret = destructify_varyings(ret)
   return ret

def validate_shader(source, target):
   command = ['cgc', '-noentry', '-ogles']
   p = subprocess.Popen(command, stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
   stdout_ret, stderr_ret = p.communicate(source.encode())

   log('Shader:')
   log('===')
   log(source)
   log('===')
   log('CGC:', stderr_ret.decode())

   return p.returncode == 0

def convert(source, dest):
   vert_cmd = ['cgc', '-profile', 'glesv', '-entry', 'main_vertex', source]
   p = subprocess.Popen(vert_cmd, stderr = subprocess.PIPE, stdout = subprocess.PIPE)
   vertex_source, stderr_ret = p.communicate()
   log(stderr_ret.decode())
   vertex_source = vertex_source.decode()

   if p.returncode != 0:
      log('Vertex compilation failed ...')
      return 1

   frag_cmd = ['cgc', '-profile', 'glesf', '-entry', 'main_fragment', source]
   p = subprocess.Popen(frag_cmd, stderr = subprocess.PIPE, stdout = subprocess.PIPE)
   fragment_source, stderr_ret = p.communicate()
   log(stderr_ret.decode())
   fragment_source = fragment_source.decode()

   if p.returncode != 0:
      log('Vertex compilation failed ...')
      return 1

   vertex_source   = replace_global_vertex(vertex_source)
   fragment_source = replace_global_fragment(fragment_source)

   vertex_source   = vertex_source.split('\n')
   fragment_source = fragment_source.split('\n')

   # Cg think we're using row-major matrices, but we're using column major.
   # Also, Cg tends to compile matrix multiplications as dot products in GLSL.
   # Hack in a fix for this.
   vertex_source   = hack_source_vertex(vertex_source)
   fragment_source = hack_source_fragment(fragment_source)

   # We compile to GLES, but we really just want modern GL ...
   vertex_source   = keep_line_if(lambda line: 'precision' not in line, vertex_source)
   fragment_source = keep_line_if(lambda line: 'precision' not in line, fragment_source)

   # Kill all comments. Cg adds lots of useless comments.
   # Remove first line. It contains the name of the cg program.
   vertex_source   = remove_comments(vertex_source[1:])
   fragment_source = remove_comments(fragment_source[1:])

   out_vertex = '\n'.join(vertex_source)
   out_fragment = '\n'.join(['#ifdef GL_ES', 'precision mediump float;', '#endif'] + fragment_source)

   if not validate_shader(out_vertex, 'glesv'):
      log('Vertex shader does not compile ...')
      return 1

   if not validate_shader(out_fragment, 'glesf'):
      log('Fragment shader does not compile ...')
      return 1

   with open(dest, 'w') as f:
      f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
      f.write('<!-- XML/GLSL shader autogenerated by cg2xml.py -->\n')
      f.write('<shader language="GLSL" style="GLES2">\n')
      f.write('    <vertex><![CDATA[\n')

      f.write(out_vertex)
      f.write('\n')

      f.write(' ]]></vertex>\n')

      f.write('    <fragment><![CDATA[\n')

      f.write(out_fragment)
      f.write('\n')

      f.write(' ]]></fragment>\n')
      f.write('</shader>\n')

   return 0

def main():
   if len(sys.argv) != 3:
      print('Usage: {} prog.cg prog.shader'.format(sys.argv[0]))
      print('Batch mode usage: {} cg-dir out-xml-shader-dir'.format(sys.argv[0]))
      return 1

   if os.path.isdir(sys.argv[1]):
      global batch_mode
      batch_mode = True
      try:
         os.makedirs(sys.argv[2])
      except OSError as e:
         if e.errno != errno.EEXIST:
            raise

      failed_cnt = 0
      success_cnt = 0
      failed_files = []
      for dirname, _, filenames in os.walk(sys.argv[1]):
         for source in filter(lambda path: 'cg' == path.split('.')[-1], [os.path.join(dirname, filename) for filename in filenames]):

            dest = os.path.join(sys.argv[2], source.replace(sys.argv[1], '')[1:]).replace('.cg', '.shader')
            dirpath = os.path.split(dest)[0]
            print('Dirpath:', dirpath)
            if not os.path.isdir(dirpath):
               try:
                  os.makedirs(dirpath)
               except OSError as e:
                  if e.errno != errno.EEXIST:
                     raise

            try:
               ret = convert(source, dest)
               print(source, '->', dest, '...', 'suceeded!' if ret == 0 else 'failed!')

               if ret == 0:
                  success_cnt += 1
               else:
                  failed_cnt += 1
                  failed_files.append(source)
            except Exception as e:
               print(e)
               failed_files.append(source)
               failed_cnt += 1

      print(success_cnt, 'shaders converted successfully.')
      print(failed_cnt, 'shaders failed.')
      if failed_cnt > 0:
         print('Failed shaders:')
         for path in failed_files:
            print(path)

   else:
      source = sys.argv[1]
      dest   = sys.argv[2]
      sys.exit(convert(source, dest))

if __name__ == '__main__':
   sys.exit(main())

