#!/usr/bin/env python3

import re
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile
import core_option_translation as t

# --------------------          MAIN          -------------------- #

if __name__ == '__main__':
   # Check Crowdin API Token and core name
   try:
      API_KEY = sys.argv[1]
      CORE_NAME = t.clean_file_name(sys.argv[2])
   except IndexError as e:
      print('Please provide Crowdin API Token and core name!')
      raise e

   DIR_PATH = t.os.path.dirname(t.os.path.realpath(__file__))
   YAML_PATH = t.os.path.join(DIR_PATH, 'crowdin.yaml')

   # Apply Crowdin API Key
   with open(YAML_PATH, 'r') as crowdin_config_file:
      crowdin_config = crowdin_config_file.read()
   crowdin_config = re.sub(r'"api_token": "_secret_"',
                           f'"api_token": "{API_KEY}"',
                           crowdin_config, 1)
   crowdin_config = re.sub(r'/_core_name_',
                           f'/{CORE_NAME}'
                           , crowdin_config)
   with open(YAML_PATH, 'w') as crowdin_config_file:
      crowdin_config_file.write(crowdin_config)

   try:
      # Download Crowdin CLI
      jar_name = 'crowdin-cli.jar'
      jar_path = t.os.path.join(DIR_PATH, jar_name)
      crowdin_cli_file = 'crowdin-cli.zip'
      crowdin_cli_url = 'https://downloads.crowdin.com/cli/v3/' + crowdin_cli_file
      crowdin_cli_path = t.os.path.join(DIR_PATH, crowdin_cli_file)

      if not os.path.isfile(t.os.path.join(DIR_PATH, jar_name)):
         print('download crowdin-cli.jar')
         urllib.request.urlretrieve(crowdin_cli_url, crowdin_cli_path)
         with zipfile.ZipFile(crowdin_cli_path, 'r') as zip_ref:
            jar_dir = t.os.path.join(DIR_PATH, zip_ref.namelist()[0])
            for file in zip_ref.namelist():
               if file.endswith(jar_name):
                  jar_file = file
                  break
            zip_ref.extract(jar_file, path=DIR_PATH)
            os.rename(t.os.path.join(DIR_PATH, jar_file), jar_path)
            os.remove(crowdin_cli_path)
            shutil.rmtree(jar_dir)

      print('upload source *.json')
      subprocess.run(['java', '-jar', jar_path, 'upload', 'sources', '--config', YAML_PATH])

      # Reset Crowdin API Key
      with open(YAML_PATH, 'r') as crowdin_config_file:
         crowdin_config = crowdin_config_file.read()
      crowdin_config = re.sub(r'"api_token": ".*?"',
                              '"api_token": "_secret_"',
                              crowdin_config, 1)

      # TODO this is NOT safe!
      crowdin_config = re.sub(re.escape(f'/{CORE_NAME}'),
                              '/_core_name_',
                              crowdin_config)

      with open(YAML_PATH, 'w') as crowdin_config_file:
         crowdin_config_file.write(crowdin_config)

   except Exception as e:
      # Try really hard to reset Crowdin API Key
      with open(YAML_PATH, 'r') as crowdin_config_file:
         crowdin_config = crowdin_config_file.read()
      crowdin_config = re.sub(r'"api_token": ".*?"',
                              '"api_token": "_secret_"',
                              crowdin_config, 1)

      # TODO this is NOT safe!
      crowdin_config = re.sub(re.escape(f'/{CORE_NAME}'),
                              '/_core_name_',
                              crowdin_config)

      with open(YAML_PATH, 'w') as crowdin_config_file:
         crowdin_config_file.write(crowdin_config)
      raise e
