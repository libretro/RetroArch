#!/usr/bin/env python3

import sys
import subprocess

try:
   api_key = sys.argv[1]
   core_name = sys.argv[2]
   dir_path = sys.argv[3]
except IndexError as e:
   print('Please provide path to libretro_core_options.h, Crowdin API Token and core name!')
   raise e

subprocess.run(['python3', 'intl/crowdin_prep.py', dir_path, core_name])
subprocess.run(['python3', 'intl/crowdin_translation_download.py', api_key, core_name])
subprocess.run(['python3', 'intl/crowdin_translate.py', dir_path, core_name])
