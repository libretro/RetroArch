import re

# 0: full struct; 1: up to & including first []; 2: content between first {}
p_struct = re.compile(r'(struct\s*[a-zA-Z0-9_\s]+\[])\s*'
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+)\s*)*'
                      r'=\s*'  # =
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+)\s*)*'
                      r'{((?:.|[\r\n])*?)\{\s*NULL,\s*NULL,\s*NULL\s*(?:.|[\r\n])*?},?(?:.|[\r\n])*?};')  # captures full struct, it's beginning and it's content
# 0: type name[]; 1: type; 2: name
p_type_name = re.compile(r'(retro_core_option_[a-zA-Z0-9_]+)\s*'
                         r'(option_cats([a-z_]{0,8})|option_defs([a-z_]{0,8}))\s*\[]')
# 0: full option; 1: key; 2: description; 3: additional info; 4: key/value pairs
p_option = re.compile(r'{\s*'  # opening braces
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r'(\".*?\"|'  # key start; group 1
                      r'[a-zA-Z0-9_]+\s*\((?:.|[\r\n])*?\)|'
                      r'[a-zA-Z0-9_]+\s*\[(?:.|[\r\n])*?]|'
                      r'[a-zA-Z0-9_]+\s*\".*?\")\s*'  # key end
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r',\s*'  # comma
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r'(\".*?\")\s*'  # description; group 2
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r',\s*'  # comma
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r'((?:'  # group 3
                      r'(?:NULL|\"(?:.|[\r\n])*?\")\s*'  # description in category, info, info in category, category
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r',?\s*'  # comma
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r')+)'
                      r'(?:'  # defs only start
                      r'{\s*'  # opening braces
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r'((?:'  # key/value pairs start; group 4
                      r'{\s*'  # opening braces
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r'(?:NULL|\".*?\")\s*'  # option key
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r',\s*'  # comma
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r'(?:NULL|\".*?\")\s*'  # option value
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r'}\s*'  # closing braces
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r',?\s*'  # comma
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r')*)'  # key/value pairs end
                      r'}\s*'  # closing braces
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r',?\s*'  # comma
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r'(?:'  # defaults start
                      r'(?:NULL|\".*?\")\s*'  # default value
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r',?\s*'  # comma
                      r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                      r')*'  # defaults end
                      r')?'  # defs only end
                      r'},')  # closing braces
# analyse option group 3
p_info = re.compile(r'(NULL|\"(?:.|[\r\n])*?\")\s*'  # description in category, info, info in category, category
                    r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                    r',')
p_info_cat = re.compile(r'(NULL|\"(?:.|[\r\n])*?\")')
# analyse option group 4
p_key_value = re.compile(r'{\s*'  # opening braces
                         r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                         r'(NULL|\".*?\")\s*'  # option key; 1
                         r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                         r',\s*'  # comma
                         r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                         r'(NULL|\".*?\")\s*'  # option value; 2
                         r'(?:(?:\/\*(?:.|[\r\n])*?\*\/|\/\/.*[\r\n]+|#.*[\r\n]+)\s*)*'
                         r'}')

p_masked = re.compile(r'([A-Z_][A-Z0-9_]+)\s*(\"(?:"\s*"|\\\s*|.)*\")')

p_intl = re.compile(r'(struct retro_core_option_definition \*option_defs_intl\[RETRO_LANGUAGE_LAST]) = {'
                    r'((?:.|[\r\n])*?)};')
p_set = re.compile(r'static INLINE void libretro_set_core_options\(retro_environment_t environ_cb\)'
                   r'(?:.|[\r\n])*?};?\s*#ifdef __cplusplus\s*}\s*#endif')

p_yaml = re.compile(r'"project_id": "[0-9]+".*\s*'
                    r'"api_token": "([a-zA-Z0-9]+)".*\s*'
                    r'"base_path": "\./intl".*\s*'
                    r'"base_url": "https://api\.crowdin\.com".*\s*'
                    r'"preserve_hierarchy": true.*\s*'
                    r'"files": \[\s*'
                    r'\{\s*'
                    r'"source": "/_us/\*\.json",.*\s*'
                    r'"translation": "/_%two_letters_code%/%original_file_name%",.*\s*'
                    r'"skip_untranslated_strings": true.*\s*'
                    r'},\s*'
                    r']')
