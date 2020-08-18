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

p = re.compile(r'MSG_HASH\(\s*\/?\*?.*\*?\/?\s*[a-zA-Z0-9_]+\s*,\s*\".*\"\s*\)')

def parse_message(message):
   key_start = max(message.find('(') + 1, message.find('*/') + 2)
   key_end = message.find(',', key_start)
   key = message[key_start:key_end].strip()
   value_start = message.find('"') + 1
   value_end = message.rfind('"')
   value = message[value_start:value_end]
   return key, value


try:
   with open(h_filename, 'r+') as h_file:
      text = h_file.read()
      result = p.findall(text)
      seen = set()
      messages = {}
      for msg in result:
         key, val = parse_message(msg)
         if not key.startswith('MENU_ENUM_LABEL_VALUE_LANG_') and val:
            messages[key] = val.replace('\\\"', '"') # unescape
            if key not in seen:
               seen.add(key)
            else:
               print("Duplicate key: " + key)
      with open(json_filename, 'w') as json_file:
         json.dump(messages, json_file, indent=2)
except EnvironmentError:
   print('Cannot read/write ' + h_filename)
