/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_http_parse.c).
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
#include <compat/strcasestr.h>

/**
 * string_parse_html_anchor:
 * @line               : Buffer where the <a> tag is stored
 * @link               : Buffer to store the link URL in
 * @name               : Buffer to store the link URL in
 * @link_size          : Size of the link buffer including the NUL-terminator
 * @name_size          : Size of the name buffer including the NUL-terminator
 *
 * Parses an HTML anchor link stored in @line in the form of: <a href="/path/to/url">Title</a>
 * The buffer pointed to by @link is filled with the URL path the link points to,
 * and @name is filled with the title portion of the link.
 *
 * Returns: 0 if URL was parsed completely, otherwise 1.
 **/
int string_parse_html_anchor(const char *line, char *link, char *name,
      size_t link_size, size_t name_size)
{
   if (!line || !link || !name)
      return 1;

   memset(link, 0, link_size);
   memset(name, 0, name_size);

   line = strcasestr(line, "<a href=\"");

   if (!line)
      return 1;

   line += 9;

   if (line && *line)
   {
      if (!*link)
      {
         const char *end = strstr(line, "\"");

         if (!end)
            return 1;

         memcpy(link, line, end - line);

         *(link + (end - line)) = '\0';
         line += end - line;
      }

      if (!*name)
      {
         const char *start = strstr(line, "\">");
         const char *end   = start ? strstr(start, "</a>") : NULL;

         if (!start || !end)
            return 1;

         memcpy(name, start + 2, end - start - 2);

         *(name + (end - start - 2)) = '\0';
      }
   }

   return 0;
}

#ifndef RARCH_INTERNAL
int main(int argc, char *argv[])
{
   char link[1024];
   char name[1024];
   const char *line  = "<a href=\"http://www.test.com/somefile.zip\">Test</a>\n";

   link[0] = name[0] = '\0';

   string_parse_html_anchor(line, link, name, sizeof(link), sizeof(name));

   printf("link: %s\nname: %s\n", link, name);

   return 1;
}
#endif
