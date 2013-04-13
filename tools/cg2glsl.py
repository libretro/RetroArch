#!/usr/bin/env python3

"""
Python 3 script which converts simple RetroArch Cg shaders to modern GLSL (ES) format.
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

def replace_global_in(source):
   split_source = source.split('\n')
   replace_table = [
      ('IN.video_size', 'InputSize'),
      ('IN.texture_size', 'TextureSize'),
      ('IN.output_size', 'OutputSize'),
      ('IN.frame_count', 'FrameCount'),
      ('IN.frame_direction', 'FrameDirection')
   ]

   for line in split_source:
      if '//var' in line:
         for index, replace in enumerate(replace_table):
            orig = line.split(' ')[2]
            if replace[0] == orig:
               replace_table[index] = (line.split(':')[2].split(' ')[1], replace_table[index][1])

   log('Replace globals:', replace_table)

   for replace in replace_table:
      if replace[0]:
         source = source.replace(replace[0], replace[1])

   return source


def replace_global_vertex(source):

   source = replace_global_in(source)
   replace_table = [
         ('POSITION', 'VertexCoord'),
         ('TEXCOORD1', 'LUTTexCoord'),
         ('TEXCOORD0', 'TexCoord'),
         ('TEXCOORD', 'TexCoord'),
         ('uniform vec4 _modelViewProj1[4]', 'uniform mat4 MVPMatrix'),
         ('_modelViewProj1', 'MVPMatrix'),
         ('MVPMatrix[0]', 'MVPMatrix_[0]'),
         ('MVPMatrix[1]', 'MVPMatrix_[1]'),
         ('MVPMatrix[2]', 'MVPMatrix_[2]'),
         ('MVPMatrix[3]', 'MVPMatrix_[3]'),

         ('FrameCount', 'float(FrameCount)'),
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

def no_uniform(elem):
   banned = [
         '_video_size',
         '_texture_size',
         '_output_size',
         '_output_dummy_size',
         '_frame_count',
         '_frame_direction',
         'sampler2D'
   ]

   for ban in banned:
      if ban in elem:
         return False
   return True

def destructify_varyings(source):
   #for line in source:
   #   log('  ', line)

   # We have to change varying structs that Cg support to single varyings for GL.
   # Varying structs aren't supported until later versions
   # of GLSL.

   # Global structs are sometimes used to store temporary data.
   # Don't try to remove this as it breaks compile.
   vout_lines = []
   for line in source:
      if ('//var' in line) and (('$vout.' in line) or ('$vin.' in line)):
         vout_lines.append(line)

   struct_types = []
   for line in source:
      if 'struct' in line:
         struct_type = line.split(' ')[1]
         if struct_type not in struct_types:
            struct_types.append(struct_type)

   log('Struct types:', struct_types)

   last_struct_decl_line = 0
   varyings = []
   varyings_name = []
   # Find all varyings in structs and make them "global" varyings.
   for struct in struct_types:
      for i, line in enumerate(source):
         if ('struct ' + struct) in line:
            j = i + 1
            while (j < len(source)) and ('};' not in source[j]):
               j += 1

            lines = ['varying ' + string for string in source[i + 1 : j]]
            varyings.extend(lines)
            names = [string.strip().split(' ')[1].split(';')[0].strip() for string in source[i + 1 : j]]
            varyings_name.extend(names)
            log('Found elements in struct', struct + ':', names)
            last_struct_decl_line = j

            # Must have explicit uniform sampler2D in struct.
            for index in range(i, j + 1):
               if 'sampler2D' in source[index]:
                  source[index] = ''

   varyings_tmp = varyings
   varyings = []
   variables = []

   # Don't include useless varyings like IN.video_size, IN.texture_size, etc as they are not actual varyings ...
   for i in filter(no_uniform, varyings_tmp):
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

            # Only redirect if the struct is actually used as vertex output.
            for vout_line in vout_lines:
               if variable in vout_line:
                  log('Found struct variable for', struct + ':', variable)
                  variables.append(variable)
                  break

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

   # Replace union <struct>. Sometimes we get collision in vertex and fragment.
   for index, line in enumerate(source):
      for struct_type in struct_types:
         line = line.replace('uniform ' + struct_type, struct_type)
      source[index] = line

   return source

def translate_varying(cg):
   #log('Translate:', cg)
   translations = {
      'ORIG.tex_coord'  : 'OrigTexCoord',
      'PREV.tex_coord'  : 'PrevTexCoord',
      'PREV1.tex_coord' : 'Prev1TexCoord',
      'PREV2.tex_coord' : 'Prev2TexCoord',
      'PREV3.tex_coord' : 'Prev3TexCoord',
      'PREV4.tex_coord' : 'Prev4TexCoord',
      'PREV5.tex_coord' : 'Prev5TexCoord',
      'PREV6.tex_coord' : 'Prev6TexCoord',
      'PASS1.tex_coord' : 'Pass1TexCoord',
      'PASS2.tex_coord' : 'Pass2TexCoord',
      'PASS3.tex_coord' : 'Pass3TexCoord',
      'PASS4.tex_coord' : 'Pass4TexCoord',
      'PASS5.tex_coord' : 'Pass5TexCoord',
      'PASS6.tex_coord' : 'Pass6TexCoord',
      'PASS7.tex_coord' : 'Pass7TexCoord',
      'PASS8.tex_coord' : 'Pass8TexCoord',
   }

   if cg in translations:
      return translations[cg]
   else:
      return cg


def replace_varyings(source):
   ret = []
   translations = []
   attribs = []
   for index, line in enumerate(source):
      if ('//var' in line) and ('$vin.' in line):
         orig = line.split(' ')[2]
         translated = translate_varying(orig)
         if translated != orig and translated not in attribs:
            cg_attrib = line.split(':')[2].split(' ')[1]
            translations.append((cg_attrib, translated))
            attribs.append(translated)

   for index, line in enumerate(source):
      if 'void main()' in line:
         for attrib in attribs:
            source.insert(index, 'attribute vec2 ' + attrib + ';')
         break

   for line in source:
      for trans in translations:
         line = line.replace(trans[0], trans[1])
      ret.append(line)

   return ret

def hack_source_vertex(source):
   transpose_index = 2
   code_index = 0
   for index, line in enumerate(source):
      if 'void main()' in line:
         source.insert(index + 2, '    mat4 MVPMatrix_ = transpose_(MVPMatrix);') # transpose() is GLSL 1.20+, doesn't exist in GLSL ES 1.0
         source.insert(index, '#endif')
         source.insert(index, 'uniform vec2 InputSize;')
         source.insert(index, 'uniform vec2 TextureSize;')
         source.insert(index, 'uniform vec2 OutputSize;')
         source.insert(index, '#else')
         source.insert(index, 'uniform mediump vec2 InputSize;')
         source.insert(index, 'uniform mediump vec2 TextureSize;')
         source.insert(index, 'uniform mediump vec2 OutputSize;')
         source.insert(index, '#ifdef GL_ES')
         source.insert(index, 'uniform int FrameCount;')
         source.insert(index, 'uniform int FrameDirection;')

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
   source = replace_varyings(source)
   return source

def replace_global_fragment(source):
   source = replace_global_in(source)
   replace_table = [
         ('FrameCount', 'float(FrameCount)'),
         ('input', 'input_dummy'),
         ('output', 'output_dummy'), # 'output' is reserved in GLSL.
   ]

   for replacement in replace_table:
      source = source.replace(replacement[0], replacement[1])

   return source

def translate_texture(cg):
   log('Translate:', cg)
   translations = {
      'ORIG.texture'  : 'OrigTexture',
      'PREV.texture'  : 'PrevTexture',
      'PREV1.texture' : 'Prev1Texture',
      'PREV2.texture' : 'Prev2Texture',
      'PREV3.texture' : 'Prev3Texture',
      'PREV4.texture' : 'Prev4Texture',
      'PREV5.texture' : 'Prev5Texture',
      'PREV6.texture' : 'Prev6Texture',
      'PASS1.texture' : 'Pass1Texture',
      'PASS2.texture' : 'Pass2Texture',
      'PASS3.texture' : 'Pass3Texture',
      'PASS4.texture' : 'Pass4Texture',
      'PASS5.texture' : 'Pass5Texture',
      'PASS6.texture' : 'Pass6Texture',
      'PASS7.texture' : 'Pass7Texture',
      'PASS8.texture' : 'Pass8Texture',
   }

   if cg in translations:
      return translations[cg]
   else:
      return cg

def hack_source_fragment(source):
   ref_index = 0
   for index, line in enumerate(source):
      if 'void main()' in line:
         source.insert(index, '#endif')
         source.insert(index, 'uniform vec2 InputSize;')
         source.insert(index, 'uniform vec2 TextureSize;')
         source.insert(index, 'uniform vec2 OutputSize;')
         source.insert(index, '#else')
         source.insert(index, 'uniform mediump vec2 InputSize;')
         source.insert(index, 'uniform mediump vec2 TextureSize;')
         source.insert(index, 'uniform mediump vec2 OutputSize;')
         source.insert(index, '#ifdef GL_ES')
         source.insert(index, 'uniform int FrameCount;')
         source.insert(index, 'uniform int FrameDirection;')
         ref_index = index
         break

   samplers = []
   added_samplers = []
   translated_samplers = []
   for line in source:
      if ('TEXUNIT0' in line) and ('semantic' not in line):
         main_sampler = (line.split(':')[2].split(' ')[1], 'Texture')
         samplers.append(main_sampler)
         log('Fragment: Sampler:', main_sampler[0], '->', main_sampler[1])
      elif '//var sampler2D' in line:
         cg_texture = line.split(' ')[2]
         translated = translate_texture(cg_texture)
         if translated != cg_texture and translated not in translated_samplers:
            translated_samplers.append(translated)
            added_samplers.append('uniform sampler2D ' + translated + ';')
         orig_name = translated
         new_name = line.split(':')[2].split(' ')[1]
         samplers.append((new_name, orig_name))
         log('Fragment: Sampler:', new_name, '->', orig_name)

   for sampler in added_samplers:
      source.insert(ref_index, sampler)

   ret = []
   for line in source:
      for sampler in samplers:
         line = line.replace(sampler[0], sampler[1])
      ret.append(line)

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
   log('Hacking vertex')
   vertex_source   = hack_source_vertex(vertex_source)
   log('Hacking fragment')
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
      f.write('// GLSL shader autogenerated by cg2glsl.py.\n')
      f.write('#if defined(VERTEX)\n')
      f.write(out_vertex)
      f.write('\n')
      f.write('#elif defined(FRAGMENT)\n')
      f.write(out_fragment)
      f.write('\n')
      f.write('#endif\n')
   return 0

def main():
   if len(sys.argv) != 3:
      print('Usage: {} prog.cg prog.glsl'.format(sys.argv[0]))
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

            dest = os.path.join(sys.argv[2], source.replace(sys.argv[1], '')[1:]).replace('.cg', '.glsl')
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

