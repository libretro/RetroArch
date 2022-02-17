#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

# Check Crowdin API Key

try:
   api_key = sys.argv[1]  # IndexError, if no key is given
   if not api_key:   # if key is empty
      raise ValueError
except IndexError:
    print('Please provide Crowdin API Key!')
    raise
except ValueError:
    print("Crowdin API Key can't be empty!")
    raise

# Apply Crowdin API Key
crowdin_config_file = open('crowdin.yaml', 'r')
crowdin_config = crowdin_config_file.read()
crowdin_config_file.close()
crowdin_config = crowdin_config.replace('_secret_', api_key)
crowdin_config_file = open('crowdin.yaml', 'w')
crowdin_config_file.write(crowdin_config)
crowdin_config_file.close()

try:  # catch any exception after crowdin.yaml was changed
    # Download Crowdin CLI
    dir_path = os.path.dirname(os.path.realpath(__file__))

    jar_name = 'crowdin-cli.jar'

    if not os.path.isfile(jar_name):
       print('download crowdin-cli.jar')
       crowdin_cli_file = 'crowdin-cli.zip'
       crowdin_cli_url = 'https://downloads.crowdin.com/cli/v3/' + crowdin_cli_file
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
    subprocess.run(['java', '-jar', 'crowdin-cli.jar', 'download'])

    print('convert *.json to *.h')
    for file in os.listdir(dir_path):
        if file.startswith('msg_hash_') and file.endswith('.json'):
            print(file)
            subprocess.run(['python3', 'json2h.py', file])

    print('fetch translation progress')
    subprocess.run(['python3', 'fetch_progress.py'])

    # Reset Crowdin API Key
    crowdin_config_file = open('crowdin.yaml', 'r')
    crowdin_config = crowdin_config_file.read()
    crowdin_config_file.close()
    crowdin_config = crowdin_config.replace(api_key, '_secret_')
    crowdin_config_file = open('crowdin.yaml', 'w')
    crowdin_config_file.write(crowdin_config)
    crowdin_config_file.close()
except:
    # Reset Crowdin API Key no matter what
    crowdin_config_file = open('crowdin.yaml', 'r')
    crowdin_config = crowdin_config_file.read()
    crowdin_config_file.close()
    crowdin_config = crowdin_config.replace(api_key, '_secret_')
    crowdin_config_file = open('crowdin.yaml', 'w')
    crowdin_config_file.write(crowdin_config)
    crowdin_config_file.close()
    raise
