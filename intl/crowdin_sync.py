#!/usr/bin/env python3

import os
import shutil
import subprocess
import time
import urllib.request
import zipfile

dir_path = os.path.dirname(os.path.realpath(__file__))

jar_name = 'crowdin-cli.jar'

if not os.path.isfile(jar_name):
   print('download crowdin-cli.jar')
   crowdin_cli_file = 'crowdin-cli.zip'
   crowdin_cli_url = 'https://downloads.crowdin.com/cli/v2/' + crowdin_cli_file
   urllib.request.urlretrieve(crowdin_cli_url, crowdin_cli_file)
   with zipfile.ZipFile(crowdin_cli_file, 'r') as zip_ref:
      jar_dir = zip_ref.namelist()[0]
      for file in zip_ref.namelist():
         if file.endswith(jar_name):
               jar_file = file
      zip_ref.extract(jar_file)
      os.rename(jar_file, jar_name)
      os.remove(crowdin_cli_file)
      shutil.rmtree(jar_dir)

print('convert *.h to *.json')
for item in os.listdir(dir_path):
    if item.endswith(".h"):
        subprocess.run(['python3', 'h2json.py', item])

print('upload source *.json')
subprocess.run(['java', '-jar', 'crowdin-cli.jar', 'upload', 'sources'])

print('wait for crowdin server to process data')
time.sleep(10)

print('download translation *.json')
subprocess.run(['java', '-jar', 'crowdin-cli.jar', 'download', 'translations'])

print('convert *.json to *.h')
for file in os.listdir(dir_path):
    if file.startswith('msg_hash_') and file.endswith('.json'):
        print(file)
        subprocess.run(['python3', 'json2h.py', file])

print('fetch translation progress')
subprocess.run(['python3', 'fetch_progress.py'])
