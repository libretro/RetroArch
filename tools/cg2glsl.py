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
      ('IN.frame_direction', 'FrameDirection'),
   ]

   for line in split_source:
      if ('//var' in line) or ('#var' in line):
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
         ('attribute', 'COMPAT_ATTRIBUTE'),
         ('varying', 'COMPAT_VARYING'),
         ('texture2D', 'COMPAT_TEXTURE'),
         ('POSITION', 'VertexCoord'),
         ('TEXCOORD1', 'LUTTexCoord'),
         ('TEXCOORD0', 'TexCoord'),
         ('TEXCOORD', 'TexCoord'),
         ('uniform vec4 _modelViewProj1[4];', ''),
         ('_modelViewProj1', 'MVPMatrix'),
         ('_IN1._mvp_matrix[0]', 'MVPMatrix[0]'),
         ('_IN1._mvp_matrix[1]', 'MVPMatrix[1]'),
         ('_IN1._mvp_matrix[2]', 'MVPMatrix[2]'),
         ('_IN1._mvp_matrix[3]', 'MVPMatrix[3]'),

         ('FrameCount', 'float(FrameCount)'),
         ('FrameDirection', 'float(FrameDirection)'),
         ('input', 'input_dummy'), # 'input' is reserved in GLSL.
         ('output', 'output_dummy'), # 'output' is reserved in GLSL.
   ]

   for replacement in replace_table:
      source = source.replace(replacement[0], replacement[1])

   return source

def translate_varyings(varyings, source, direction):
   dictionary = {}
   for varying in varyings:
      for line in source:
         if (varying in line) and (('//var' in line) or ('#var' in line)) and (direction in line):
            log('Found line for', varying + ':', line)
            dictionary[varying] = 'VAR' + line.split(':')[0].split('.')[-1].strip()
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
         '_mvp_matrix',
         '_vertex_coord',
         'sampler2D'
   ]

   for ban in banned:
      if ban in elem:
         return False
   return True

def destructify_varyings(source, direction):
   # We have to change varying structs that Cg support to single varyings for GL.
   # Varying structs aren't supported until later versions
   # of GLSL.

   # Global structs are sometimes used to store temporary data.
   # Don't try to remove this as it breaks compile.
   vout_lines = []
   for line in source:
      if (('//var' in line) or ('#var' in line)) and (('$vout.' in line) or ('$vin.' in line)):
         vout_lines.append(line)

   struct_types = []
   for line in source[1:]:
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

            lines = ['COMPAT_VARYING ' + string for string in source[i + 1 : j]]
            varyings.extend(lines)
            names = [string.strip().split(' ')[1].split(';')[0].strip() for string in source[i + 1 : j]]
            varyings_name.extend(names)
            log('Found elements in struct', struct + ':', names)
            last_struct_decl_line = j

            # Must have explicit uniform sampler2D in struct.
            for index in range(i, j + 1):
               if 'sampler2D' in source[index]:
                  source[index] = 'float _placeholder{};'.format(index)

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
            decomment_line = line.split('//')[0].strip()
            if len(decomment_line) == 0:
               continue
            variable = decomment_line.split(' ')[1].split(';')[0]

            # Only redirect if the struct is actually used as vertex output.
            for vout_line in vout_lines:
               if variable in vout_line:
                  log('Found struct variable for', struct + ':', variable, 'in line:', line)
                  variables.append(variable)
                  break

   varyings_dict = translate_varyings(varyings_name, source, direction)
   log('Varyings dict:', varyings_dict)

   # Append all varyings. Keep the structs as they might be used as regular values.
   for varying in varyings:
      source.insert(1, varying)

   log('Variables:', variables)
   log('Varying names:', varyings_name)

   # Replace struct access with global access, e.g. (_co1._c00 => _c00)
   # Also replace mangled Cg name with 'real' name.
   for index, _ in enumerate(source):
      for variable in variables:
         for varying_name in varyings_dict:
            trans_from = variable + '.' + varying_name
            trans_to = varyings_dict[varying_name]
            source[index] = source[index].replace(trans_from, trans_to);

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
   # Ye, it's ugly as shit. :(
   #log('Translate:', cg)
   translations = {
      'IN.tex_coord'    : 'TexCoord',
      'IN.vertex_coord' : 'VertexCoord',
      'IN.lut_tex_coord' : 'LUTTexCoord',
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
      'PASSPREV2.tex_coord' : 'PassPrev2TexCoord',
      'PASSPREV3.tex_coord' : 'PassPrev3TexCoord',
      'PASSPREV4.tex_coord' : 'PassPrev4TexCoord',
      'PASSPREV5.tex_coord' : 'PassPrev5TexCoord',
      'PASSPREV6.tex_coord' : 'PassPrev6TexCoord',
      'PASSPREV7.tex_coord' : 'PassPrev7TexCoord',
      'PASSPREV8.tex_coord' : 'PassPrev8TexCoord',
   }

   if cg in translations:
      return translations[cg]
   else:
      return cg

def translate_texture_size(cg):
   # Ye, it's ugly as shit. :(
   #log('Translate:', cg)
   translations = {
      'ORIG.texture_size'  : 'OrigTextureSize',
      'PREV.texture_size'  : 'PrevTextureSize',
      'PREV1.texture_size' : 'Prev1TextureSize',
      'PREV2.texture_size' : 'Prev2TextureSize',
      'PREV3.texture_size' : 'Prev3TextureSize',
      'PREV4.texture_size' : 'Prev4TextureSize',
      'PREV5.texture_size' : 'Prev5TextureSize',
      'PREV6.texture_size' : 'Prev6TextureSize',
      'PASS1.texture_size' : 'Pass1TextureSize',
      'PASS2.texture_size' : 'Pass2TextureSize',
      'PASS3.texture_size' : 'Pass3TextureSize',
      'PASS4.texture_size' : 'Pass4TextureSize',
      'PASS5.texture_size' : 'Pass5TextureSize',
      'PASS6.texture_size' : 'Pass6TextureSize',
      'PASS7.texture_size' : 'Pass7TextureSize',
      'PASS8.texture_size' : 'Pass8TextureSize',
      'PASSPREV2.texture_size' : 'PassPrev2TextureSize',
      'PASSPREV3.texture_size' : 'PassPrev3TextureSize',
      'PASSPREV4.texture_size' : 'PassPrev4TextureSize',
      'PASSPREV5.texture_size' : 'PassPrev5TextureSize',
      'PASSPREV6.texture_size' : 'PassPrev6TextureSize',
      'PASSPREV7.texture_size' : 'PassPrev7TextureSize',
      'PASSPREV8.texture_size' : 'PassPrev8TextureSize',
      'ORIG.video_size'  : 'OrigInputSize',
      'PREV.video_size'  : 'PrevInputSize',
      'PREV1.video_size' : 'Prev1InputSize',
      'PREV2.video_size' : 'Prev2InputSize',
      'PREV3.video_size' : 'Prev3InputSize',
      'PREV4.video_size' : 'Prev4InputSize',
      'PREV5.video_size' : 'Prev5InputSize',
      'PREV6.video_size' : 'Prev6InputSize',
      'PASS1.video_size' : 'Pass1InputSize',
      'PASS2.video_size' : 'Pass2InputSize',
      'PASS3.video_size' : 'Pass3InputSize',
      'PASS4.video_size' : 'Pass4InputSize',
      'PASS5.video_size' : 'Pass5InputSize',
      'PASS6.video_size' : 'Pass6InputSize',
      'PASS7.video_size' : 'Pass7InputSize',
      'PASS8.video_size' : 'Pass8InputSize',
      'PASSPREV2.video_size' : 'PassPrev2InputSize',
      'PASSPREV3.video_size' : 'PassPrev3InputSize',
      'PASSPREV4.video_size' : 'PassPrev4InputSize',
      'PASSPREV5.video_size' : 'PassPrev5InputSize',
      'PASSPREV6.video_size' : 'PassPrev6InputSize',
      'PASSPREV7.video_size' : 'PassPrev7InputSize',
      'PASSPREV8.video_size' : 'PassPrev8InputSize',
   }

   if cg in translations:
      return translations[cg]
   else:
      return cg



def replace_varyings(source):
   ret = []
   translations = []
   attribs = []
   uniforms = []
   for index, line in enumerate(source):
      if (('//var' in line) or ('#var' in line)) and ('$vin.' in line):
         orig = line.split(' ')[2]
         translated = translate_varying(orig)
         if translated != orig and translated not in attribs:
            cg_attrib = line.split(':')[2].split(' ')[1]
            if len(cg_attrib.strip()) > 0:
               translations.append((cg_attrib, translated))
               attribs.append(translated)
      elif ('//var' in line) or ('#var' in line):
         orig = line.split(' ')[2]
         translated = translate_texture_size(orig)
         if translated != orig and translated not in uniforms:
            cg_uniform = line.split(':')[2].split(' ')[1]
            if len(cg_uniform.strip()) > 0:
               translations.append((cg_uniform, translated))
               uniforms.append(translated)

   for index, line in enumerate(source):
      if 'void main()' in line:
         for attrib in attribs:
            if attrib == 'VertexCoord':
               source.insert(index, 'COMPAT_ATTRIBUTE vec4 ' + attrib + ';')
            else:
               source.insert(index, 'COMPAT_ATTRIBUTE vec2 ' + attrib + ';')
         for uniform in uniforms:
            source.insert(index, 'uniform COMPAT_PRECISION vec2 ' + uniform + ';')
         break

   for line in source:
      for trans in translations:
         line = line.replace(trans[0], trans[1])
      ret.append(line)

   return ret

def hack_source_vertex(source):
   ref_index = 0
   for index, line in enumerate(source):
      if 'void main()' in line:
         source.insert(index, 'uniform COMPAT_PRECISION vec2 InputSize;')
         source.insert(index, 'uniform COMPAT_PRECISION vec2 TextureSize;')
         source.insert(index, 'uniform COMPAT_PRECISION vec2 OutputSize;')
         source.insert(index, 'uniform int FrameCount;')
         source.insert(index, 'uniform int FrameDirection;')
         source.insert(index, 'uniform mat4 MVPMatrix;')

         ref_index = index
         break

   # Fix samplers in vertex shader (supported by GLSL).
   translations = []
   added_samplers = []
   translated_samplers = []
   struct_texunit0 = False # If True, we have to append uniform sampler2D Texture manually ...
   for line in source:
      if ('TEXUNIT0' in line) and ('semantic' not in line):
         main_sampler = (line.split(':')[2].split(' ')[1], 'Texture')
         if len(main_sampler[0]) > 0:
            translations.append(main_sampler)
            log('Vertex: Sampler:', main_sampler[0], '->', main_sampler[1])
            struct_texunit0 = '.' in main_sampler[0]
      elif ('//var sampler2D' in line) or ('#var sampler2D' in line):
         cg_texture = line.split(' ')[2]
         translated = translate_texture(cg_texture)
         orig_name = translated
         new_name = line.split(':')[2].split(' ')[1]
         log('Vertex: Sampler:', new_name, '->', orig_name)
         if len(new_name) > 0:
            if translated != cg_texture and translated not in translated_samplers:
               translated_samplers.append(translated)
               added_samplers.append('uniform sampler2D ' + translated + ';')
            translations.append((new_name, orig_name))

   for sampler in added_samplers:
      source.insert(ref_index, sampler)
   if struct_texunit0:
      source.insert(ref_index, 'uniform sampler2D Texture;')
   for index, line in enumerate(source):
      for translation in translations:
         source[index] = source[index].replace(translation[0], translation[1])

   source = destructify_varyings(source, '$vout.')
   source = replace_varyings(source)
   return source

def replace_global_fragment(source):
   source = replace_global_in(source)
   replace_table = [
         ('varying', 'COMPAT_VARYING'),
         ('texture2D', 'COMPAT_TEXTURE'),
         ('FrameCount', 'float(FrameCount)'),
         ('FrameDirection', 'float(FrameDirection)'),
         ('input', 'input_dummy'),
         ('output', 'output_dummy'), # 'output' is reserved in GLSL.
         ('gl_FragColor', 'FragColor'),
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
      'PASSPREV2.texture' : 'PassPrev2Texture',
      'PASSPREV3.texture' : 'PassPrev3Texture',
      'PASSPREV4.texture' : 'PassPrev4Texture',
      'PASSPREV5.texture' : 'PassPrev5Texture',
      'PASSPREV6.texture' : 'PassPrev6Texture',
      'PASSPREV7.texture' : 'PassPrev7Texture',
      'PASSPREV8.texture' : 'PassPrev8Texture',
   }

   if cg in translations:
      return translations[cg]
   else:
      return cg

def hack_source_fragment(source):
   ref_index = 0
   for index, line in enumerate(source):
      if 'void main()' in line:
         source.insert(index, 'uniform COMPAT_PRECISION vec2 InputSize;')
         source.insert(index, 'uniform COMPAT_PRECISION vec2 TextureSize;')
         source.insert(index, 'uniform COMPAT_PRECISION vec2 OutputSize;')
         source.insert(index, 'uniform int FrameCount;')
         source.insert(index, 'uniform int FrameDirection;')
         ref_index = index
         break

   translations = []
   added_samplers = []
   translated_samplers = []
   uniforms = []
   struct_texunit0 = False # If True, we have to append uniform sampler2D Texture manually ...
   for line in source:
      if ('TEXUNIT0' in line) and ('semantic' not in line):
         main_sampler = (line.split(':')[2].split(' ')[1], 'Texture')
         if len(main_sampler[0]) > 0:
            translations.append(main_sampler)
            log('Fragment: Sampler:', main_sampler[0], '->', main_sampler[1])
            struct_texunit0 = '.' in main_sampler[0]
      elif ('//var sampler2D' in line) or ('#var sampler2D' in line):
         cg_texture = line.split(' ')[2]
         translated = translate_texture(cg_texture)
         orig_name = translated
         new_name = line.split(':')[2].split(' ')[1]
         log('Fragment: Sampler:', new_name, '->', orig_name)
         if len(new_name) > 0:
            if translated != cg_texture and translated not in translated_samplers:
               translated_samplers.append(translated)
               added_samplers.append('uniform sampler2D ' + translated + ';')
            translations.append((new_name, orig_name))
      elif ('//var' in line) or ('#var' in line):
         orig = line.split(' ')[2]
         translated = translate_texture_size(orig)
         if translated != orig and translated not in uniforms:
            cg_uniform = line.split(':')[2].split(' ')[1]
            if len(cg_uniform.strip()) > 0:
               translations.append((cg_uniform, translated))
               uniforms.append(translated)

   for sampler in added_samplers:
      source.insert(ref_index, sampler)
   for uniform in uniforms:
      source.insert(ref_index, 'uniform COMPAT_PRECISION vec2 ' + uniform + ';')
   if struct_texunit0:
      source.insert(ref_index, 'uniform sampler2D Texture;')

   ret = []
   for line in source:
      for translation in translations:
         log('Translation:', translation[0], '->', translation[1])
         line = line.replace(translation[0], translation[1])
      ret.append(line)

   ret = destructify_varyings(ret, '$vin.')
   return ret

def validate_shader(source, target):
   log('Shader:')
   log('===')
   log(source)
   log('===')

   command = ['cgc', '-noentry', '-ogles']
   p = subprocess.Popen(command, stdin = subprocess.PIPE, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
   stdout_ret, stderr_ret = p.communicate(source.encode())

   log('CGC:', stderr_ret.decode())

   return p.returncode == 0

def preprocess_vertex(source_data):
   input_data = source_data.split('\n')
   ret = []
   for line in input_data:
      if ('uniform' in line) and ('float4x4' in line):
         ret.append('#pragma pack_matrix(column_major)\n')
         ret.append(line)
         ret.append('#pragma pack_matrix(row_major)\n')
      else:
         ret.append(line)
   return '\n'.join(ret)

def convert(source, dest):
   vert_cmd = ['cgc', '-profile', 'glesv', '-entry', 'main_vertex']
   with open(source, 'r') as f:
      source_data = f.read()
   p = subprocess.Popen(vert_cmd, stdin = subprocess.PIPE, stderr = subprocess.PIPE, stdout = subprocess.PIPE)
   source_data = preprocess_vertex(source_data)
   vertex_source, stderr_ret = p.communicate(input = source_data.encode())
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

   vert_hacks = []
   vert_hacks.append('''
#if __VERSION__ >= 130
#define COMPAT_VARYING out
#define COMPAT_ATTRIBUTE in
#define COMPAT_TEXTURE texture
#else
#define COMPAT_VARYING varying 
#define COMPAT_ATTRIBUTE attribute 
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif''')

   out_vertex = '\n'.join(vert_hacks + vertex_source)

   frag_hacks = []
   frag_hacks.append('''
#if __VERSION__ >= 130
#define COMPAT_VARYING in
#define COMPAT_TEXTURE texture
out vec4 FragColor;
#else
#define COMPAT_VARYING varying
#define FragColor gl_FragColor
#define COMPAT_TEXTURE texture2D
#endif

#ifdef GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif
#define COMPAT_PRECISION mediump
#else
#define COMPAT_PRECISION
#endif''')

   out_fragment = '\n'.join(frag_hacks + fragment_source)

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

def convert_cgp(source, dest):
   string = ''
   with open(source, 'r') as f:
      string = f.read().replace('.cg', '.glsl')

   open(dest, 'w').write(string)

def path_ext(path):
   _, ext = os.path.splitext(path)
   return ext

def convert_path(source, source_dir, dest_dir, conv):
   index = 0 if source_dir[-1] == '/' else 1
   return os.path.join(dest_dir, source.replace(source_dir, '')[index:]).replace(conv[0], conv[1])

def main():
   if len(sys.argv) != 3:
      print('Usage: {} prog.cg(p) prog.glsl(p)'.format(sys.argv[0]))
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
         for source in filter(lambda path: path_ext(path) == '.cg', [os.path.join(dirname, filename) for filename in filenames]):
            dest = convert_path(source, sys.argv[1], sys.argv[2], ('.cg', '.glsl'))
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

         for source in filter(lambda path: path_ext(path) == '.cgp', [os.path.join(dirname, filename) for filename in filenames]):
            dest = convert_path(source, sys.argv[1], sys.argv[2], ('.cgp', '.glslp'))
            dirpath = os.path.split(dest)[0]
            print('Dirpath:', dirpath)
            if not os.path.isdir(dirpath):
               try:
                  os.makedirs(dirpath)
               except OSError as e:
                  if e.errno != errno.EEXIST:
                     raise

            try:
               convert_cgp(source, dest)
               success_cnt += 1
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

      if path_ext(source) == '.cgp':
         sys.exit(convert_cgp(source, dest))
      else:
         sys.exit(convert(source, dest))

if __name__ == '__main__':
   sys.exit(main())

