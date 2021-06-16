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

if h_filename == 'msg_hash_lbl.h':
    print("Skip")
    sys.exit(0)

p = re.compile(
    r'MSG_HASH\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*\(\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*[a-zA-Z0-9_]+\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*,\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*\".*\"\s*(?:\/\*(?:.|[\r\n])*?\*\/\s*)*\)')


def parse_message(message):
    a = message.find('/*')
    b = message.find('*/')
    c = message.find('"')
    new_msg = message
    while (a >= 0 and b >= 0) and (a < c < b or b < c):
        new_msg = new_msg[:a] + new_msg[b + 2:]
        c = new_msg.find('"', a)
        b = new_msg.find('*/', a)
        a = new_msg.find('/*', a)
    # get key word
    word = new_msg[new_msg.find('(') + 1:new_msg.find(',')].strip()

    a = new_msg.rfind('/*')
    b = new_msg.rfind('*/')
    d = new_msg.rfind('"')
    while (a >= 0 and b >= 0) and (a < d < b or a > d):
        new_msg = new_msg[:a]
        a = new_msg.rfind('/*')
        b = new_msg.rfind('*/')
        d = new_msg.rfind('"')
    # get value
    value = new_msg[c + 1:d]

    return word, value


try:
    with open(h_filename, 'r+', encoding='utf-8') as h_file:
        text = h_file.read()
        result = p.findall(text)
        seen = set()
        messages = {}
        for msg in result:
            key, val = parse_message(msg)
            if not key.startswith('MENU_ENUM_LABEL_VALUE_LANG_') and val:
                messages[key] = val.replace('\\\"', '"')  # unescape
                if key not in seen:
                    seen.add(key)
                else:
                    print("Duplicate key: " + key)
        with open(json_filename, 'w', encoding='utf-8') as json_file:
            json.dump(messages, json_file, indent=2)
except EnvironmentError:
    print('Cannot read/write ' + h_filename)
