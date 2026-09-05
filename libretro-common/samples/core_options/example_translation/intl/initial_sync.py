#!/usr/bin/env python3

import re
import os
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile
import core_option_translation as t

# --------------------          MAIN          -------------------- #

if __name__ == '__main__':
   # Check Crowdin API Token and core name
   try:
      API_KEY = sys.argv[1]
      CORE_NAME = t.clean_file_name(sys.argv[2])
      OPTIONS_PATH = t.clean_file_name(sys.argv[3])
   except IndexError as e:
      print('Please provide Crowdin API Token, core name and path to the core options file!')
      raise e

   DIR_PATH = os.path.dirname(os.path.realpath(__file__))
   YAML_PATH = os.path.join(DIR_PATH, 'crowdin.yaml')

   # Apply Crowdin API Key
   with open(YAML_PATH, 'r') as crowdin_config_file:
      crowdin_config = crowdin_config_file.read()
   crowdin_config = re.sub(r'"api_token": "_secret_"',
                           f'"api_token": "{API_KEY}"',
                           crowdin_config, 1)
   crowdin_config = re.sub(r'/_core_name_(?=[/.])]',
                           f'/{CORE_NAME}'
                           , crowdin_config)
   with open(YAML_PATH, 'w') as crowdin_config_file:
      crowdin_config_file.write(crowdin_config)

   try:
      jar_name = 'crowdin-cli.jar'
      jar_path = os.path.join(DIR_PATH, jar_name)
      crowdin_cli_file = 'crowdin-cli.zip'
      crowdin_cli_url = 'https://downloads.crowdin.com/cli/v3/' + crowdin_cli_file
      crowdin_cli_path = os.path.join(DIR_PATH, crowdin_cli_file)

      # Download Crowdin CLI
      if not os.path.isfile(os.path.join(DIR_PATH, jar_name)):
         print('download crowdin-cli.jar')
         urllib.request.urlretrieve(crowdin_cli_url, crowdin_cli_path)
         with zipfile.ZipFile(crowdin_cli_path, 'r') as zip_ref:
            jar_dir = os.path.join(DIR_PATH, zip_ref.namelist()[0])
            for file in zip_ref.namelist():
               if file.endswith(jar_name):
                  jar_file = file
                  break
            zip_ref.extract(jar_file, path=DIR_PATH)
            os.rename(os.path.join(DIR_PATH, jar_file), jar_path)
            os.remove(crowdin_cli_path)
            shutil.rmtree(jar_dir)

      # Create JSON data
      subprocess.run(['python3', 'intl/core_option_translation.py', OPTIONS_PATH, CORE_NAME])
      print('upload source & translations *.json')
      subprocess.run(['java', '-jar', jar_path, 'upload', 'sources', '--config', YAML_PATH])
      subprocess.run(['java', '-jar', jar_path, 'upload', 'translations', '--config', YAML_PATH])

      print('wait for crowdin server to process data')
      time.sleep(10)

      print('download translation *.json')
      subprocess.run(['java', '-jar', jar_path, 'download', '--config', YAML_PATH])

      # Reset Crowdin API Key
      with open(YAML_PATH, 'r') as crowdin_config_file:
         crowdin_config = crowdin_config_file.read()
      crowdin_config = re.sub(r'"api_token": ".*"', '"api_token": "_secret_"', crowdin_config, 1)

      # TODO This is technically not safe and could replace more than intended.
      crowdin_config = re.sub(r'/' + re.escape(CORE_NAME) + r'(?=[/.])',
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

      # TODO This is technically not safe and could replace more than intended.
      crowdin_config = re.sub(r'/' + re.escape(CORE_NAME) + r'(?=[/.])',
                              '/_core_name_',
                              crowdin_config)

      with open(YAML_PATH, 'w') as crowdin_config_file:
         crowdin_config_file.write(crowdin_config)
      raise e
