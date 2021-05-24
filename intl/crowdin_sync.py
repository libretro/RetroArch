#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile
from datetime import date

# Check Crowdin API Key
if len(sys.argv) < 2:
    print('Please provide Crowdin API Key!')
    exit()

api_key = sys.argv[1]

dir_path = os.path.dirname(os.path.realpath(__file__))

# git constants
upstream = 'upstream'
branch_name = 'translationsupdatewithscript'
ra_url = 'https://github.com/libretro/RetroArch.git'

# save current branch & changes
current = subprocess.run(['git', 'branch', '--show-current'], cwd=dir_path, capture_output=True).stdout
current = current[:-1].decode('UTF-8')
subprocess.run(['git', 'stash'], cwd=dir_path)

# check upstream url
b_url = subprocess.run(['git', 'remote', 'get-url', upstream], cwd=dir_path, capture_output=True).stdout
url = b_url[:-1].decode('UTF-8')
# create upstream or change its url to match libretro/RetroArch/master
if url != ra_url:
    if url == "fatal: No such remote 'upstream'" or url == '':
        subprocess.run(['git', 'remote', 'add', upstream, ra_url], cwd=dir_path)
        url = ra_url
    else:
        subprocess.run(['git', 'remote', 'set-url', upstream, ra_url, url], cwd=dir_path)

b_tmp = subprocess.run(['git', 'fetch', upstream], cwd=dir_path, capture_output=True).stdout

# create new clean branch
c_tmp = subprocess.run(['git', 'branch', branch_name, 'upstream/master'], cwd=dir_path, capture_output=True).stdout
tmp = b_tmp[:-1].decode('UTF-8')

# check if branch name is taken
if tmp == "fatal: A branch named '" + branch_name + "' already exists." or tmp == '':
    i = 0
    while True:
        tmp_name = branch_name + str(i)
        b_tmp = subprocess.run(['git', 'branch', tmp_name, 'upstream/master'], cwd=dir_path, capture_output=True).stdout
        tmp = b_tmp[:-1].decode('UTF-8')
        if tmp != "fatal: A branch named '" + tmp_name + "' already exists." and tmp != '':
            break
        i = i + 1
    branch_name = tmp_name

b_tmp = subprocess.run(['git', 'checkout', branch_name], cwd=dir_path, capture_output=True).stdout

# Apply Crowdin API Key
yaml_path = dir_path + '/crowdin.yaml'
crowdin_config_file = open(yaml_path, 'r')
crowdin_config = crowdin_config_file.read()
crowdin_config_file.close()
crowdin_config = crowdin_config.replace('_secret_', api_key)
crowdin_config_file = open(yaml_path, 'w')
crowdin_config_file.write(crowdin_config)
crowdin_config_file.close()

# Download Crowdin CLI

jar_name = 'crowdin-cli.jar'
jar_path = dir_path + '/' + jar_name

if not os.path.isfile(jar_path):
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
        os.replace(jar_file, jar_path)
        os.remove(crowdin_cli_file)
        shutil.rmtree(jar_dir)

print('convert *.h to *.json')
for item in os.listdir(dir_path):
    if item.endswith(".h"):
        subprocess.run(['python3', 'h2json.py', item], cwd=dir_path)

print('upload source *.json')
subprocess.run(['java', '-jar', 'crowdin-cli.jar', 'upload', 'sources'], cwd=dir_path)

print('wait for crowdin server to process data')
time.sleep(10)

print('download translation *.json')
subprocess.run(['java', '-jar', 'crowdin-cli.jar', 'download', 'translations'], cwd=dir_path)

print('convert *.json to *.h')
for file in os.listdir(dir_path):
    if file.startswith('msg_hash_') and file.endswith('.json'):
        print(file)
        subprocess.run(['python3', 'json2h.py', file], cwd=dir_path)

print('fetch translation progress')
subprocess.run(['python3', 'fetch_progress.py'], cwd=dir_path)

# Reset Crowdin API Key
crowdin_config_file = open(yaml_path, 'r')
crowdin_config = crowdin_config_file.read()
crowdin_config_file.close()
crowdin_config = crowdin_config.replace(api_key, '_secret_')
crowdin_config_file = open(yaml_path, 'w')
crowdin_config_file.write(crowdin_config)
crowdin_config_file.close()

# commit changes
parent_path = os.path.dirname(dir_path)

b_tmp = subprocess.run(['git', 'add', 'intl/*'], cwd=parent_path, capture_output=True).stdout
today = date.today().strftime('%d-%b-%Y')
subprocess.run(['git', 'commit', '-m', "'Fetch translations from Crowdin " + today + "'"], cwd=dir_path)
# pushing
while True:
    u_in = input('Would you like to push to origin? (y/n)\n')
    u_in = str(u_in).lower()
    if u_in == 'y' or u_in == 'yes':
        subprocess.run(['git', 'push', '--set-upstream', 'origin', branch_name], cwd=dir_path)
        break
    elif u_in == 'n' or u_in == 'no':
        break
    else:
        print('Please provide a valid input.')

# restore previous state
b_tmp = subprocess.run(['git', 'checkout', current], cwd=dir_path, capture_output=True).stdout
c_tmp = subprocess.run(['git', 'remote', 'set-url', upstream, url, ra_url], cwd=dir_path, capture_output=True).stdout
d_tmp = subprocess.run(['git', 'fetch', upstream], cwd=dir_path, capture_output=True).stdout
e_tmp = subprocess.run(['git', 'stash', 'apply'], cwd=dir_path, capture_output=True).stdout
# clean up
while True:
    u_in = input('Would you like to clean up (delete automatically created branch\n'
                 + branch_name + ')? (y/n)\n')
    u_in = str(u_in).lower()
    if u_in == 'y' or u_in == 'yes':
        subprocess.run(['git', 'branch', '-D', branch_name], cwd=dir_path, capture_output=True).stdout
        print('Process finished.')
        break
    elif u_in == 'n' or u_in == 'no':
        print('Process finished.')
        break
    else:
        print('Please provide a valid input.')
