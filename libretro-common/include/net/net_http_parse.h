/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_http.h).
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

#ifndef _LIBRETRO_SDK_NET_HTTP_PARSE_H
#define _LIBRETRO_SDK_NET_HTTP_PARSE_H

#include <stdint.h>
#include <boolean.h>
#include <string.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

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
 * @return 0 if URL was parsed completely, otherwise 1.
 **/
int string_parse_html_anchor(const char *line, char *link, char *name,
      size_t link_size, size_t name_size);

RETRO_END_DECLS

#endif
