#!/usr/bin/env python3
"""Exercise shipping menu-driver selectors with and without the native host."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[3]


def function(source, name):
    start = source.index('static ', source.rfind('\n}', 0, source.index(name)) + 2)
    body = source.index('{', source.index(name, start))
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


settings = (root / 'menu/menu_setting.c').read_text()
frontend = (root / 'retroarch.c').read_text()
start = frontend.index('         for (i = 0; menu_ctx_drivers[i]; i++)',
                       frontend.index('case STRING_LIST_MENU_DRIVERS:'))
options = frontend[start:frontend.index('         break;', start)]
selectors = '\n'.join(function(settings, name) for name in [
    'setting_is_protected_driver(', 'setting_skip_driver(',
    'setting_string_action_left_driver(', 'setting_string_action_right_driver('])
program = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "menu/menu_setting.h"
#include "menu/menu_driver.h"
#include "configuration.h"
#include "driver.h"
#include <string/stdstring.h>
#include <compat/strl.h>

static settings_t settings;
static bool available;
static menu_ctx_driver_t drivers[6];
const menu_ctx_driver_t *menu_ctx_drivers[] = {
   &drivers[0], &drivers[1], &drivers[2], &drivers[3], &drivers[4], &drivers[5], NULL
};
static const char *const names[] = { "glui", "ozone", "rgui", "xmb", "ios", "null" };
bool ios_menu_bridge_available(void) { return available; }
settings_t *config_get_ptr(void) { return &settings; }
bool driver_ctl(enum driver_ctl_state state, void *data)
{
   driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
   int index;
   for (index = 0; index < 6 && strcmp(names[index], drv->s); index++) { }
   switch (state)
   {
      case RARCH_DRIVER_CTL_FIND_FIRST: index = 0; break;
      case RARCH_DRIVER_CTL_FIND_LAST: index = 5; break;
      case RARCH_DRIVER_CTL_FIND_PREV:
         if (index <= 0 || index >= 6) return false;
         index--;
         break;
      case RARCH_DRIVER_CTL_FIND_NEXT:
         if (index >= 5) return false;
         index++;
         break;
      default: assert(0); return false;
   }
   strlcpy(drv->s, names[index], drv->len);
   return true;
}

bool string_list_append(struct string_list *list, const char *value,
      union string_list_elem_attr attr)
{
   assert(list->size < 6);
   list->elems[list->size].data = (char*)value;
   list->elems[list->size++].attr = attr;
   return true;
}
''' + selectors + r'''
static void collect_options(struct string_list *s, size_t *len)
{
   unsigned i;
   union string_list_elem_attr attr;
   attr.i = 0;
   *len = 0;
''' + options + r'''
}
int main(void)
{
   rarch_setting_t setting;
   setting_actions_t actions;
   struct string_list list;
   struct string_list_elem elems[6];
   char selected[16];
   unsigned mode, wrap, direction, index, count;
   size_t len;
   memset(&setting, 0, sizeof(setting));
   memset(&actions, 0, sizeof(actions));
   memset(&list, 0, sizeof(list));
   setting.enum_idx = MENU_ENUM_LABEL_MENU_DRIVER;
   setting.value.target.string = selected;
   setting.size = sizeof(selected);
   setting.actions = &actions;
   list.elems = elems;
   for (index = 0; index < 6; index++) drivers[index].ident = names[index];
   for (mode = 0; mode < 2; mode++)
   {
      available = mode != 0;
      count = available ? 5 : 4;
      list.size = 0;
      collect_options(&list, &len);
      assert(list.size == count && len > 0);
      for (index = 0; index < count; index++)
         assert(!strcmp(list.elems[index].data, names[index]));
      for (wrap = 0; wrap < 2; wrap++)
         for (direction = 0; direction < 2; direction++)
            for (index = 0; index < count; index++)
            {
               unsigned expected = index;
               settings.bools.menu_navigation_wraparound_enable = wrap != 0;
               strcpy(selected, names[index]);
               if (direction)
               {
                  expected = index + 1 < count ? index + 1 : wrap ? 0 : index;
                  setting_string_action_right_driver(&setting, 0, false);
               }
               else
               {
                  expected = index > 0 ? index - 1 : wrap ? count - 1 : 1;
                  setting_string_action_left_driver(&setting, 0, false);
               }
               if (strcmp(selected, names[expected]))
                  fprintf(stderr, "available=%u wrap=%u right=%u index=%u got=%s expected=%s\n",
                        mode, wrap, direction, index, selected, names[expected]);
               assert(!strcmp(selected, names[expected]));
            }
   }
   puts("PASS: native menu availability, dropdowns and driver cycling");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='ios-menu-availability-') as tmp:
    source = Path(tmp) / 'availability.c'
    exe = Path(tmp) / 'availability_test.exe'
    source.write_text(program)
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c89', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
        '-DHAVE_MENU', '-DHAVE_IOS_MENU', '-I' + str(root),
        '-I' + str(root / 'libretro-common/include'), str(source),
        str(root / 'libretro-common/compat/compat_strl.c'), '-o', str(exe)] +
        shlex.split(os.environ.get('CFLAGS', '')), check=True)
    if '--build-only' not in sys.argv:
        subprocess.run(shlex.split(os.environ.get('RUNNER', '')) + [str(exe)], check=True)
