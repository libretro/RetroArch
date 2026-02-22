/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MCP_ADAPTER_UTILS_H__
#define MCP_ADAPTER_UTILS_H__

static void mcp_json_escape(char *dst, size_t dst_size, const char *src)
{
   size_t w = 0;
   if (!src)
   {
      if (dst_size > 0)
         dst[0] = '\0';
      return;
   }
   for (; *src && w + 6 < dst_size; src++)
   {
      switch (*src)
      {
         case '"':  dst[w++] = '\\'; dst[w++] = '"';  break;
         case '\\': dst[w++] = '\\'; dst[w++] = '\\'; break;
         case '\n': dst[w++] = '\\'; dst[w++] = 'n';  break;
         case '\r': dst[w++] = '\\'; dst[w++] = 'r';  break;
         case '\t': dst[w++] = '\\'; dst[w++] = 't';  break;
         default:
            if ((unsigned char)*src < 0x20)
               w += snprintf(dst + w, dst_size - w, "\\u%04x",
                     (unsigned)(unsigned char)*src);
            else
               dst[w++] = *src;
            break;
      }
   }
   if (w < dst_size)
      dst[w] = '\0';
}

static void mcp_json_add(char *buf, size_t buf_size,
      const char *key, const char *value)
{
   char escaped[2048];
   char tmp[2048 + 256];
   size_t len = strlen(buf);
   mcp_json_escape(escaped, sizeof(escaped), value);
   snprintf(tmp, sizeof(tmp), "\"%s\":\"%s\"", key, escaped);
   if (len > 1)
      strlcat(buf, ",", buf_size);
   strlcat(buf, tmp, buf_size);
}

#endif /* MCP_ADAPTER_UTILS_H__ */
