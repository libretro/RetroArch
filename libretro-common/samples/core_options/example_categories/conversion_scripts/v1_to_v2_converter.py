#!/usr/bin/env python3

"""Core options v1 to v2 converter

Just copy 'libretro_core_options.h' & 'libretro_core_options_intl.h' into the same folder as this script
and run it! The original files will be preserved as *.v1
"""
import core_option_regex as cor
import os
import sys

if os.name == 'nt':
    joiner = '\\'
else:
    joiner = '/'
dir_path = os.path.dirname(os.path.realpath(__file__))
h_filename = joiner.join((dir_path, 'libretro_core_options.h'))
intl_filename = joiner.join((dir_path, 'libretro_core_options_intl.h'))


def create_v2_code_file(struct_text, file_name):
    def replace_option(option_match):
        _offset = option_match.start(0)

        if option_match.group(3):
            res = option_match.group(0)[:option_match.end(2) - _offset] + ',\n      NULL' + \
                  option_match.group(0)[option_match.end(2) - _offset:option_match.end(3) - _offset] + \
                  'NULL,\n      NULL,\n      ' + option_match.group(0)[option_match.end(3) - _offset:]
        else:
            return option_match.group(0)

        return res

    comment_v1 = '/*\n' \
                 ' ********************************\n' \
                 ' * VERSION: 1.3\n' \
                 ' ********************************\n' \
                 ' *\n' \
                 ' * - 1.3: Move translations to libretro_core_options_intl.h\n' \
                 ' *        - libretro_core_options_intl.h includes BOM and utf-8\n' \
                 ' *          fix for MSVC 2010-2013\n' \
                 ' *        - Added HAVE_NO_LANGEXTRA flag to disable translations\n' \
                 ' *          on platforms/compilers without BOM support\n' \
                 ' * - 1.2: Use core options v1 interface when\n' \
                 ' *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1\n' \
                 ' *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)\n' \
                 ' * - 1.1: Support generation of core options v0 retro_core_option_value\n' \
                 ' *        arrays containing options with a single value\n' \
                 ' * - 1.0: First commit\n' \
                 '*/\n'

    comment_v2 = '/*\n' \
                 ' ********************************\n' \
                 ' * VERSION: 2.0\n' \
                 ' ********************************\n' \
                 ' *\n' \
                 ' * - 2.0: Add support for core options v2 interface\n' \
                 ' * - 1.3: Move translations to libretro_core_options_intl.h\n' \
                 ' *        - libretro_core_options_intl.h includes BOM and utf-8\n' \
                 ' *          fix for MSVC 2010-2013\n' \
                 ' *        - Added HAVE_NO_LANGEXTRA flag to disable translations\n' \
                 ' *          on platforms/compilers without BOM support\n' \
                 ' * - 1.2: Use core options v1 interface when\n' \
                 ' *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1\n' \
                 ' *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)\n' \
                 ' * - 1.1: Support generation of core options v0 retro_core_option_value\n' \
                 ' *        arrays containing options with a single value\n' \
                 ' * - 1.0: First commit\n' \
                 '*/\n'

    p_intl = cor.re.compile(r'(struct retro_core_option_definition \*option_defs_intl\[RETRO_LANGUAGE_LAST]) = {'
                            r'((?:.|[\r\n])*?)};')
    p_set = cor.re.compile(r'static INLINE void libretro_set_core_options\(retro_environment_t environ_cb\)'
                           r'(?:.|[\r\n])*?};?\s*#ifdef __cplusplus\s*}\s*#endif')
    new_set = 'static INLINE void libretro_set_core_options(retro_environment_t environ_cb,\n' \
              '      bool *categories_supported)\n' \
              '{\n' \
              '   unsigned version  = 0;\n' \
              '#ifndef HAVE_NO_LANGEXTRA\n' \
              '   unsigned language = 0;\n' \
              '#endif\n' \
              '\n' \
              '   if (!environ_cb || !categories_supported)\n' \
              '      return;\n' \
              '\n' \
              '   *categories_supported = false;\n' \
              '\n' \
              '   if (!environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version))\n' \
              '      version = 0;\n' \
              '\n' \
              '   if (version >= 2)\n' \
              '   {\n' \
              '#ifndef HAVE_NO_LANGEXTRA\n' \
              '      struct retro_core_options_v2_intl core_options_intl;\n' \
              '\n' \
              '      core_options_intl.us    = &options_us;\n' \
              '      core_options_intl.local = NULL;\n' \
              '\n' \
              '      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&\n' \
              '          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))\n' \
              '         core_options_intl.local = options_intl[language];\n' \
              '\n' \
              '      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL,\n' \
              '            &core_options_intl);\n' \
              '#else\n' \
              '      *categories_supported = environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,\n' \
              '            &options_us);\n' \
              '#endif\n' \
              '   }\n' \
              '   else\n' \
              '   {\n' \
              '      size_t i, j;\n' \
              '      size_t option_index              = 0;\n' \
              '      size_t num_options               = 0;\n' \
              '      struct retro_core_option_definition\n' \
              '            *option_v1_defs_us         = NULL;\n' \
              '#ifndef HAVE_NO_LANGEXTRA\n' \
              '      size_t num_options_intl          = 0;\n' \
              '      struct retro_core_option_v2_definition\n' \
              '            *option_defs_intl          = NULL;\n' \
              '      struct retro_core_option_definition\n' \
              '            *option_v1_defs_intl       = NULL;\n' \
              '      struct retro_core_options_intl\n' \
              '            core_options_v1_intl;\n' \
              '#endif\n' \
              '      struct retro_variable *variables = NULL;\n' \
              '      char **values_buf                = NULL;\n' \
              '\n' \
              '      /* Determine total number of options */\n' \
              '      while (true)\n' \
              '      {\n' \
              '         if (option_defs_us[num_options].key)\n' \
              '            num_options++;\n' \
              '         else\n' \
              '            break;\n' \
              '      }\n' \
              '\n' \
              '      if (version >= 1)\n' \
              '      {\n' \
              '         /* Allocate US array */\n' \
              '         option_v1_defs_us = (struct retro_core_option_definition *)\n' \
              '               calloc(num_options + 1, sizeof(struct retro_core_option_definition));\n' \
              '\n' \
              '         /* Copy parameters from option_defs_us array */\n' \
              '         for (i = 0; i < num_options; i++)\n' \
              '         {\n' \
              '            struct retro_core_option_v2_definition *option_def_us = &option_defs_us[i];\n' \
              '            struct retro_core_option_value *option_values         = option_def_us->values;\n' \
              '            struct retro_core_option_definition *option_v1_def_us = &option_v1_defs_us[i];\n' \
              '            struct retro_core_option_value *option_v1_values      = option_v1_def_us->values;\n' \
              '\n' \
              '            option_v1_def_us->key           = option_def_us->key;\n' \
              '            option_v1_def_us->desc          = option_def_us->desc;\n' \
              '            option_v1_def_us->info          = option_def_us->info;\n' \
              '            option_v1_def_us->default_value = option_def_us->default_value;\n' \
              '\n' \
              '            /* Values must be copied individually... */\n' \
              '            while (option_values->value)\n' \
              '            {\n' \
              '               option_v1_values->value = option_values->value;\n' \
              '               option_v1_values->label = option_values->label;\n' \
              '\n' \
              '               option_values++;\n' \
              '               option_v1_values++;\n' \
              '            }\n' \
              '         }\n' \
              '\n' \
              '#ifndef HAVE_NO_LANGEXTRA\n' \
              '         if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&\n' \
              '             (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH) &&\n' \
              '             options_intl[language])\n' \
              '            option_defs_intl = options_intl[language]->definitions;\n' \
              '\n' \
              '         if (option_defs_intl)\n' \
              '         {\n' \
              '            /* Determine number of intl options */\n' \
              '            while (true)\n' \
              '            {\n' \
              '               if (option_defs_intl[num_options_intl].key)\n' \
              '                  num_options_intl++;\n' \
              '               else\n' \
              '                  break;\n' \
              '            }\n' \
              '\n' \
              '            /* Allocate intl array */\n' \
              '            option_v1_defs_intl = (struct retro_core_option_definition *)\n' \
              '                  calloc(num_options_intl + 1, sizeof(struct retro_core_option_definition));\n' \
              '\n' \
              '            /* Copy parameters from option_defs_intl array */\n' \
              '            for (i = 0; i < num_options_intl; i++)\n' \
              '            {\n' \
              '               struct retro_core_option_v2_definition *option_def_intl = &option_defs_intl[i];\n' \
              '               struct retro_core_option_value *option_values           = option_def_intl->values;\n' \
              '               struct retro_core_option_definition *option_v1_def_intl = &option_v1_defs_intl[i];\n' \
              '               struct retro_core_option_value *option_v1_values        = option_v1_def_intl->values;\n' \
              '\n' \
              '               option_v1_def_intl->key           = option_def_intl->key;\n' \
              '               option_v1_def_intl->desc          = option_def_intl->desc;\n' \
              '               option_v1_def_intl->info          = option_def_intl->info;\n' \
              '               option_v1_def_intl->default_value = option_def_intl->default_value;\n' \
              '\n' \
              '               /* Values must be copied individually... */\n' \
              '               while (option_values->value)\n' \
              '               {\n' \
              '                  option_v1_values->value = option_values->value;\n' \
              '                  option_v1_values->label = option_values->label;\n' \
              '\n' \
              '                  option_values++;\n' \
              '                  option_v1_values++;\n' \
              '               }\n' \
              '            }\n' \
              '         }\n' \
              '\n' \
              '         core_options_v1_intl.us    = option_v1_defs_us;\n' \
              '         core_options_v1_intl.local = option_v1_defs_intl;\n' \
              '\n' \
              '         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_v1_intl);\n' \
              '#else\n' \
              '         environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, option_v1_defs_us);\n' \
              '#endif\n' \
              '      }\n' \
              '      else\n' \
              '      {\n' \
              '         /* Allocate arrays */\n' \
              '         variables  = (struct retro_variable *)calloc(num_options + 1,\n' \
              '               sizeof(struct retro_variable));\n' \
              '         values_buf = (char **)calloc(num_options, sizeof(char *));\n' \
              '\n' \
              '         if (!variables || !values_buf)\n' \
              '            goto error;\n' \
              '\n' \
              '         /* Copy parameters from option_defs_us array */\n' \
              '         for (i = 0; i < num_options; i++)\n' \
              '         {\n' \
              '            const char *key                        = option_defs_us[i].key;\n' \
              '            const char *desc                       = option_defs_us[i].desc;\n' \
              '            const char *default_value              = option_defs_us[i].default_value;\n' \
              '            struct retro_core_option_value *values = option_defs_us[i].values;\n' \
              '            size_t buf_len                         = 3;\n' \
              '            size_t default_index                   = 0;\n' \
              '\n' \
              '            values_buf[i] = NULL;\n' \
              '\n' \
              '            if (desc)\n' \
              '            {\n' \
              '               size_t num_values = 0;\n' \
              '\n' \
              '               /* Determine number of values */\n' \
              '               while (true)\n' \
              '               {\n' \
              '                  if (values[num_values].value)\n' \
              '                  {\n' \
              '                     /* Check if this is the default value */\n' \
              '                     if (default_value)\n' \
              '                        if (strcmp(values[num_values].value, default_value) == 0)\n' \
              '                           default_index = num_values;\n' \
              '\n' \
              '                     buf_len += strlen(values[num_values].value);\n' \
              '                     num_values++;\n' \
              '                  }\n' \
              '                  else\n' \
              '                     break;\n' \
              '               }\n' \
              '\n' \
              '               /* Build values string */\n' \
              '               if (num_values > 0)\n' \
              '               {\n' \
              '                  buf_len += num_values - 1;\n' \
              '                  buf_len += strlen(desc);\n' \
              '\n' \
              '                  values_buf[i] = (char *)calloc(buf_len, sizeof(char));\n' \
              '                  if (!values_buf[i])\n' \
              '                     goto error;\n' \
              '\n' \
              '                  strcpy(values_buf[i], desc);\n' \
              '                  strcat(values_buf[i], "; ");\n' \
              '\n' \
              '                  /* Default value goes first */\n' \
              '                  strcat(values_buf[i], values[default_index].value);\n' \
              '\n' \
              '                  /* Add remaining values */\n' \
              '                  for (j = 0; j < num_values; j++)\n' \
              '                  {\n' \
              '                     if (j != default_index)\n' \
              '                     {\n' \
              '                        strcat(values_buf[i], "|");\n' \
              '                        strcat(values_buf[i], values[j].value);\n' \
              '                     }\n' \
              '                  }\n' \
              '               }\n' \
              '            }\n' \
              '\n' \
              '            variables[option_index].key   = key;\n' \
              '            variables[option_index].value = values_buf[i];\n' \
              '            option_index++;\n' \
              '         }\n' \
              '\n' \
              '         /* Set variables */\n' \
              '         environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);\n' \
              '      }\n' \
              '\n' \
              'error:\n' \
              '      /* Clean up */\n' \
              '\n' \
              '      if (option_v1_defs_us)\n' \
              '      {\n' \
              '         free(option_v1_defs_us);\n' \
              '         option_v1_defs_us = NULL;\n' \
              '      }\n' \
              '\n' \
              '#ifndef HAVE_NO_LANGEXTRA\n' \
              '      if (option_v1_defs_intl)\n' \
              '      {\n' \
              '         free(option_v1_defs_intl);\n' \
              '         option_v1_defs_intl = NULL;\n' \
              '      }\n' \
              '#endif\n' \
              '\n' \
              '      if (values_buf)\n' \
              '      {\n' \
              '         for (i = 0; i < num_options; i++)\n' \
              '         {\n' \
              '            if (values_buf[i])\n' \
              '            {\n' \
              '               free(values_buf[i]);\n' \
              '               values_buf[i] = NULL;\n' \
              '            }\n' \
              '         }\n' \
              '\n' \
              '         free(values_buf);\n' \
              '         values_buf = NULL;\n' \
              '      }\n' \
              '\n' \
              '      if (variables)\n' \
              '      {\n' \
              '         free(variables);\n' \
              '         variables = NULL;\n' \
              '      }\n' \
              '   }\n' \
              '}\n' \
              '\n' \
              '#ifdef __cplusplus\n' \
              '}\n' \
              '#endif'

    struct_groups = cor.p_struct.finditer(struct_text)
    out_text = struct_text

    for construct in struct_groups:
        repl_text = ''
        declaration = construct.group(1)
        struct_match = cor.p_type_name.search(declaration)
        if struct_match:
            struct_type_name = struct_match.group(1, 2)
        else:
            return -1

        if 'retro_core_option_definition' == struct_type_name[0]:
            import shutil
            shutil.copy(file_name, file_name + '.v1')
            new_declaration = f'\nstruct retro_core_option_v2_category option_cats_{struct_match.group(3)}[] = ' \
                              '{\n   { NULL, NULL, NULL },\n' \
                              '};\n\n' \
                              + declaration[:struct_match.start(1)] + \
                              'retro_core_option_v2_definition' \
                              + declaration[struct_match.end(1):]
            offset = construct.start(0)
            repl_text = repl_text + cor.re.sub(cor.re.escape(declaration), new_declaration,
                                               construct.group(0)[:construct.start(2) - offset])
            content = construct.group(2)
            new_content = cor.p_option.sub(replace_option, content)

            repl_text = repl_text + new_content + cor.re.sub(r'{\s*NULL,\s*NULL,\s*NULL,\s*{\{0}},\s*NULL\s*},\s*};',
                                                             '{ NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL },\n};'
                                                             '\n\nstruct retro_core_options_v2 options_' +
                                                             struct_match.group(3) + ' = {\n'
                                                             f'   option_cats_{struct_match.group(3)},\n'
                                                             f'   option_defs_{struct_match.group(3)}\n'
                                                             '};',
                                                             construct.group(0)[construct.end(2) - offset:])
            out_text = cor.re.sub(cor.re.escape(construct.group(0)), repl_text, out_text)
        else:
            return -2
    with open(file_name, 'w', encoding='utf-8') as code_file:
        out_text = cor.re.sub(cor.re.escape(comment_v1), comment_v2, out_text)
        intl = p_intl.search(out_text)
        if intl:
            new_intl = out_text[:intl.start(1)] \
                       + 'struct retro_core_options_v2 *options_intl[RETRO_LANGUAGE_LAST]' \
                       + out_text[intl.end(1):intl.start(2)] + cor.re.sub(r'option_defs_', '&options_', intl.group(2)) \
                       + out_text[intl.end(2):]
            out_text = p_set.sub(new_set, new_intl)
        else:
            out_text = p_set.sub(new_set, out_text)
        code_file.write(out_text)

    return 1


# --------------------          MAIN          -------------------- #

if __name__ == '__main__':
    try:
        for file in (h_filename, intl_filename):
            if os.path.isfile(file):
                with open(file, 'r+', encoding='utf-8') as h_file:
                    text = h_file.read()
                    test = create_v2_code_file(text, file)
                    if -1 > test:
                        print('Your file looks like it already is v2? (' + file + ')')
                        continue
                    if 0 > test:
                        print('An error occurred! Please make sure to use the complete v1 struct! (' + file + ')')
                        continue
            else:
                print(file + ' not found.')
    except EnvironmentError:
        print('Something went wrong with reading or writing files!')
        sys.exit(1)
