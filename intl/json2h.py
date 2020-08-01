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

if json_filename == 'msg_hash_us.json' or json_filename == 'msg_hash_lbl.json':
   print("Skip")
   sys.exit(0)

p = re.compile(r'MSG_HASH\(\s*\/?\*?.*\*?\/?\s*[a-zA-Z0-9_]+\s*,\s*\".*\"\s*\)')

def c89_cut(old_str):
   new_str = ''
   byte_count = 0
   for c in old_str:
      byte_count += len(c.encode('utf-8'))
      if byte_count > 500:
         return new_str + '[...]'
      new_str += c
   return new_str

def parse_message(message):
   key_start = max(message.find('(') + 1, message.find('*/') + 2)
   key_end = message.find(',')
   key = message[key_start:key_end].strip()
   value_start = message.find('"') + 1
   value_end = message.rfind('"')
   value = message[value_start:value_end]
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


def update(messages, template, source_messages):
   new_translation = template
   template_messages = parse_messages(template)
   for tp_msg in template_messages:
      old_msg = tp_msg['msg']
      if tp_msg['key'] in messages and messages[tp_msg['key']] != source_messages[tp_msg['key']]:
         tp_msg_val = tp_msg['val']
         tl_msg_val = messages[tp_msg['key']]
         tl_msg_val = tl_msg_val.replace('"', '\\\"').replace('\n', '') # escape
         if tp_msg['key'].find('_QT_') < 0:
            tl_msg_val = c89_cut(tl_msg_val)
         # Replace last match, incase the key contains the value string
         new_msg = old_msg[::-1].replace(tp_msg_val[::-1], tl_msg_val[::-1], 1)[::-1]
         new_translation = new_translation.replace(old_msg, new_msg)
      # Remove English duplicates and non-translateable strings
      else:
         new_translation = new_translation.replace(old_msg + '\n', '')
   return new_translation


with open('msg_hash_us.h', 'r') as template_file:
   template = template_file.read()
   with open('msg_hash_us.json', 'r+', encoding='utf-8') as source_json_file:
      source_messages = json.load(source_json_file)
      with open(json_filename, 'r+', encoding='utf-8') as json_file:
         messages = json.load(json_file)
         new_translation = update(messages, template, source_messages)
         with open(h_filename, 'w', encoding='utf-8') as h_file:
            h_file.seek(0)
            h_file.write(new_translation)
            h_file.truncate()
