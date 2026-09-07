/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <stdint.h>
#include <net/net_http.h>
#include <net/net_compat.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

/* Issue a single HTTP GET and print the first 256 bytes of the
 * response.  Sample usage of the two-stage connection API:
 *
 *    1. net_http_connection_new(url, method, data)  -- handle
 *    2. loop net_http_connection_iterate until connection is up
 *    3. check net_http_connection_done for success
 *    4. net_http_new(conn)  -- transfer handle
 *    5. loop net_http_update until transfer is complete
 *    6. net_http_data -- fetch the response body
 *    7. net_http_delete + net_http_connection_free
 */
static int http_get_and_print(const char *url)
{
   struct http_connection_t *conn;
   struct http_t *http;
   uint8_t *data;
   size_t _len = 0, pos = 0, tot = 0;

   conn = net_http_connection_new(url, "GET", NULL);
   if (!conn)
   {
      fprintf(stderr, "net_http_connection_new failed for %s\n", url);
      return -1;
   }

   /* Drive the connection state machine to completion. */
   while (!net_http_connection_iterate(conn)) {}
   if (!net_http_connection_done(conn))
   {
      fprintf(stderr, "net_http_connection_done failed for %s\n", url);
      net_http_connection_free(conn);
      return -1;
   }

   http = net_http_new(conn);
   if (!http)
   {
      fprintf(stderr, "net_http_new failed for %s\n", url);
      net_http_connection_free(conn);
      return -1;
   }

   while (!net_http_update(http, &pos, &tot))
      printf("%9lu / %9lu    \r", (unsigned long)pos, (unsigned long)tot);
   printf("\n");

   data = net_http_data(http, &_len, false);
   if (data && _len > 0)
      printf("%.*s\n", (int)(_len < 256 ? _len : 256), (const char*)data);

   net_http_delete(http);
   net_http_connection_free(conn);
   return 0;
}

int main(void)
{
   if (!network_init())
      return -1;

   http_get_and_print("http://buildbot.libretro.com/nightly/windows/x86_64/latest/mednafen_psx_libretro.dll.zip");
   http_get_and_print("http://www.wikipedia.org/");

   return 0;
}
