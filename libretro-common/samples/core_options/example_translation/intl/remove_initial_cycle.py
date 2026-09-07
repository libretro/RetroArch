#!/usr/bin/env python3

with open('intl/upload_workflow.py', 'r') as workflow:
   workflow_config = workflow.read()

workflow_config = workflow_config.replace(
   "subprocess.run(['python3', 'intl/core_option_translation.py', dir_path, core_name])",
   "subprocess.run(['python3', 'intl/crowdin_prep.py', dir_path, core_name])"
)
workflow_config = workflow_config.replace(
   "subprocess.run(['python3', 'intl/initial_sync.py', api_key, core_name])",
   "subprocess.run(['python3', 'intl/crowdin_source_upload.py', api_key, core_name])"
)
with open('intl/upload_workflow.py', 'w') as workflow:
   workflow.write(workflow_config)


with open('intl/download_workflow.py', 'r') as workflow:
   workflow_config = workflow.read()

workflow_config = workflow_config.replace(
   "subprocess.run(['python3', 'intl/core_option_translation.py', dir_path, core_name])",
   "subprocess.run(['python3', 'intl/crowdin_prep.py', dir_path, core_name])"
)
workflow_config = workflow_config.replace(
   "subprocess.run(['python3', 'intl/initial_sync.py', api_key, core_name])",
   "subprocess.run(['python3', 'intl/crowdin_translation_download.py', api_key, core_name])"
)
with open('intl/download_workflow.py', 'w') as workflow:
   workflow.write(workflow_config)
