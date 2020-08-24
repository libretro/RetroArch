# Internationalization Workflow

## For Translators

### Use Crowdin

1. Register user account at https://crowdin.com/
2. Join the project https://crowdin.com/project/retroarch/
3. Select your language to translate
4. Click the file name `msg_hash_us.json` and the editor should open
5. Select an untranslated (red) string from the list
6. Type translation and save
7. Next string...

Links:
- [Video: How to use Crowdin](https://www.youtube.com/watch?v=kRMeCCr-D7s)
- [Learn more about contributing](https://support.crowdin.com/for-volunteer-translators/)
- [Learn more about the editor](https://support.crowdin.com/online-editor/)
- [Learn more about conversations](https://support.crowdin.com/conversations/)
- [Learn more about joining project](https://support.crowdin.com/joining-translation-project/)

### Request New Language

You can open a new issue and @guoyunhe to add new language.

## For Maintainers

### Set Up

Install Java, Python3 and Git

### Synchronize

```
cd intl
python3 crowin_sync.py
```

### Manage Crowdin Project

1. You need to be project admin. Please contact @guoyunhe or @twinaphex
2. Go to https://crowdin.com/project/retroarch/settings
3. You can manage languages, members etc. here

Links:
- [Learn more about project management](https://support.crowdin.com/advanced-project-setup/)
- [Learn more about inviting project members](https://support.crowdin.com/inviting-participants/)
- [Learn more about roles of members](https://support.crowdin.com/modifying-project-participants-roles/)

### Message File Format

1. Must **NOT** contain `#else`
2. Must **NOT** have multiple-line string syntax
   ```cpp
   // bad
   MSG_HASH(
     MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
     "Compete to earn custom-made achievements in classic games.\n"
     "For more information, visit http://retroachievements.org"
     )
   // good
   MSG_HASH(
     MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
     "Compete to earn custom-made achievements in classic games.\nFor more information, visit http://retroachievements.org"
     )
   ```
3. Must **NOT** contain lowercase letters in key name
   ```cpp
   // bad (x)
   MSG_HASH(
     MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800x240,
     "2D (High Resolution)"
     )
   // good (X)
   MSG_HASH(
     MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
     "2D (High Resolution)"
     )
   ```

### Add New Languages

1. Go to Crowdin and add the language
2. Run Crowdin script to download new translations
3. Add new language into menu (see [#10787](https://github.com/libretro/RetroArch/pull/10787))

