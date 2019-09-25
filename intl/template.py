#!/usr/bin/env python3

# Apply template (us) updates to translations (fr, ja, chs, etc.)
# Usage: ./template.py xx
# xx is the language code postfix of translation files

import re
import sys

try:
    lc = sys.argv[1]
except:
    print("Usage: ./template.py <language_postfix>")
    sys.exit(1)

p = re.compile('MSG_HASH\(\s*[A-Z0-9_]+,\s*\".*\"\s*\)')

header = """#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif
"""

def parse_message(message):
    key_start = message.find('(') + 1
    key_end = message.find(',')
    key = message[key_start:key_end].strip()
    value_start = message.find('"') + 1
    value_end = message.rfind('"')
    value = message[value_start:value_end].strip()
    return key, value

def messages(text):
    result = p.findall(text)
    msg_list = []
    for msg in result:
        key, val = parse_message(msg)
        item = {'key': key, 'val': val, 'msg': msg}
        msg_list.append(item)
    return msg_list

def update(translation, template):
    new_translation = header + template
    template_messages = messages(template)
    translation_messages = messages(translation)
    for tp_msg in template_messages:
        for ts_msg in translation_messages:
            if tp_msg['key'] == ts_msg['key']:
                new_translation = new_translation.replace(tp_msg['msg'], ts_msg['msg'])
    return new_translation

with open('msg_hash_us.h', 'r') as template_file:
    template = template_file.read()
    with open('msg_hash_' + lc + '.h', 'r+') as translation_file:
        translation = translation_file.read()
        new_translation = update(translation, template)
        translation_file.seek(0)
        translation_file.write(new_translation)
        translation_file.truncate()
