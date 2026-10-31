#!/usr/bin/env python3
"""Exercise production menu allocation checks before any driver callbacks."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
s = (root/'menu/menu_driver.c').read_text()
functions = []
for name, tail in [('bool menu_entries_append(', 'return true;'),
                   ('void menu_entries_prepend(', 'return;')]:
    start = s.index(name)
    end = s.index('   if (mlist && mlist->size)', start)
    functions.append(s[start:end] + '\n   continued++;\n   ' + tail + '\n}\n')
program = r'''
#include <assert.h>
#include <stdio.h>
#include <boolean.h>
#include <stddef.h>
typedef int menu_ctx_list_t;
typedef int menu_file_list_cbs_t;
typedef int rarch_setting_t;
typedef struct { unsigned size; } file_list_t;
enum msg_hash_enums { MSG_UNKNOWN };
struct menu_state { struct { void *list; } entries; };
static struct menu_state menu_driver_state;
#define MENU_LIST_GET(a,b) NULL
static int menu_cbs_pool, value;
static unsigned live, fail_cbs, fail_list, continued;
static menu_file_list_cbs_t *menu_cbs_alloc(void)
{ if (fail_cbs) return NULL; live++; return &value; }
static void mempool_free(int *pool, void *p)
{ (void)pool; (void)p; assert(live); live--; }
static bool file_list_append(file_list_t *list, const char *p, const char *l,
      unsigned t, size_t d, size_t e)
{
   (void)p; (void)l; (void)t; (void)d; (void)e;
   if (fail_list) return false;
   list->size++; return true;
}
static bool file_list_insert(file_list_t *list, const char *p, const char *l,
      unsigned t, size_t d, size_t e, size_t i)
{ (void)i; return file_list_append(list,p,l,t,d,e); }
''' + '\n'.join(functions) + r'''
int main(void)
{
   file_list_t list = {1};
   unsigned prepend, failure;
   for (prepend = 0; prepend < 2; prepend++)
      for (failure = 0; failure < 2; failure++)
      {
         fail_cbs = failure == 0;
         fail_list = failure == 1;
         continued = live = 0;
         if (prepend) menu_entries_prepend(&list,"p","l",MSG_UNKNOWN,0,0,0);
         else assert(!menu_entries_append(&list,"p","l",MSG_UNKNOWN,0,0,0,NULL));
         assert(!continued && !live && list.size == 1);
      }
   puts("PASS: menu append/prepend reject callback and list allocation failures before driver callbacks");
   return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='menu-entry-oom-') as tmp:
    tmp = Path(tmp)
    src = tmp/'test.c'
    src.write_text(program)
    exe = tmp/'menu_test.exe'
    subprocess.run(shlex.split(os.environ.get('CC','cc')) + [
        '-std=c89','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-unused-parameter',
        '-I'+str(root/'libretro-common/include'), str(src), '-o', str(exe)], check=True)
    subprocess.run(shlex.split(os.environ.get('RUNNER','')) + [str(exe)], check=True)
