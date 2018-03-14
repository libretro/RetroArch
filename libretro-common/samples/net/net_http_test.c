/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_http_test.c).
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

#include <stdio.h>
#include <net/net_http.h>
#include <net/net_compat.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

int main(void)
{
   char   *data;
   struct http_t *http1, *http3;
   size_t len, pos = 0, tot = 0;

   if (!network_init())
      return -1;

   http1 = net_http_new("http://buildbot.libretro.com/nightly/windows/x86_64/latest/mednafen_psx_libretro.dll.zip");

   while (!net_http_update(http1, &pos, &tot))
      printf("%.9lu / %.9lu        \r",pos,tot);

   http3 = net_http_new("http://www.wikipedia.org/");
   while (!net_http_update(http3, NULL, NULL)) {}

   data  = (char*)net_http_data(http3, &len, false);

   printf("%.*s\n", (int)256, data);

   net_http_delete(http1);
   net_http_delete(http3);

   network_deinit();

   return 0;
}
