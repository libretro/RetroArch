#!/usr/bin/env python3

import requests

import yaml

with open("crowdin.yaml", 'r') as config_file:
   config = yaml.safe_load(config_file)
   headers = { 'Authorization': 'Bearer ' + config['api_token']}

   url1 = ('https://api.crowdin.com/api/v2/projects/' + config['project_id'] +
           '/files/' + config['main_file_id'] + '/languages/progress?limit=100')

   res1 = requests.get(url1, headers=headers)
   output = ''
   for lang in res1.json()['data']:
      lang_id = lang['data']['languageId']
      url2 = 'https://api.crowdin.com/api/v2/languages/' + lang_id
      res2 =  requests.get(url2, headers=headers)
      lang_name = res2.json()['data']['name']
   
      output += '/* ' + lang_name + ' */\n'
      replacements = lang_name.maketrans(' ', '_', ',()')
      escaped_name = lang_name.translate(replacements).upper()
      output += '#define LANGUAGE_PROGRESS_' + escaped_name + '_TRANSLATED ' + str(lang['data']['translationProgress']) + '\n'
      output += '#define LANGUAGE_PROGRESS_' + escaped_name + '_APPROVED   ' + str(lang['data']['approvalProgress']) + '\n\n'
   with open("progress.h", 'w') as output_file:
      output_file.write(output)
