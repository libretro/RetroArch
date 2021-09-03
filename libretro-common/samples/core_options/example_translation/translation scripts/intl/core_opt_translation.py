#!/usr/bin/env python3

"""Core options text extractor

The purpose of this script is to set up & provide functions for automatic generation of 'libretro_core_options_intl.h'
from 'libretro_core_options.h' using translations from Crowdin.

Both v1 and v2 structs are supported. It is, however, recommended to convert v1 files to v2 using the included
'v1_to_v2_converter.py'.

Usage:
python3 path/to/core_opt_translation.py "path/to/where/libretro_core_options.h & libretro_core_options_intl.h/are"

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

# for uploading translations to Crowdin, the Crowdin 'language id' is required
LANG_CODE_TO_ID = {'_ar': 'ar',
                   '_ast': 'ast',
                   '_chs': 'zh-CN',
                   '_cht': 'zh-TW',
                   '_cs': 'cs',
                   '_cy': 'cy',
                   '_da': 'da',
                   '_de': 'de',
                   '_el': 'el',
                   '_eo': 'eo',
                   '_es': 'es-ES',
                   '_fa': 'fa',
                   '_fi': 'fi',
                   '_fr': 'fr',
                   '_gl': 'gl',
                   '_he': 'he',
                   '_hu': 'hu',
                   '_id': 'id',
                   '_it': 'it',
                   '_ja': 'ja',
                   '_ko': 'ko',
                   '_nl': 'nl',
                   '_pl': 'pl',
                   '_pt_br': 'pt-BR',
                   '_pt_pt': 'pt-PT',
                   '_ru': 'ru',
                   '_sk': 'sk',
                   '_sv': 'sv-SE',
                   '_tr': 'tr',
                   '_uk': 'uk',
                   '_vn': 'vi'}
LANG_CODE_TO_R_LANG = {'_ar': 'RETRO_LANGUAGE_ARABIC',
                       '_ast': 'RETRO_LANGUAGE_ASTURIAN',
                       '_chs': 'RETRO_LANGUAGE_CHINESE_SIMPLIFIED',
                       '_cht': 'RETRO_LANGUAGE_CHINESE_TRADITIONAL',
                       '_cs': 'RETRO_LANGUAGE_CZECH',
                       '_cy': 'RETRO_LANGUAGE_WELSH',
                       '_da': 'RETRO_LANGUAGE_DANISH',
                       '_de': 'RETRO_LANGUAGE_GERMAN',
                       '_el': 'RETRO_LANGUAGE_GREEK',
                       '_eo': 'RETRO_LANGUAGE_ESPERANTO',
                       '_es': 'RETRO_LANGUAGE_SPANISH',
                       '_fa': 'RETRO_LANGUAGE_PERSIAN',
                       '_fi': 'RETRO_LANGUAGE_FINNISH',
                       '_fr': 'RETRO_LANGUAGE_FRENCH',
                       '_gl': 'RETRO_LANGUAGE_GALICIAN',
                       '_he': 'RETRO_LANGUAGE_HEBREW',
                       '_hu': 'RETRO_LANGUAGE_HUNGARIAN',
                       '_id': 'RETRO_LANGUAGE_INDONESIAN',
                       '_it': 'RETRO_LANGUAGE_ITALIAN',
                       '_ja': 'RETRO_LANGUAGE_JAPANESE',
                       '_ko': 'RETRO_LANGUAGE_KOREAN',
                       '_nl': 'RETRO_LANGUAGE_DUTCH',
                       '_pl': 'RETRO_LANGUAGE_POLISH',
                       '_pt_br': 'RETRO_LANGUAGE_PORTUGUESE_BRAZIL',
                       '_pt_pt': 'RETRO_LANGUAGE_PORTUGUESE_PORTUGAL',
                       '_ru': 'RETRO_LANGUAGE_RUSSIAN',
                       '_sk': 'RETRO_LANGUAGE_SLOVAK',
                       '_sv': 'RETRO_LANGUAGE_SWEDISH',
                       '_tr': 'RETRO_LANGUAGE_TURKISH',
                       '_uk': 'RETRO_LANGUAGE_UKRAINIAN',
                       '_us': 'RETRO_LANGUAGE_ENGLISH',
                       '_vn': 'RETRO_LANGUAGE_VIETNAMESE'}

# these are handled by RetroArch directly - no need to include them in core translations
ON_OFFS = {'"enabled"', '"disabled"', '"true"', '"false"', '"on"', '"off"'}


def remove_special_chars(text: str, char_set=0) -> str:
    """Removes special characters from a text.

    :param text: String to be cleaned.
    :param char_set: 0 -> remove all ASCII special chars except for '_' & 'space';
                     1 -> remove invalid chars from file names
    :return: Clean text.
    """
    command_chars = [chr(unicode) for unicode in tuple(range(0, 32)) + (127,)]
    special_chars = ([chr(unicode) for unicode in tuple(range(33, 48)) + tuple(range(58, 65)) + tuple(range(91, 95))
                      + (96,) + tuple(range(123, 127))],
                     ('\\', '/', ':', '*', '?', '"', '<', '>', '|'))
    res = text
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
    """text must be longer than 2 ('""'), not 'NULL' and text.lower() not in
    {'"enabled"', '"disabled"', '"true"', '"false"', '"on"', '"off"'}.

    :param text: String to be tested.
    :return: bool
    """
    return 2 < len(text) and text != 'NULL' and text.lower() not in ON_OFFS


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

        is_v2 = False
        pre_name = ''
        p = cor.p_info
        if 'retro_core_option_v2_definition' == struct_type_name[0]:
            is_v2 = True
        elif 'retro_core_option_v2_category' == struct_type_name[0]:
            pre_name = 'CATEGORY_'
            p = cor.p_info_cat

        struct_content = struct.group(2)
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
                if is_v2:
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

            # group 4:
            if option.group(4):
                for j, kv_set in enumerate(cor.p_key_value.finditer(option.group(4))):
                    set_key, set_value = kv_set.group(1, 2)
                    if not is_viable_value(set_value):
                        if not is_viable_value(set_key):
                            continue
                        set_value = set_key
                    # re.fullmatch(r'(?:[+-][0-9]+)+', value[1:-1])
                    if set_value not in just_string[lang] and not re.sub(r'[+-]', '', set_value[1:-1]).isdigit():
                        clean_key = set_key.encode('ascii', errors='ignore').decode('unicode-escape')[1:-1]
                        clean_key = remove_special_chars(clean_key).upper().replace(' ', '_')
                        m_h = create_non_dupe(re.sub(r'__+', '_', f"OPTION_VAL_{clean_key}"), opt, hash_n_string[lang])
                        hash_n_string[lang][m_h] = set_value
                        just_string[lang].add(set_value)
    return hash_n_string


def create_msg_hash(intl_dir_path: str, core_name: str, keyword_string_dict: dict) -> dict:
    """Creates '<core_name>.h' files in 'intl/_<lang>/' containing the macro name & string combinations.

    :param intl_dir_path: Path to the intl directory.
    :param core_name: Name of the core, used for naming the files.
    :param keyword_string_dict: Dictionary of the form { '_<lang>': { 'macro': 'string', ... }, ... }.
    :return: Dictionary of the form { '_<lang>': 'path/to/file (./intl/_<lang>/<core_name>.h)', ... }.
    """
    files = {}
    for localisation in keyword_string_dict:
        path = os.path.join(intl_dir_path, localisation)  # intl/_<lang>
        files[localisation] = os.path.join(path, core_name + '.h')  # intl/_<lang>/<core_name>.h
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
        jsons[file_lang] = file_paths[file_lang][:-2] + '.json'

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


def json2h(intl_dir_path: str, json_file_path: str, core_name: str) -> None:
    """Converts .json file in json_file_path into an .h ready to be included in C code.

    :param intl_dir_path: Path to the intl directory.
    :param json_file_path: Base path of translation .json.
    :param core_name: Name of the core, required for naming the files.
    :return: None
    """
    h_filename = os.path.join(json_file_path, core_name + '.h')
    json_filename = os.path.join(json_file_path, core_name + '.json')
    file_lang = os.path.basename(json_file_path).upper()

    if os.path.basename(json_file_path).lower() == '_us':
        print('    skipped')
        return

    p = cor.p_masked

    def update(s_messages, s_template, s_source_messages):
        translation = ''
        template_messages = p.finditer(s_template)
        for tp_msg in template_messages:
            old_key = tp_msg.group(1)
            if old_key in s_messages and s_messages[old_key] != s_source_messages[old_key]:
                tl_msg_val = s_messages[old_key]
                tl_msg_val = tl_msg_val.replace('"', '\\\"').replace('\n', '')  # escape
                translation = ''.join((translation, '#define ', old_key, file_lang, f' "{tl_msg_val}"\n'))

            else:  # Remove English duplicates and non-translatable strings
                translation = ''.join((translation, '#define ', old_key, file_lang, ' NULL\n'))
        return translation

    with open(os.path.join(intl_dir_path, '_us', core_name + '.h'), 'r', encoding='utf-8') as template_file:
        template = template_file.read()
    with open(os.path.join(intl_dir_path, '_us', core_name + '.json'), 'r+', encoding='utf-8') as source_json_file:
        source_messages = json.load(source_json_file)
    with open(json_filename, 'r+', encoding='utf-8') as json_file:
        messages = json.load(json_file)
        new_translation = update(messages, template, source_messages)
    with open(h_filename, 'w', encoding='utf-8') as h_file:
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


def create_intl_file(intl_file_path: str, intl_dir_path: str, text: str, core_name: str, file_path: str) -> None:
    """Creates 'libretro_core_options_intl.h' from Crowdin translations.

    :param intl_file_path: Path to 'libretro_core_options_intl.h'
    :param intl_dir_path: Path to the intl directory.
    :param text: Content of the 'libretro_core_options.h' being translated.
    :param core_name: Name of the core. Needed to identify the files to pull the translations from.
    :param file_path: Path to the '<core name>_us.h' file, containing the original English texts.
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

    with open(file_path, 'r+', encoding='utf-8') as template:  # intl/_us/<core_name>.h
        masked_msgs = cor.p_masked.finditer(template.read())
        for msg in masked_msgs:
            msg_dict[msg.group(2)] = msg.group(1)

    with open(intl_file_path, 'r', encoding='utf-8') as intl:  # libretro_core_options_intl.h
        in_text = intl.read()
        intl_start = re.search(re.escape('/*\n'
                                         ' ********************************\n'
                                         ' * Core Option Definitions\n'
                                         ' ********************************\n'
                                         '*/\n'), in_text)
        if intl_start:
            out_txt = in_text[:intl_start.end(0)]
        else:
            intl_start = re.search(re.escape('#ifdef __cplusplus\n'
                                             'extern "C" {\n'
                                             '#endif\n'), in_text)
            out_txt = in_text[:intl_start.end(0)]

    for folder in os.listdir(intl_dir_path):  # intl/_*
        if os.path.isdir(os.path.join(intl_dir_path, folder)) and folder.startswith('_')\
                and folder != '_us' and folder != '__pycache__':
            translation_path = os.path.join(intl_dir_path, folder, core_name + '.h')  # <core_name>_<lang>.h
            # all structs: group(0) full struct, group(1) beginning, group(2) content
            struct_groups = cor.p_struct.finditer(text)
            lang_up = folder.upper()
            lang_low = folder.lower()
            out_txt = out_txt + f'/* {LANG_CODE_TO_R_LANG[lang_low]} */\n\n'  # /* RETRO_LANGUAGE_NAME */
            with open(translation_path, 'r+', encoding='utf-8') as f_in:  # <core name>.h
                out_txt = out_txt + f_in.read() + '\n'
            for construct in struct_groups:
                declaration = construct.group(1)
                struct_type_name = get_struct_type_name(declaration)
                if 3 > len(struct_type_name):  # no language specifier
                    new_decl = re.sub(re.escape(struct_type_name[1]), struct_type_name[1] + lang_low, declaration)
                else:
                    new_decl = re.sub(re.escape(struct_type_name[2]), lang_low, declaration)
                    if '_us' != struct_type_name[2]:
                        continue

                p = cor.p_info
                if 'retro_core_option_v2_category' == struct_type_name[0]:
                    p = cor.p_info_cat
                offset_construct = construct.start(0)
                start = construct.end(1) - offset_construct
                end = construct.start(2) - offset_construct
                out_txt = out_txt + new_decl + construct.group(0)[start:end]

                content = construct.group(2)
                new_content = cor.p_option.sub(replace_option, content)

                start = construct.end(2) - offset_construct
                out_txt = out_txt + new_content + construct.group(0)[start:] + '\n'

                if 'retro_core_option_v2_definition' == struct_type_name[0]:
                    out_txt = out_txt + f'struct retro_core_options_v2 options{lang_low}' \
                                        ' = {\n' \
                                        f'   option_cats{lang_low},\n' \
                                        f'   option_defs{lang_low}\n' \
                                        '};\n\n'
        #    shutil.rmtree(JOINER.join((intl_dir_path, folder)))

    with open(intl_file_path, 'w', encoding='utf-8') as intl:
        intl.write(out_txt + '\n#ifdef __cplusplus\n'
                             '}\n#endif\n'
                             '\n#endif')
    return


# --------------------          MAIN          -------------------- #

if __name__ == '__main__':
    #
    try:
        if os.path.isfile(sys.argv[1]):
            _temp = os.path.dirname(sys.argv[1])
        else:
            _temp = sys.argv[1]
        while _temp.endswith('/') or _temp.endswith('\\'):
            _temp = _temp[:-1]
        TARGET_DIR_PATH = _temp
    except IndexError:
        TARGET_DIR_PATH = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
        print("No path provided, assuming parent directory:\n" + TARGET_DIR_PATH)

    DIR_PATH = os.path.dirname(os.path.realpath(__file__))
    H_FILE_PATH = os.path.join(TARGET_DIR_PATH, 'libretro_core_options.h')
    INTL_FILE_PATH = os.path.join(TARGET_DIR_PATH, 'libretro_core_options_intl.h')

    _core_name = 'core_options'
    try:
        print('Getting texts from libretro_core_options.h')
        with open(H_FILE_PATH, 'r+', encoding='utf-8') as _h_file:
            _main_text = _h_file.read()
        _hash_n_str = get_texts(_main_text)
        _files = create_msg_hash(DIR_PATH, _core_name, _hash_n_str)
        _source_jsons = h2json(_files)
    except Exception as e:
        print(e)

    print('Getting texts from libretro_core_options_intl.h')
    with open(INTL_FILE_PATH, 'r+', encoding='utf-8') as _intl_file:
        _intl_text = _intl_file.read()
        _hash_n_str_intl = get_texts(_intl_text)
        _intl_files = create_msg_hash(DIR_PATH, _core_name, _hash_n_str_intl)
        _intl_jsons = h2json(_intl_files)

    print('\nAll done!')
