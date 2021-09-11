#!/usr/bin/env python3

import core_opt_translation as t


if __name__ == '__main__':
    _core_name = 'core_options'

    try:
        if t.os.path.isfile(t.sys.argv[1]):
            _temp = t.os.path.dirname(t.sys.argv[1])
        else:
            _temp = t.sys.argv[1]
        while _temp.endswith('/') or _temp.endswith('\\'):
            _temp = _temp[:-1]
        TARGET_DIR_PATH = _temp
    except IndexError:
        TARGET_DIR_PATH = t.os.path.dirname(t.os.path.dirname(t.os.path.realpath(__file__)))
        print("No path provided, assuming parent directory:\n" + TARGET_DIR_PATH)

    DIR_PATH = t.os.path.dirname(t.os.path.realpath(__file__))
    H_FILE_PATH = t.os.path.join(TARGET_DIR_PATH, 'libretro_core_options.h')

    _core_name = t.clean_file_name(_core_name)

    print('Getting texts from libretro_core_options.h')
    with open(H_FILE_PATH, 'r+', encoding='utf-8') as _h_file:
        _main_text = _h_file.read()
    _hash_n_str = t.get_texts(_main_text)
    _files = t.create_msg_hash(DIR_PATH, _core_name, _hash_n_str)

    _source_jsons = t.h2json(_files)

    print('\nAll done!')
