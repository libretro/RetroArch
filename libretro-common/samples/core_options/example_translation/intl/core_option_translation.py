#!/usr/bin/env python3

"""Core options text extractor

The purpose of this script is to set up & provide functions for automatic generation of 'libretro_core_options_intl.h'
from 'libretro_core_options.h' using translations from Crowdin.

Both v1 and v2 structs are supported. It is, however, recommended to convert v1 files to v2 using the included
'v1_to_v2_converter.py'.

Usage:
python3 path/to/core_option_translation.py "path/to/where/libretro_core_options.h & libretro_core_options_intl.h/are" "core_name"

This script will:
1.) create key words for & extract the texts from libretro_core_options.h & save them into intl/_us/core_options.h
2.) do the same for any present translations in libretro_core_options_intl.h, saving those in their respective folder
"""
import core_option_regex as cor
import re
import os
import sys
import json
import urllib.request as req
import shutil

# LANG_CODE_TO_R_LANG = {'_ar': 'RETRO_LANGUAGE_ARABIC',
#                        '_ast': 'RETRO_LANGUAGE_ASTURIAN',
#                        '_chs': 'RETRO_LANGUAGE_CHINESE_SIMPLIFIED',
#                        '_cht': 'RETRO_LANGUAGE_CHINESE_TRADITIONAL',
#                        '_cs': 'RETRO_LANGUAGE_CZECH',
#                        '_cy': 'RETRO_LANGUAGE_WELSH',
#                        '_da': 'RETRO_LANGUAGE_DANISH',
#                        '_de': 'RETRO_LANGUAGE_GERMAN',
#                        '_el': 'RETRO_LANGUAGE_GREEK',
#                        '_eo': 'RETRO_LANGUAGE_ESPERANTO',
#                        '_es': 'RETRO_LANGUAGE_SPANISH',
#                        '_fa': 'RETRO_LANGUAGE_PERSIAN',
#                        '_fi': 'RETRO_LANGUAGE_FINNISH',
#                        '_fr': 'RETRO_LANGUAGE_FRENCH',
#                        '_gl': 'RETRO_LANGUAGE_GALICIAN',
#                        '_he': 'RETRO_LANGUAGE_HEBREW',
#                        '_hu': 'RETRO_LANGUAGE_HUNGARIAN',
#                        '_id': 'RETRO_LANGUAGE_INDONESIAN',
#                        '_it': 'RETRO_LANGUAGE_ITALIAN',
#                        '_ja': 'RETRO_LANGUAGE_JAPANESE',
#                        '_ko': 'RETRO_LANGUAGE_KOREAN',
#                        '_nl': 'RETRO_LANGUAGE_DUTCH',
#                        '_oc': 'RETRO_LANGUAGE_OCCITAN',
#                        '_pl': 'RETRO_LANGUAGE_POLISH',
#                        '_pt_br': 'RETRO_LANGUAGE_PORTUGUESE_BRAZIL',
#                        '_pt_pt': 'RETRO_LANGUAGE_PORTUGUESE_PORTUGAL',
#                        '_ru': 'RETRO_LANGUAGE_RUSSIAN',
#                        '_sk': 'RETRO_LANGUAGE_SLOVAK',
#                        '_sv': 'RETRO_LANGUAGE_SWEDISH',
#                        '_tr': 'RETRO_LANGUAGE_TURKISH',
#                        '_uk': 'RETRO_LANGUAGE_UKRAINIAN',
#                        '_us': 'RETRO_LANGUAGE_ENGLISH',
#                        '_vn': 'RETRO_LANGUAGE_VIETNAMESE'}

# these are handled by RetroArch directly - no need to include them in core translations
ON_OFFS = {'"enabled"', '"disabled"', '"true"', '"false"', '"on"', '"off"'}


def remove_special_chars(text: str, char_set=0, allow_non_ascii=False) -> str:
   """Removes special characters from a text.

   :param text: String to be cleaned.
   :param char_set: 0 -> remove all ASCII special chars except for '_' & 'space' (default)
                    1 -> remove invalid chars from file names
   :param allow_non_ascii: False -> all non-ascii characters will be removed (default)
                           True -> non-ascii characters will be passed through
   :return: Clean text.
   """
   command_chars = [chr(unicode) for unicode in tuple(range(0, 32)) + (127,)]
   special_chars = ([chr(unicode) for unicode in tuple(range(33, 48)) + tuple(range(58, 65)) + tuple(range(91, 95))
                     + (96,) + tuple(range(123, 127))],
                    ('\\', '/', ':', '*', '?', '"', '<', '>', '|', '#', '%',
                     '&', '{', '}', '$', '!', '¸', "'", '@', '+', '='))
   res = text if allow_non_ascii \
      else text.encode('ascii', errors='ignore').decode('unicode-escape')

   for cm in command_chars:
      res = res.replace(cm, '_')
   for sp in special_chars[char_set]:
      res = res.replace(sp, '_')
   while res.startswith('_'):
      res = res[1:]
   while res.endswith('_'):
      res = res[:-1]
   return res


def clean_file_name(file_name: str) -> str:
   """Removes characters which might make file_name inappropriate for files on some OS.

   :param file_name: File name to be cleaned.
   :return: The clean file name.
   """
   file_name = remove_special_chars(file_name, 1)
   file_name = re.sub(r'__+', '_', file_name.replace(' ', '_'))
   return file_name


def get_struct_type_name(decl: str) -> tuple:
   """ Returns relevant parts of the struct declaration:
   type, name of the struct and the language appendix, if present.
   :param decl: The struct declaration matched by cor.p_type_name.
   :return: Tuple, e.g.: ('retro_core_option_definition', 'option_defs_us', '_us')
   """
   struct_match = cor.p_type_name.search(decl)
   if struct_match:
      if struct_match.group(3):
         struct_type_name = struct_match.group(1, 2, 3)
         return struct_type_name
      elif struct_match.group(4):
         struct_type_name = struct_match.group(1, 2, 4)
         return struct_type_name
      else:
         struct_type_name = struct_match.group(1, 2)
         return struct_type_name
   else:
      raise ValueError(f'No or incomplete struct declaration: {decl}!\n'
                       'Please make sure all structs are complete, including the type and name declaration.')


def is_viable_non_dupe(text: str, comparison) -> bool:
   """text must be longer than 2 ('""'), not 'NULL' and not in comparison.

   :param text: String to be tested.
   :param comparison: Dictionary or set to search for text in.
   :return: bool
   """
   return 2 < len(text) and text != 'NULL' and text not in comparison


def is_viable_value(text: str) -> bool:
   """text must be longer than 2 ('""') and not 'NULL'.

   :param text: String to be tested.
   :return: bool
   """
   return 2 < len(text) and text != 'NULL'


def create_non_dupe(base_name: str, opt_num: int, comparison) -> str:
   """Makes sure base_name is not in comparison, and if it is it's renamed.

   :param base_name: Name to check/make unique.
   :param opt_num: Number of the option base_name belongs to, used in making it unique.
   :param comparison: Dictionary or set to search for base_name in.
   :return: Unique name.
   """
   h = base_name
   if h in comparison:
      n = 0
      h = h + '_O' + str(opt_num)
      h_end = len(h)
      while h in comparison:
         h = h[:h_end] + '_' + str(n)
         n += 1
   return h


def get_texts(text: str) -> dict:
   """Extracts the strings, which are to be translated/are the translations,
   from text and creates macro names for them.

   :param text: The string to be parsed.
   :return: Dictionary of the form { '_<lang>': { 'macro': 'string', ... }, ... }.
   """
   # all structs: group(0) full struct, group(1) beginning, group(2) content
   structs = cor.p_struct.finditer(text)
   hash_n_string = {}
   just_string = {}
   for struct in structs:
      struct_declaration = struct.group(1)
      struct_type_name = get_struct_type_name(struct_declaration)
      if 3 > len(struct_type_name):
         lang = '_us'
      else:
         lang = struct_type_name[2]
      if lang not in just_string:
         hash_n_string[lang] = {}
         just_string[lang] = set()
      is_v2_definition = 'retro_core_option_v2_definition' == struct_type_name[0]
      pre_name = ''
      # info texts format
      p = cor.p_info
      if 'retro_core_option_v2_category' == struct_type_name[0]:
         # prepend category labels, as they can be the same as option labels
         pre_name = 'CATEGORY_'
         # categories have different info texts format
         p = cor.p_info_cat

      struct_content = struct.group(4)
      # 0: full option; 1: key; 2: description; 3: additional info; 4: key/value pairs
      struct_options = cor.p_option.finditer(struct_content)
      for opt, option in enumerate(struct_options):
         # group 1: key
         if option.group(1):
            opt_name = pre_name + option.group(1)
            # no special chars allowed in key
            opt_name = remove_special_chars(opt_name).upper().replace(' ', '_')
         else:
            raise ValueError(f'No option name (key) found in struct {struct_type_name[1]} option {opt}!')

         # group 2: description0
         if option.group(2):
            desc0 = option.group(2)
            if is_viable_non_dupe(desc0, just_string[lang]):
               just_string[lang].add(desc0)
               m_h = create_non_dupe(re.sub(r'__+', '_', f'{opt_name}_LABEL'), opt, hash_n_string[lang])
               hash_n_string[lang][m_h] = desc0
         else:
            raise ValueError(f'No label found in struct {struct_type_name[1]} option {option.group(1)}!')

         # group 3: desc1, info0, info1, category
         if option.group(3):
            infos = option.group(3)
            option_info = p.finditer(infos)
            if is_v2_definition:
               desc1 = next(option_info).group(1)
               if is_viable_non_dupe(desc1, just_string[lang]):
                  just_string[lang].add(desc1)
                  m_h = create_non_dupe(re.sub(r'__+', '_', f'{opt_name}_LABEL_CAT'), opt, hash_n_string[lang])
                  hash_n_string[lang][m_h] = desc1
               last = None
               m_h = None
               for j, info in enumerate(option_info):
                  last = info.group(1)
                  if is_viable_non_dupe(last, just_string[lang]):
                     just_string[lang].add(last)
                     m_h = create_non_dupe(re.sub(r'__+', '_', f'{opt_name}_INFO_{j}'), opt,
                                           hash_n_string[lang])
                     hash_n_string[lang][m_h] = last
               if last in just_string[lang]:  # category key should not be translated
                  hash_n_string[lang].pop(m_h)
                  just_string[lang].remove(last)
            else:
               for j, info in enumerate(option_info):
                  gr1 = info.group(1)
                  if is_viable_non_dupe(gr1, just_string[lang]):
                     just_string[lang].add(gr1)
                     m_h = create_non_dupe(re.sub(r'__+', '_', f'{opt_name}_INFO_{j}'), opt,
                                           hash_n_string[lang])
                     hash_n_string[lang][m_h] = gr1
         else:
            raise ValueError(f'Too few arguments in struct {struct_type_name[1]} option {option.group(1)}!')

         # group 4: key/value pairs
         if option.group(4):
            for j, kv_set in enumerate(cor.p_key_value.finditer(option.group(4))):
               set_key, set_value = kv_set.group(1, 2)
               if not is_viable_value(set_value):
                  # use the key if value not available
                  set_value = set_key
                  if not is_viable_value(set_value):
                     continue
               # re.fullmatch(r'(?:[+-][0-9]+)+', value[1:-1])

               # add only if non-dupe, not translated by RetroArch directly & not purely numeric
               if set_value not in just_string[lang]\
                  and set_value.lower() not in ON_OFFS\
                  and not re.sub(r'[+-]', '', set_value[1:-1]).isdigit():
                  clean_key = set_key[1:-1]
                  clean_key = remove_special_chars(clean_key).upper().replace(' ', '_')
                  m_h = create_non_dupe(re.sub(r'__+', '_', f"OPTION_VAL_{clean_key}"), opt, hash_n_string[lang])
                  hash_n_string[lang][m_h] = set_value
                  just_string[lang].add(set_value)
   return hash_n_string


def create_msg_hash(intl_dir_path: str, core_name: str, keyword_string_dict: dict) -> dict:
   """Creates '<core_name>.h' files in 'intl/_<lang>/' containing the macro name & string combinations.

   :param intl_dir_path: Path to the intl directory.
   :param core_name: Name of the core, used for the files' paths.
   :param keyword_string_dict: Dictionary of the form { '_<lang>': { 'macro': 'string', ... }, ... }.
   :return: Dictionary of the form { '_<lang>': 'path/to/file (./intl/_<lang>/<core_name>.h)', ... }.
   """
   files = {}
   for localisation in keyword_string_dict:
      path = os.path.join(intl_dir_path, core_name)  # intl/<core_name>/
      files[localisation] = os.path.join(path, localisation + '.h')  # intl/<core_name>/_<lang>.h
      if not os.path.exists(path):
         os.makedirs(path)
      with open(files[localisation], 'w', encoding='utf-8') as crowdin_file:
         out_text = ''
         for keyword in keyword_string_dict[localisation]:
            out_text = f'{out_text}{keyword} {keyword_string_dict[localisation][keyword]}\n'
         crowdin_file.write(out_text)
   return files


def h2json(file_paths: dict) -> dict:
   """Converts .h files pointed to by file_paths into .jsons.

   :param file_paths: Dictionary of the form { '_<lang>': 'path/to/file (./intl/_<lang>/<core_name>.h)', ... }.
   :return: Dictionary of the form { '_<lang>': 'path/to/file (./intl/_<lang>/<core_name>.json)', ... }.
   """
   jsons = {}
   for file_lang in file_paths:
      if not os.path.isfile(file_paths[file_lang]):
         continue
      file_path = file_paths[file_lang]
      try:
         jsons[file_lang] = file_path[:file_path.rindex('.')] + '.json'
      except ValueError:
         print(f"File {file_path} has incorrect format! File ending missing?")
         continue

      p = cor.p_masked

      with open(file_paths[file_lang], 'r+', encoding='utf-8') as h_file:
         text = h_file.read()
         result = p.finditer(text)
         messages = {}
         for msg in result:
            key, val = msg.group(1, 2)
            if key not in messages:
               if key and val:
                  # unescape & remove "\n"
                  messages[key] = re.sub(r'"\s*(?:(?:/\*(?:.|[\r\n])*?\*/|//.*[\r\n]+)\s*)*"',
                                         '\\\n', val[1:-1].replace('\\\"', '"'))
            else:
               print(f"DUPLICATE KEY in {file_paths[file_lang]}: {key}")
         with open(jsons[file_lang], 'w', encoding='utf-8') as json_file:
            json.dump(messages, json_file, indent=2)

   return jsons


def json2h(intl_dir_path: str, file_list) -> None:
   """Converts .json file in json_file_path into an .h ready to be included in C code.

   :param intl_dir_path: Path to the intl/<core_name> directory.
   :param file_list: Iterator of os.DirEntry objects. Contains localisation files to convert.
   :return: None
   """

   p = cor.p_masked

   def update(s_messages, s_template, s_source_messages, file_name):
      translation = ''
      template_messages = p.finditer(s_template)
      for tp_msg in template_messages:
         old_key = tp_msg.group(1)
         if old_key in s_messages and s_messages[old_key] != s_source_messages[old_key]:
            tl_msg_val = s_messages[old_key]
            tl_msg_val = tl_msg_val.replace('"', '\\\"').replace('\n', '')  # escape
            translation = ''.join((translation, '#define ', old_key, file_name.upper(), f' "{tl_msg_val}"\n'))

         else:  # Remove English duplicates and non-translatable strings
            translation = ''.join((translation, '#define ', old_key, file_name.upper(), ' NULL\n'))
      return translation

   us_h = os.path.join(intl_dir_path, '_us.h')
   us_json = os.path.join(intl_dir_path, '_us.json')

   with open(us_h, 'r', encoding='utf-8') as template_file:
      template = template_file.read()
   with open(us_json, 'r+', encoding='utf-8') as source_json_file:
      source_messages = json.load(source_json_file)

   for file in file_list:
      if file.name.lower().startswith('_us') \
         or file.name.lower().endswith('.h') \
         or file.is_dir():
         continue

      with open(file.path, 'r+', encoding='utf-8') as json_file:
         messages = json.load(json_file)
         new_translation = update(messages, template, source_messages, os.path.splitext(file.name)[0])
      with open(os.path.splitext(file.path)[0] + '.h', 'w', encoding='utf-8') as h_file:
         h_file.seek(0)
         h_file.write(new_translation)
         h_file.truncate()
   return


def get_crowdin_client(dir_path: str) -> str:
   """Makes sure the Crowdin CLI client is present. If it isn't, it is fetched & extracted.

   :return: The path to 'crowdin-cli.jar'.
   """
   jar_name = 'crowdin-cli.jar'
   jar_path = os.path.join(dir_path, jar_name)

   if not os.path.isfile(jar_path):
      print('Downloading crowdin-cli.jar')
      crowdin_cli_file = os.path.join(dir_path, 'crowdin-cli.zip')
      crowdin_cli_url = 'https://downloads.crowdin.com/cli/v3/crowdin-cli.zip'
      req.urlretrieve(crowdin_cli_url, crowdin_cli_file)
      import zipfile
      with zipfile.ZipFile(crowdin_cli_file, 'r') as zip_ref:
         jar_dir = zip_ref.namelist()[0]
         for file in zip_ref.namelist():
            if file.endswith(jar_name):
               jar_file = file
               break
         zip_ref.extract(jar_file)
         os.rename(jar_file, jar_path)
         os.remove(crowdin_cli_file)
         shutil.rmtree(jar_dir)
   return jar_path


def create_intl_file(intl_file_path: str, localisations_path: str, text: str, file_path: str) -> None:
   """Creates 'libretro_core_options_intl.h' from Crowdin translations.

   :param intl_file_path: Path to 'libretro_core_options_intl.h'
   :param localisations_path: Path to the intl/<core_name> directory.
   :param text: Content of the 'libretro_core_options.h' being translated.
   :param file_path: Path to the '_us.h' file, containing the original English texts.
   :return: None
   """
   msg_dict = {}
   lang_up = ''

   def replace_pair(pair_match):
      """Replaces a key-value-pair of an option with the macros corresponding to the language.

      :param pair_match: The re match object representing the key-value-pair block.
      :return: Replacement string.
      """
      offset = pair_match.start(0)
      if pair_match.group(1):  # key
         if pair_match.group(2) in msg_dict:  # value
            val = msg_dict[pair_match.group(2)] + lang_up
         elif pair_match.group(1) in msg_dict:  # use key if value not viable (e.g. NULL)
            val = msg_dict[pair_match.group(1)] + lang_up
         else:
            return pair_match.group(0)
      else:
         return pair_match.group(0)
      res = pair_match.group(0)[:pair_match.start(2) - offset] + val \
            + pair_match.group(0)[pair_match.end(2) - offset:]
      return res

   def replace_info(info_match):
      """Replaces the 'additional strings' of an option with the macros corresponding to the language.

      :param info_match: The re match object representing the 'additional strings' block.
      :return: Replacement string.
      """
      offset = info_match.start(0)
      if info_match.group(1) in msg_dict:
         res = info_match.group(0)[:info_match.start(1) - offset] + \
               msg_dict[info_match.group(1)] + lang_up + \
               info_match.group(0)[info_match.end(1) - offset:]
         return res
      else:
         return info_match.group(0)

   def replace_option(option_match):
      """Replaces strings within an option
      '{ "opt_key", "label", "additional strings", ..., { {"key", "value"}, ... }, ... }'
      within a struct with the macros corresponding to the language:
      '{ "opt_key", MACRO_LABEL, MACRO_STRINGS, ..., { {"key", MACRO_VALUE}, ... }, ... }'

      :param option_match: The re match object representing the option.
      :return: Replacement string.
      """
      # label
      offset = option_match.start(0)
      if option_match.group(2):
         res = option_match.group(0)[:option_match.start(2) - offset] + msg_dict[option_match.group(2)] + lang_up
      else:
         return option_match.group(0)
      # additional block
      if option_match.group(3):
         res = res + option_match.group(0)[option_match.end(2) - offset:option_match.start(3) - offset]
         new_info = p.sub(replace_info, option_match.group(3))
         res = res + new_info
      else:
         return res + option_match.group(0)[option_match.end(2) - offset:]
      # key-value-pairs
      if option_match.group(4):
         res = res + option_match.group(0)[option_match.end(3) - offset:option_match.start(4) - offset]
         new_pairs = cor.p_key_value.sub(replace_pair, option_match.group(4))
         res = res + new_pairs + option_match.group(0)[option_match.end(4) - offset:]
      else:
         res = res + option_match.group(0)[option_match.end(3) - offset:]

      return res

   # ------------------------------------------------------------------------------------

   with open(file_path, 'r+', encoding='utf-8') as template:  # intl/<core_name>/_us.h
      masked_msgs = cor.p_masked.finditer(template.read())

   for msg in masked_msgs:
      msg_dict[msg.group(2)] = msg.group(1)

   # top of the file - in case there is no file to copy it from
   out_txt = "﻿#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__\n" \
             "#define LIBRETRO_CORE_OPTIONS_INTL_H__\n\n" \
             "#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)\n" \
             "/* https://support.microsoft.com/en-us/kb/980263 */\n" \
             '#pragma execution_character_set("utf-8")\n' \
             "#pragma warning(disable:4566)\n" \
             "#endif\n\n" \
             "#include <libretro.h>\n\n" \
             '#ifdef __cplusplus\n' \
             'extern "C" {\n' \
             '#endif\n'

   if os.path.isfile(intl_file_path):
      # copy top of the file for re-use
      with open(intl_file_path, 'r', encoding='utf-8') as intl:  # libretro_core_options_intl.h
         in_text = intl.read()
         # attempt 1: find the distinct comment header
         intl_start = re.search(re.escape('/*\n'
                                          ' ********************************\n'
                                          ' * Core Option Definitions\n'
                                          ' ********************************\n'
                                          '*/\n'), in_text)
         if intl_start:
            out_txt = in_text[:intl_start.end(0)]
         else:
            # attempt 2: if no comment header present, find c++ compiler instruction (it is kind of a must)
            intl_start = re.search(re.escape('#ifdef __cplusplus\n'
                                             'extern "C" {\n'
                                             '#endif\n'), in_text)
            if intl_start:
               out_txt = in_text[:intl_start.end(0)]
            # if all attempts fail, use default from above

   # only write to file, if there is anything worthwhile to write!
   overwrite = False

   # iterate through localisation files
   files = {}
   for file in os.scandir(localisations_path):
      files[file.name] = {'is_file': file.is_file(), 'path': file.path}

   for file in sorted(files):  # intl/<core_name>/_*
      if files[file]['is_file'] \
         and file.startswith('_') \
         and file.endswith('.h') \
         and not file.startswith('_us'):
         translation_path = files[file]['path']  # <core_name>_<lang>.h
         # all structs: group(0) full struct, group(1) beginning, group(2) content
         struct_groups = cor.p_struct.finditer(text)
         lang_low = os.path.splitext(file)[0].lower()
         lang_up = lang_low.upper()
         # mark each language's section with a comment, for readability
         out_txt = out_txt + f'/* RETRO_LANGUAGE{lang_up} */\n\n'  # /* RETRO_LANGUAGE_NM */

         # copy adjusted translations (makros)
         with open(translation_path, 'r+', encoding='utf-8') as f_in:  # <core name>.h
            out_txt = out_txt + f_in.read() + '\n'
         # replace English texts with makros
         for construct in struct_groups:
            declaration = construct.group(1)
            struct_type_name = get_struct_type_name(declaration)
            if 3 > len(struct_type_name):  # no language specifier
               new_decl = re.sub(re.escape(struct_type_name[1]), struct_type_name[1] + lang_low, declaration)
            else:
               if '_us' != struct_type_name[2]:
                  # only use _us constructs - other languages present in the source file are not important
                  continue
               new_decl = re.sub(re.escape(struct_type_name[2]), lang_low, declaration)

            p = (cor.p_info_cat if 'retro_core_option_v2_category' == struct_type_name[0] else cor.p_info)
            offset_construct = construct.start(0)
            # append localised construct name and ' = {'
            start = construct.end(1) - offset_construct
            end = construct.start(4) - offset_construct
            out_txt = out_txt + new_decl + construct.group(0)[start:end]
            # insert macros
            content = construct.group(4)
            new_content = cor.p_option.sub(replace_option, content)
            start = construct.end(4) - offset_construct
            # append macro-filled content and close the construct
            out_txt = out_txt + new_content + construct.group(0)[start:] + '\n'

            # for v2
            if 'retro_core_option_v2_definition' == struct_type_name[0]:
               out_txt = out_txt + f'struct retro_core_options_v2 options{lang_low}' \
                                   ' = {\n' \
                                   f'   option_cats{lang_low},\n' \
                                   f'   option_defs{lang_low}\n' \
                                   '};\n\n'
         # if it got this far, we've got something to write
         overwrite = True

   # only write to file, if there is anything worthwhile to write!
   if overwrite:
      with open(intl_file_path, 'w', encoding='utf-8') as intl:
         intl.write(out_txt + '\n#ifdef __cplusplus\n'
                              '}\n#endif\n'
                              '\n#endif')
   return


# --------------------          MAIN          -------------------- #

if __name__ == '__main__':
   try:
      if os.path.isfile(sys.argv[1]) or sys.argv[1].endswith('.h'):
         _temp = os.path.dirname(sys.argv[1])
      else:
         _temp = sys.argv[1]
      while _temp.endswith('/') or _temp.endswith('\\'):
         _temp = _temp[:-1]
      TARGET_DIR_PATH = _temp
   except IndexError:
      TARGET_DIR_PATH = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
      print("No path provided, assuming parent directory:\n" + TARGET_DIR_PATH)

   CORE_NAME = clean_file_name(sys.argv[2])

   DIR_PATH = os.path.dirname(os.path.realpath(__file__))
   H_FILE_PATH = os.path.join(TARGET_DIR_PATH, 'libretro_core_options.h')
   INTL_FILE_PATH = os.path.join(TARGET_DIR_PATH, 'libretro_core_options_intl.h')

   print('Getting texts from libretro_core_options.h')
   with open(H_FILE_PATH, 'r+', encoding='utf-8') as _h_file:
      _main_text = _h_file.read()
   _hash_n_str = get_texts(_main_text)
   _files = create_msg_hash(DIR_PATH, CORE_NAME, _hash_n_str)
   _source_jsons = h2json(_files)

   print('Getting texts from libretro_core_options_intl.h')
   if os.path.isfile(INTL_FILE_PATH):
      with open(INTL_FILE_PATH, 'r+', encoding='utf-8') as _intl_file:
         _intl_text = _intl_file.read()
         _hash_n_str_intl = get_texts(_intl_text)
         _intl_files = create_msg_hash(DIR_PATH, CORE_NAME, _hash_n_str_intl)
         _intl_jsons = h2json(_intl_files)

   print('\nAll done!')
