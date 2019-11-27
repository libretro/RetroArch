#!/usr/bin/env python3

# Convert *.json to *.h
# Usage: ./json2h.py msg_hash_fr.json

import re
import sys
import json

try:
    json_filename = sys.argv[1]
    h_filename = json_filename.replace('.json', '.h')
except IndexError:
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


def parse_messages(text):
    result = p.findall(text)
    seen = set()
    msg_list = []
    for msg in result:
        key, val = parse_message(msg)
        item = {'key': key, 'val': val, 'msg': msg}
        msg_list.append(item)
        if key not in seen:
            seen.add(key)
        else:
            print("Duplicate key: " + key)

    return msg_list


def update(messages, template):
    new_translation = header + template
    template_messages = parse_messages(template)
    for tp_msg in template_messages:
        if tp_msg['key'] in messages:
            tp_msg_val = tp_msg['val']
            tl_msg_val = messages[tp_msg['key']]
            old_msg = tp_msg['msg']
            new_msg = old_msg.replace(tp_msg_val, tl_msg_val)
            new_translation = new_translation.replace(old_msg, new_msg)
    return new_translation


with open('msg_hash_us.h', 'r') as template_file:
    template = template_file.read()
    try:
        with open(json_filename, 'r+') as json_file:
            messages = json.load(json_file)
            new_translation = update(messages, template)
            with open(h_filename, 'w') as h_file:
                h_file.seek(0)
                h_file.write(new_translation)
                h_file.truncate()
    except EnvironmentError:
        print('Cannot read/write ' + json_filename)
