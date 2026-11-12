#!/usr/bin/env python3
"""Extract actual recording capabilities, settings rows and navigation actions."""
from pathlib import Path

here = Path(__file__).resolve().parent
root = here.parents[2]
driver = (root / 'record/record_driver.c').read_text(encoding='utf-8')
display = (root / 'menu/menu_displaylist.c').read_text(encoding='utf-8')
setting = (root / 'menu/menu_setting.c').read_text(encoding='utf-8')

def between(text, first, last):
    start = text.index(first)
    return text[start:text.index(last, start)]

caps = between(driver, 'const record_driver_t *recording_driver_find(',
               'static void find_record_driver(')
types = between(display, 'typedef struct menu_displaylist_build_info_selective',
                '} menu_displaylist_build_info_selective_t;')
types += '} menu_displaylist_build_info_selective_t;\n'
rows = between(display, '      case DISPLAYLIST_RECORDING_SETTINGS_LIST:',
               '      case DISPLAYLIST_ACCOUNTS_YOUTUBE_LIST:')
rows = rows.split('\n', 1)[1]
build = ('static unsigned build_rows(void)\n{\n'
         '   settings_t *settings = &test_settings;\n'
         '   void *list = NULL;\n   unsigned i, count = 0;\n'
         '   row_count = 0;\n   switch (0)\n   {\n      case 0:\n'
         + rows + '   }\n   return count;\n}\n')
actions = between(setting, 'static int setting_uint_action_left_default(',
                  'static int setting_bool_action_right_with_refresh(')
changed = between(setting, 'static void record_driver_write_handler(',
                  'static void audio_driver_write_handler(')
start = display.index('                                    if (setting->enum_idx == MENU_ENUM_LABEL_STREAMING_MODE')
end = display.index('continue;', start) + len('continue;')
filter_mode = display[start:end]
dropdown = ('static unsigned dropdown_modes(unsigned *values)\n{\n'
            '   settings_t *settings = &test_settings;\n'
            '   rarch_setting_t row;\n   rarch_setting_t *setting = &row;\n'
            '   unsigned val, count = 0;\n'
            '   memset(&row,0,sizeof(row));\n'
            '   row.enum_idx = MENU_ENUM_LABEL_STREAMING_MODE;\n'
            '   for (val = 0; val <= STREAMING_MODE_CUSTOM; val++)\n   {\n'
            + filter_mode + '\n      values[count++] = val;\n   }\n'
            '   return count;\n}\n')
start = display.index('               if (     settings->bools.quick_menu_show_start_streaming')
condition = display[start:display.index('\n               {', start)]
quick = ('static bool quick_stream_offered(void)\n{\n'
         '   settings_t *settings = &test_settings;\n'
         + condition + '\n      return true;\n   return false;\n}\n')
(here / 'menu_fragment.c').write_text(caps + types + build + actions + changed
                                    + dropdown + quick, encoding='utf-8')
