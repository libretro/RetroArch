#!/usr/bin/env python3

import core_option_translation as t

if __name__ == '__main__':
   try:
      if t.os.path.isfile(t.sys.argv[1]) or t.sys.argv[1].endswith('.h'):
         _temp = t.os.path.dirname(t.sys.argv[1])
      else:
         _temp = t.sys.argv[1]
      while _temp.endswith('/') or _temp.endswith('\\'):
         _temp = _temp[:-1]
      TARGET_DIR_PATH = _temp
   except IndexError:
      TARGET_DIR_PATH = t.os.path.dirname(t.os.path.dirname(t.os.path.realpath(__file__)))
      print("No path provided, assuming parent directory:\n" + TARGET_DIR_PATH)

   CORE_NAME = t.clean_file_name(t.sys.argv[2])
   DIR_PATH = t.os.path.dirname(t.os.path.realpath(__file__))
   LOCALISATIONS_PATH = t.os.path.join(DIR_PATH, CORE_NAME)
   US_FILE_PATH = t.os.path.join(LOCALISATIONS_PATH, '_us.h')
   H_FILE_PATH = t.os.path.join(TARGET_DIR_PATH, 'libretro_core_options.h')
   INTL_FILE_PATH = t.os.path.join(TARGET_DIR_PATH, 'libretro_core_options_intl.h')

   print('Getting texts from libretro_core_options.h')
   with open(H_FILE_PATH, 'r+', encoding='utf-8') as _h_file:
      _main_text = _h_file.read()
   _hash_n_str = t.get_texts(_main_text)
   _files = t.create_msg_hash(DIR_PATH, CORE_NAME, _hash_n_str)
   _source_jsons = t.h2json(_files)

   print('Converting translations *.json to *.h:')
   localisation_files = t.os.scandir(LOCALISATIONS_PATH)
   t.json2h(LOCALISATIONS_PATH, localisation_files)

   print('Constructing libretro_core_options_intl.h')
   t.create_intl_file(INTL_FILE_PATH, LOCALISATIONS_PATH, _main_text, _files["_us"])

   print('\nAll done!')
