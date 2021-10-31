#!/usr/bin/env python3

import requests

import yaml

with open("crowdin.yaml", 'r') as config_file:
   config = yaml.safe_load(config_file)
   headers = { 'Authorization': 'Bearer ' + config['api_token']}
   url = 
   r = requests.get('https://api.crowdin.com/api/project/' + config['project_identifier'] + '/status?key=' + config['api_key'] + '&json', headers=headers)
   output = ''
   for lang in r.json():
      output += '/* ' + lang['name'] + ' */\n'
      escaped_name = lang['name'].replace(', ', '_').replace(' ', '_').upper()
      output += '#define LANGUAGE_PROGRESS_' + escaped_name + '_TRANSLATED ' + str(lang['translated_progress']) + '\n'
      output += '#define LANGUAGE_PROGRESS_' + escaped_name + '_APPROVED   ' + str(lang['approved_progress']) + '\n\n'
   with open("progress.h", 'w') as output_file:
      output_file.write(output)
