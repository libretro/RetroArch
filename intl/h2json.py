#!/usr/bin/env python3

# Convert *.h to *.json
# Usage: ./h2json.py msg_has_us.h

import re
import sys
import json

try:
    h_filename = sys.argv[1]
    json_filename = h_filename.replace('.h', '.json')
except IndexError:
    print("Usage: ./h2json.py msg_has_us.h")
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


try:
    with open(h_filename, 'r+') as h_file:
        text = h_file.read()
        result = p.findall(text)
        seen = set()
        messages = {}
        for msg in result:
            key, val = parse_message(msg)
            messages[key] = val
            if key not in seen:
                seen.add(key)
            else:
                print("Duplicate key: " + key)
        with open(json_filename, 'w') as json_file:
            json.dump(messages, json_file, indent=2)
except EnvironmentError:
    print('Cannot read/write ' + h_filename)
