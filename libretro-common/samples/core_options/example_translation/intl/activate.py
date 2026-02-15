#!/usr/bin/env python3

import os
import glob
import random as r

# --------------------          MAIN          -------------------- #

if __name__ == '__main__':
   DIR_PATH = os.path.dirname(os.path.realpath(__file__))
   if os.path.basename(DIR_PATH) != "intl":
      raise RuntimeError("Script is not in intl folder!")

   BASE_PATH = os.path.dirname(DIR_PATH)
   WORKFLOW_PATH = os.path.join(BASE_PATH, ".github", "workflows")
   PREP_WF = os.path.join(WORKFLOW_PATH, "crowdin_prep.yml")
   TRANSLATE_WF = os.path.join(WORKFLOW_PATH, "crowdin_translate.yml")
   CORE_NAME = os.path.basename(BASE_PATH)
   CORE_OP_FILE = os.path.join(BASE_PATH, "**", "libretro_core_options.h")

   core_options_hits = glob.glob(CORE_OP_FILE, recursive=True)

   if len(core_options_hits) == 0:
      raise RuntimeError("libretro_core_options.h not found!")
   elif len(core_options_hits) > 1:
      print("More than one libretro_core_options.h file found:\n\n")
      for i, file in enumerate(core_options_hits):
         print(f"{i} {file}\n")

      while True:
         user_choice = input("Please choose one ('q' will exit): ")
         if user_choice == 'q':
            exit(0)
         elif user_choice.isdigit():
            core_op_file = core_options_hits[int(user_choice)]
            break
         else:
            print("Please make a valid choice!\n\n")
   else:
      core_op_file = core_options_hits[0]

   core_intl_file = os.path.join(os.path.dirname(core_op_file.replace(BASE_PATH, ''))[1:],
                                 'libretro_core_options_intl.h')
   core_op_file   = os.path.join(os.path.dirname(core_op_file.replace(BASE_PATH, ''))[1:],
                                 'libretro_core_options.h')
   minutes = r.randrange(0, 59, 5)
   hour = r.randrange(0, 23)

   with open(PREP_WF, 'r') as wf_file:
      prep_txt = wf_file.read()

   prep_txt = prep_txt.replace("<CORE_NAME>", CORE_NAME)
   prep_txt = prep_txt.replace("<PATH/TO>/libretro_core_options.h",
                               core_op_file)
   with open(PREP_WF, 'w') as wf_file:
      wf_file.write(prep_txt)


   with open(TRANSLATE_WF, 'r') as wf_file:
      translate_txt = wf_file.read()

   translate_txt = translate_txt.replace('<0-59>', f"{minutes}")
   translate_txt = translate_txt.replace('<0-23>', f"{hour}")
   translate_txt = translate_txt.replace('# Fridays at , UTC',
                                         f"# Fridays at {hour%12}:{minutes if minutes > 9 else '0' + str(minutes)} {'AM' if hour < 12 else 'PM'}, UTC")
   translate_txt = translate_txt.replace("<CORE_NAME>", CORE_NAME)
   translate_txt = translate_txt.replace('<PATH/TO>/libretro_core_options_intl.h',
                                         core_intl_file)
   with open(TRANSLATE_WF, 'w') as wf_file:
      wf_file.write(translate_txt)
