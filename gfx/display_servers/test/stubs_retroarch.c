/* Copyright  (C) 2010-2025 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stubs_retroarch.c).
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

/* The only RetroArch-side symbols dispserv_x11.c refers to.  Keeping
 * the list this short is what makes the file testable in isolation; if
 * it grows, that is worth noticing. */

#include <string.h>
#include <stddef.h>
#include <X11/Xlib.h>
Display *g_x11_dpy = NULL;
Window   g_x11_win = 0;
size_t strlcpy_retro__(char *d, const char *s, size_t n)
{ size_t sl = strlen(s); if (n) { size_t c = sl < n-1 ? sl : n-1; memcpy(d,s,c); d[c]='\0'; } return sl; }
void video_monitor_set_refresh_rate(float hz) { (void)hz; }
