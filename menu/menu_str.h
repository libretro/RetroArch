/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (menu_str.h).
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

#ifndef _MENU_STR_H
#define _MENU_STR_H

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * Take a reference to a shared copy of @s.
 *
 * The menu drivers use this for node fullpaths, which are one string
 * per list repeated across every row.  The result points at the
 * characters, so it is used as an ordinary C string, but it MUST be
 * released with menu_str_unref() rather than free().
 *
 * @return The shared string, or NULL if @s was NULL or on OOM.
 */
char *menu_str_ref(const char *s);

/**
 * Drop one reference taken with menu_str_ref().  NULL is accepted and
 * ignored, matching free().
 */
void menu_str_unref(char *s);

/**
 * Drop the internal one-entry cache's own reference.  Called at menu
 * teardown so the last shared string does not outlive the drivers.
 */
void menu_str_cache_flush(void);

RETRO_END_DECLS

#endif
