/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (menu_str.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>

#include "menu_str.h"

/* Shared, reference-counted strings for the menu drivers' node
 * fullpaths.
 *
 * Every entry of a list is inserted with the same fullpath -- it is the
 * path of the menu stack top, one string for the whole list -- so each
 * driver used to strdup() it once per row: a malloc() and a copy of a
 * full path, tens of thousands of times, for a string none of them ever
 * writes to.
 *
 * The reference is handed back as a pointer to the characters, not to
 * the header, so a holder stores and reads a plain char* and only the
 * two functions below know the block has anything in front of it.  A
 * fullpath must therefore be released with menu_str_unref() and never
 * with free().
 *
 * The one-entry cache is compared by content rather than by source
 * pointer: the caller's string lives in a list entry that can be freed
 * and reallocated at the same address between two builds, which a
 * pointer compare would take for a hit.  A strcmp of a path costs a
 * fraction of the malloc() and copy it avoids.  The cache holds a
 * reference of its own, so the string it points at cannot be freed
 * underneath the comparison. */
struct menu_str
{
   size_t refs;
   char   s[1];
};

#define MENU_STR_HDR offsetof(struct menu_str, s)

static char *menu_str_cached = NULL;

char *menu_str_ref(const char *s)
{
   struct menu_str *r;
   size_t _len;

   if (!s)
      return NULL;

   if (menu_str_cached && string_is_equal(menu_str_cached, s))
   {
      r = (struct menu_str*)(menu_str_cached - MENU_STR_HDR);
      r->refs++;
      return menu_str_cached;
   }

   _len = strlen(s);
   if (!(r = (struct menu_str*)malloc(MENU_STR_HDR + _len + 1)))
      return NULL;
   memcpy(r->s, s, _len + 1);
   /* One reference for the caller and one for the cache below. */
   r->refs = 2;

   if (menu_str_cached)
      menu_str_unref(menu_str_cached);
   menu_str_cached = r->s;

   return r->s;
}

void menu_str_unref(char *s)
{
   struct menu_str *r;

   if (!s)
      return;

   r = (struct menu_str*)(s - MENU_STR_HDR);
   if (--r->refs == 0)
   {
      if (menu_str_cached == s)
         menu_str_cached = NULL;
      free(r);
   }
}

/* Drops the cache's own reference at menu teardown, so the last string
 * does not outlive the drivers that were sharing it. */
void menu_str_cache_flush(void)
{
   if (!menu_str_cached)
      return;
   menu_str_unref(menu_str_cached);
   menu_str_cached = NULL;
}

