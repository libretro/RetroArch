/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "containers/core/containers_uri.h"

/*****************************************************************************/
/* Internal types and definitions                                            */
/*****************************************************************************/

typedef struct VC_URI_QUERY_T
{
   char *name;
   char *value;
} VC_URI_QUERY_T;

struct VC_URI_PARTS_T
{
   char *scheme;     /**< Unescaped scheme */
   char *userinfo;   /**< Unescaped userinfo */
   char *host;       /**< Unescaped host name/IP address */
   char *port;       /**< Unescaped port */
   char *path;       /**< Unescaped path */
   char *path_extension; /**< Unescaped path extension */
   char *fragment;   /**< Unescaped fragment */
   VC_URI_QUERY_T *queries;   /**< Array of queries */
   uint32_t num_queries;      /**< Number of queries in array */
};

typedef const uint32_t *RESERVED_CHARS_TABLE_T;

/** Reserved character table for scheme component
 * Controls, space, !"#$%&'()*,/:;<=>?@[\]^`{|} and 0x7F and above reserved. */
static uint32_t scheme_reserved_chars[8] = {
   0xFFFFFFFF, 0xFC0097FF, 0x78000001, 0xB8000001, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

/** Reserved character table for userinfo component
 * Controls, space, "#%/<>?@[\]^`{|} and 0x7F and above reserved. */
static uint32_t userinfo_reserved_chars[8] = {
   0xFFFFFFFF, 0xD000802D, 0x78000001, 0xB8000001, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

/** Reserved character table for host component
 * Controls, space, "#%/<>?@\^`{|} and 0x7F and above reserved. */
static uint32_t host_reserved_chars[8] = {
   0xFFFFFFFF, 0xD000802D, 0x50000001, 0xB8000001, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

/** Reserved character table for port component
 * Controls, space, !"#$%&'()*+,/:;<=>?@[\]^`{|} and 0x7F and above reserved. */
static uint32_t port_reserved_chars[8] = {
   0xFFFFFFFF, 0xFC009FFF, 0x78000001, 0xB8000001, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

/** Reserved character table for path component
 * Controls, space, "#%<>?[\]^`{|} and 0x7F and above reserved. */
static uint32_t path_reserved_chars[8] = {
   0xFFFFFFFF, 0xD000002D, 0x78000000, 0xB8000001, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

/** Reserved character table for query component
 * Controls, space, "#%<>[\]^`{|} and 0x7F and above reserved. */
static uint32_t query_reserved_chars[8] = {
   0xFFFFFFFF, 0x5000002D, 0x78000000, 0xB8000001, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

/** Reserved character table for fragment component
 * Controls, space, "#%<>[\]^`{|} and 0x7F and above reserved. */
static uint32_t fragment_reserved_chars[8] = {
   0xFFFFFFFF, 0x5000002D, 0x78000000, 0xB8000001, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

#define URI_RESERVED(C, TABLE) (!!((TABLE)[(unsigned char)(C) >> 5] & (1 << ((C) & 0x1F))))

#define SCHEME_DELIMITERS     ":/?#"
#define NETWORK_DELIMITERS    "@/?#"
#define HOST_PORT_DELIMITERS  "/?#"
#define PATH_DELIMITERS       "?#"
#define QUERY_DELIMITERS      "#"

/*****************************************************************************/
/* Internal functions                                                        */
/*****************************************************************************/

static char to_hex(int v)
{
   if (v > 9)
      return 'A' + v - 10;
   return '0' + v;
}

/*****************************************************************************/
static uint32_t from_hex(const char *str, uint32_t str_len)
{
   uint32_t val = 0;

   while (str_len--)
   {
      char c = *str++;
      if (c >= '0' && c <= '9')
         c -= '0';
      else if (c >= 'A' && c <= 'F')
         c -= 'A' - 10;
      else if (c >= 'a' && c <= 'f')
         c -= 'a' - 10;
      else
         c = 0;   /* Illegal character (not hex) */
      val = (val << 4) + c;
   }

   return val;
}

/*****************************************************************************/
static uint32_t escaped_length( const char *str, RESERVED_CHARS_TABLE_T reserved )
{
   uint32_t ii;
   uint32_t esclen = 0;
   char c;

   for (ii = strlen(str); ii > 0; ii--)
   {
      c = *str++;
      if (URI_RESERVED(c, reserved))
      {
         /* Reserved character needs escaping as %xx */
         esclen += 3;
      } else {
         esclen++;
      }
   }

   return esclen;
}

/*****************************************************************************/
static uint32_t escape_string( const char *str, char *escaped,
      RESERVED_CHARS_TABLE_T reserved )
{
   uint32_t ii;
   uint32_t esclen = 0;

   if (!str)
      return 0;

   for (ii = strlen(str); ii > 0; ii--)
   {
      char c = *str++;

      if (URI_RESERVED(c, reserved))
      {
         escaped[esclen++] = '%';
         escaped[esclen++] = to_hex((c >> 4) & 0xF);
         escaped[esclen++] = to_hex(c & 0xF);
      } else {
         escaped[esclen++] = c;
      }
   }

   return esclen;
}

/*****************************************************************************/
static uint32_t unescaped_length( const char *str, uint32_t str_len )
{
   uint32_t ii;
   uint32_t unesclen = 0;

   for (ii = 0; ii < str_len; ii++)
   {
      if (*str++ == '%' && (ii + 2) < str_len)
      {
         str += 2;  /* Should be two hex values next */
         ii += 2;
      }
      unesclen++;
   }

   return unesclen;
}

/*****************************************************************************/
static void unescape_string( const char *str, uint32_t str_len, char *unescaped )
{
   uint32_t ii;

   for (ii = 0; ii < str_len; ii++)
   {
      char c = *str++;

      if (c == '%' && (ii + 2) < str_len )
      {
         c = (char)(from_hex(str, 2) & 0xFF);
         str += 2;
         ii += 2;
      }
      *unescaped++ = c;
   }

   *unescaped = '\0';
}

/*****************************************************************************/
static char *create_unescaped_string( const char *escstr, uint32_t esclen )
{
   char *unescstr;

   unescstr = (char *)malloc(unescaped_length(escstr, esclen) + 1);  /* Allow for NUL */
   if (unescstr)
      unescape_string(escstr, esclen, unescstr);

   return unescstr;
}

/*****************************************************************************/
static bool duplicate_string( const char *src, char **p_dst )
{
   if (*p_dst)
      free(*p_dst);

   if (src)
   {
      size_t str_size = strlen(src) + 1;

      *p_dst = (char *)malloc(str_size);
      if (!*p_dst)
         return false;

      memcpy(*p_dst, src, str_size);
   } else
      *p_dst = NULL;

   return true;
}

/*****************************************************************************/
static void release_string( char **str )
{
   if (*str)
   {
      free(*str);
      *str = NULL;
   }
}

/*****************************************************************************/
static void to_lower_string( char *str )
{
   char c;

   while ((c = *str) != '\0')
   {
      if (c >= 'A' && c <= 'Z')
         *str = c - 'A' + 'a';
      str++;
   }
}

/*****************************************************************************/
static const char *vc_uri_find_delimiter(const char *str, const char *delimiters)
{
   const char *ptr = str;
   char c;

   while ((c = *ptr) != 0)
   {
      if (strchr(delimiters, c) != 0)
         break;
      ptr++;
   }

   return ptr;
}

/*****************************************************************************/
static void vc_uri_set_path_extension(VC_URI_PARTS_T *p_uri)
{
   char *end;

   if (!p_uri)
      return;

   p_uri->path_extension = NULL;

   if (!p_uri->path)
      return;

   /* Look for the magic dot */
   for (end = p_uri->path + strlen(p_uri->path); *end != '.'; end--)
      if (end == p_uri->path || *end == '/' || *end == '\\')
         return;

   p_uri->path_extension = end + 1;
}

/*****************************************************************************/
static bool parse_authority( VC_URI_PARTS_T *p_uri, const char *str,
      uint32_t str_len, const char *userinfo_end )
{
   const char *marker = userinfo_end;
   const char *str_end = str + str_len;
   char c;

   if (marker)
   {
      p_uri->userinfo = create_unescaped_string(str, marker - str);
      if (!p_uri->userinfo)
         return false;
      str = marker + 1; /* Past '@' character */
   }

   if (*str == '[')     /* IPvFuture / IPv6 address */
   {
      /* Find end of address marker */
      for (marker = str; marker < str_end; marker++)
      {
         c = *marker;
         if (c == ']')
            break;
      }

      if (marker < str_end)
         marker++;   /* Found marker, move to next character */
   } else {
      /* Find port value marker*/
      for (marker = str; marker < str_end; marker++)
      {
         c = *marker;
         if (c == ':')
            break;
      }
   }

   /* Always store the host, even if empty, to trigger the "://" form of URI */
   p_uri->host = create_unescaped_string(str, marker - str);
   if (!p_uri->host)
      return false;
   to_lower_string(p_uri->host);    /* Host names are case-insensitive */

   if (*marker == ':')
   {
      str = marker + 1;
      p_uri->port = create_unescaped_string(str, str_end - str);
      if (!p_uri->port)
         return false;
   }

   return true;
}

/*****************************************************************************/
static bool store_query( VC_URI_PARTS_T *p_uri, const char *name_start,
      const char *equals_ptr, const char *query_end)
{
   uint32_t name_len, value_len;

   if (equals_ptr)
   {
      name_len = equals_ptr - name_start;
      value_len = query_end - equals_ptr - 1;   /* Don't include '=' itself */
   } else {
      name_len = query_end - name_start;
      value_len = 0;
   }

   /* Only store something if there is a name */
   if (name_len)
   {
      char *name, *value = NULL;
      VC_URI_QUERY_T *p_query;

      if (equals_ptr)
      {
         value = create_unescaped_string(equals_ptr + 1, value_len);
         if (!value)
            return false;
         equals_ptr = query_end;
      }

      name = create_unescaped_string(name_start, name_len);
      if (!name)
      {
         if (value)
            free(value);
         return false;
      }

      /* Store query data in URI structure */
      p_query = &p_uri->queries[ p_uri->num_queries++ ];
      p_query->name = name;
      p_query->value = value;
   }

   return true;
}

/*****************************************************************************/
static bool parse_query( VC_URI_PARTS_T *p_uri, const char *str, uint32_t str_len )
{
   uint32_t ii;
   uint32_t query_count;
   VC_URI_QUERY_T *queries;
   const char *name_start = str;
   const char *equals_ptr = NULL;
   char c;

   if (!str_len)
      return true;

   /* Scan for the number of query items, so array can be allocated the right size */
   query_count = 1;  /* At least */
   for (ii = 0; ii < str_len; ii++)
   {
      c = str[ii];

      if (c == '&' || c ==';')
         query_count++;
   }

   queries = (VC_URI_QUERY_T *)malloc(query_count * sizeof(VC_URI_QUERY_T));
   if (!queries)
      return false;

   p_uri->queries = queries;

   /* Go back and parse the string for each query item and store in array */
   for (ii = 0; ii < str_len; ii++)
   {
      c = *str;

      /* Take first '=' as break between name and value */
      if (c == '=' && !equals_ptr)
         equals_ptr = str;
      
      /* If at the end of the name or name/value pair */
      if (c == '&' || c ==';')
      {
         if (!store_query(p_uri, name_start, equals_ptr, str))
            return false;

         equals_ptr = NULL;
         name_start = str + 1;
      }

      str++;
   }

   return store_query(p_uri, name_start, equals_ptr, str);
}

/*****************************************************************************/
static uint32_t calculate_uri_length(const VC_URI_PARTS_T *p_uri)
{
   uint32_t length = 0;
   uint32_t count;

   /* With no scheme, assume this is a plain path (without escaping) */
   if (!p_uri->scheme)
      return p_uri->path ? strlen(p_uri->path) : 0;

   length += escaped_length(p_uri->scheme, scheme_reserved_chars);
   length++; /* for the colon */

   if (p_uri->host)
   {
      length += escaped_length(p_uri->host, host_reserved_chars) + 2;  /* for the double slash */
      if (p_uri->userinfo)
         length += escaped_length(p_uri->userinfo, userinfo_reserved_chars) + 1; /* for the '@' */
      if (p_uri->port)
         length += escaped_length(p_uri->port, port_reserved_chars) + 1;     /* for the ':' */
   }

   if (p_uri->path)
      length += escaped_length(p_uri->path, path_reserved_chars);

   count = p_uri->num_queries;
   if (count)
   {
      VC_URI_QUERY_T * queries = p_uri->queries;

      while (count--)
      {
         /* The name is preceded by either the '?' or the '&' */
         length += escaped_length(queries->name, query_reserved_chars) + 1;

         /* The value is optional, but if present will require an '=' */
         if (queries->value)
            length += escaped_length(queries->value, query_reserved_chars) + 1;
         queries++;
      }
   }

   if (p_uri->fragment)
      length += escaped_length(p_uri->fragment, fragment_reserved_chars) + 1; /* for the '#' */

   return length;
}

/*****************************************************************************/
static void build_uri(const VC_URI_PARTS_T *p_uri, char *buffer, size_t buffer_size)
{
   uint32_t count;

   /* With no scheme, assume this is a plain path (without escaping) */
   if (!p_uri->scheme)
   {
      if (p_uri->path)
         strncpy(buffer, p_uri->path, buffer_size);
      else
         buffer[0] = '\0';
      return;
   }

   buffer += escape_string(p_uri->scheme, buffer, scheme_reserved_chars);
   *buffer++ = ':';

   if (p_uri->host)
   {
      *buffer++ = '/';
      *buffer++ = '/';
      if (p_uri->userinfo)
      {
         buffer += escape_string(p_uri->userinfo, buffer, userinfo_reserved_chars);
         *buffer++ = '@';
      }
      buffer += escape_string(p_uri->host, buffer, host_reserved_chars);
      if (p_uri->port)
      {
         *buffer++ = ':';
         buffer += escape_string(p_uri->port, buffer, port_reserved_chars);
      }
   }

   if (p_uri->path)
      buffer += escape_string(p_uri->path, buffer, path_reserved_chars);

   count = p_uri->num_queries;
   if (count)
   {
      VC_URI_QUERY_T * queries = p_uri->queries;

      *buffer++ = '?';
      while (count--)
      {
         buffer += escape_string(queries->name, buffer, query_reserved_chars);

         if (queries->value)
         {
            *buffer++ = '=';
            buffer += escape_string(queries->value, buffer, query_reserved_chars);
         }

         /* Add separator if there is another item to add */
         if (count)
            *buffer++ = '&';

         queries++;
      }
   }

   if (p_uri->fragment)
   {
      *buffer++ = '#';
      buffer += escape_string(p_uri->fragment, buffer, fragment_reserved_chars);
   }

   *buffer = '\0';
}

/*****************************************************************************/
static bool vc_uri_copy_base_path( const VC_URI_PARTS_T *base_uri,
      VC_URI_PARTS_T *relative_uri )
{
   const char *base_path = vc_uri_path(base_uri);

   /* No path set (or empty), copy from base */
   if (!vc_uri_set_path(relative_uri, base_path))
      return false;

   /* If relative path has no queries, copy base queries across */
   if (!vc_uri_num_queries(relative_uri))
   {
      uint32_t base_queries = vc_uri_num_queries(base_uri);
      const char *name, *value;
      uint32_t ii;

      for (ii = 0; ii < base_queries; ii++)
      {
         vc_uri_query(base_uri, ii, &name, &value);
         if (!vc_uri_add_query(relative_uri, name, value))
            return false;
      }
   }

   return true;
}

/*****************************************************************************/
static void vc_uri_remove_single_dot_segments( char *path_str )
{
   char *slash = path_str - 1;

   while (slash++)
   {
      if (*slash == '.')
      {
         switch (slash[1])
         {
         case '/':   /* Single dot segment, remove it */
            memmove(slash, slash + 2, strlen(slash + 2) + 1);
            break;
         case '\0':  /* Trailing single dot, remove it */
            *slash = '\0';
            break;
         default:    /* Something else (e.g. ".." or ".foo") */
            ;  /* Do nothing */
         }
      }
      slash = strchr(slash, '/');
   }
}

/*****************************************************************************/
static void vc_uri_remove_double_dot_segments( char *path_str )
{
   char *previous_segment = path_str;
   char *slash;

   if (previous_segment[0] == '/')
      previous_segment++;

   /* Remove strings of the form "<segment>/../" (or "<segment>/.." at the end of the path)
    * as long as <segment> is not itself ".." */
   slash = strchr(previous_segment, '/');
   while (slash)
   {
      if (previous_segment[0] != '.' || previous_segment[1] != '.' || previous_segment[2] != '/')
      {
         if (slash[1] == '.' && slash[2] == '.')
         {
            bool previous_segment_removed = true;

            switch (slash[3])
            {
            case '/':   /* "/../" inside path, snip it and last segment out */
               memmove(previous_segment, slash + 4, strlen(slash + 4) + 1);
               break;
            case '\0':  /* Trailing "/.." on path, just terminate path at last segment */
               *previous_segment = '\0';
               break;
            default:    /* Not a simple ".." segment, so skip over it */
               previous_segment_removed = false;
            }

            if (previous_segment_removed)
            {
               /* The segment just removed was the first one in the path (optionally
                * prefixed by a slash), so no more can be removed: stop. */
               if (previous_segment < path_str + 2)
                  break;

               /* Move back to slash before previous segment, or the start of the path */
               slash = previous_segment - 1;
               while (--slash >= path_str && *slash != '/')
                  ; /* Everything done in the while */
            }
         }
      }
      previous_segment = slash + 1;
      slash = strchr(previous_segment, '/');
   }
}

/*****************************************************************************/
/* API functions                                                             */
/*****************************************************************************/

VC_URI_PARTS_T *vc_uri_create( void )
{
   VC_URI_PARTS_T *p_uri;

   p_uri = (VC_URI_PARTS_T *)malloc(sizeof(VC_URI_PARTS_T));
   if (p_uri)
   {
      memset(p_uri, 0, sizeof(VC_URI_PARTS_T));
   }

   return p_uri;
}

/*****************************************************************************/
void vc_uri_clear( VC_URI_PARTS_T *p_uri )
{
   if (!p_uri)
      return;

   release_string(&p_uri->scheme);
   release_string(&p_uri->userinfo);
   release_string(&p_uri->host);
   release_string(&p_uri->port);
   release_string(&p_uri->path);
   release_string(&p_uri->fragment);

   if (p_uri->queries)
   {
      VC_URI_QUERY_T *queries = p_uri->queries;
      uint32_t count = p_uri->num_queries;

      while (count--)
      {
         release_string(&queries[count].name);
         release_string(&queries[count].value);
      }

      free(queries);
      p_uri->queries = NULL;
      p_uri->num_queries = 0;
   }
}

/*****************************************************************************/
void vc_uri_release( VC_URI_PARTS_T *p_uri )
{
   if (!p_uri)
      return;

   vc_uri_clear(p_uri);

   free(p_uri);
}

/*****************************************************************************/
bool vc_uri_parse( VC_URI_PARTS_T *p_uri, const char *uri )
{
   const char *marker;
   uint32_t len;

   if (!p_uri || !uri)
      return false;

   vc_uri_clear(p_uri);

   /* URI = scheme ":" hier_part [ "?" query ] [ "#" fragment ] */

   /* Find end of scheme, or another separator */
   marker = vc_uri_find_delimiter(uri, SCHEME_DELIMITERS);

   if (*marker == ':')
   {
      len = (marker - uri);
      if (isalpha((int)*uri) && len == 1 && marker[1] == '\\')
      {
         /* Looks like a bare, absolute DOS/Windows filename with a drive letter */
         /* coverity[double_free] Pointer freed and set to NULL */
         bool ret = duplicate_string(uri, &p_uri->path);
         vc_uri_set_path_extension(p_uri);
         return ret;
      }

      p_uri->scheme = create_unescaped_string(uri, len);
      if (!p_uri->scheme)
         goto error;

      to_lower_string(p_uri->scheme);  /* Schemes should be handled case-insensitively */
      uri = marker + 1;
   }

   if (uri[0] == '/' && uri[1] == '/') /* hier-part includes authority */
   {
      const char *userinfo_end = NULL;

      /* authority = [ userinfo "@" ] host [ ":" port ] */
      uri += 2;

      marker = vc_uri_find_delimiter(uri, NETWORK_DELIMITERS);
      if (*marker == '@')
      {
         userinfo_end = marker;
         marker = vc_uri_find_delimiter(marker + 1, HOST_PORT_DELIMITERS);
      }

      if (!parse_authority(p_uri, uri, marker - uri, userinfo_end))
         goto error;
      uri = marker;
   }

   /* path */
   marker = vc_uri_find_delimiter(uri, PATH_DELIMITERS);
   len = marker - uri;
   if (len)
   {
      p_uri->path = create_unescaped_string(uri, len);
      vc_uri_set_path_extension(p_uri);
      if (!p_uri->path)
         goto error;
   }

   /* query */
   if (*marker == '?')
   {
      uri = marker + 1;
      marker = vc_uri_find_delimiter(uri, QUERY_DELIMITERS);
      if (!parse_query(p_uri, uri, marker - uri))
         goto error;
   }

   /* fragment */
   if (*marker == '#')
   {
      uri = marker + 1;
      p_uri->fragment = create_unescaped_string(uri, strlen(uri));
      if (!p_uri->fragment)
         goto error;
   }

   return true;

error:
   vc_uri_clear(p_uri);
   return false;
}

/*****************************************************************************/
uint32_t vc_uri_build( const VC_URI_PARTS_T *p_uri, char *buffer, size_t buffer_size )
{
   uint32_t required_length;

   if (!p_uri)
      return 0;

   required_length = calculate_uri_length(p_uri);
   if (buffer && required_length < buffer_size)  /* Allow for NUL */
      build_uri(p_uri, buffer, buffer_size);

   return required_length;
}

/*****************************************************************************/
const char *vc_uri_scheme( const VC_URI_PARTS_T *p_uri )
{
   return p_uri ? p_uri->scheme : NULL;
}

/*****************************************************************************/
const char *vc_uri_userinfo( const VC_URI_PARTS_T *p_uri )
{
   return p_uri ? p_uri->userinfo : NULL;
}

/*****************************************************************************/
const char *vc_uri_host( const VC_URI_PARTS_T *p_uri )
{
   return p_uri ? p_uri->host : NULL;
}

/*****************************************************************************/
const char *vc_uri_port( const VC_URI_PARTS_T *p_uri )
{
   return p_uri ? p_uri->port : NULL;
}

/*****************************************************************************/
const char *vc_uri_path( const VC_URI_PARTS_T *p_uri )
{
   return p_uri ? p_uri->path : NULL;
}

/*****************************************************************************/
const char *vc_uri_path_extension( const VC_URI_PARTS_T *p_uri )
{
   return p_uri ? p_uri->path_extension : NULL;
}

/*****************************************************************************/
const char *vc_uri_fragment( const VC_URI_PARTS_T *p_uri )
{
   return p_uri ? p_uri->fragment : NULL;
}

/*****************************************************************************/
uint32_t vc_uri_num_queries( const VC_URI_PARTS_T *p_uri )
{
   return p_uri ? p_uri->num_queries : 0;
}

/*****************************************************************************/
void vc_uri_query( const VC_URI_PARTS_T *p_uri, uint32_t index, const char **p_name, const char **p_value )
{
   const char *name = NULL;
   const char *value = NULL;

   if (p_uri)
   {
      if (index < p_uri->num_queries)
      {
         name = p_uri->queries[index].name;
         value = p_uri->queries[index].value;
      }
   }

   if (p_name)
      *p_name = name;
   if (p_value)
      *p_value = value;
}

/*****************************************************************************/
bool vc_uri_find_query( VC_URI_PARTS_T *p_uri, uint32_t *p_index, const char *name, const char **p_value )
{
   unsigned int i = p_index ? *p_index : 0;

   if (!p_uri)
      return false;

   for (; name && i < p_uri->num_queries; i++)
   {
      if (!strcmp(name, p_uri->queries[i].name))
      {
         if (p_value)
            *p_value = p_uri->queries[i].value;
         if (p_index)
            *p_index = i;
         return true;
      }
   }

   return false;
}

/*****************************************************************************/
bool vc_uri_set_scheme( VC_URI_PARTS_T *p_uri, const char *scheme )
{
   return p_uri ? duplicate_string(scheme, &p_uri->scheme) : false;
}

/*****************************************************************************/
bool vc_uri_set_userinfo( VC_URI_PARTS_T *p_uri, const char *userinfo )
{
   return p_uri ? duplicate_string(userinfo, &p_uri->userinfo) : false;
}

/*****************************************************************************/
bool vc_uri_set_host( VC_URI_PARTS_T *p_uri, const char *host )
{
   return p_uri ? duplicate_string(host, &p_uri->host) : false;
}

/*****************************************************************************/
bool vc_uri_set_port( VC_URI_PARTS_T *p_uri, const char *port )
{
   return p_uri ? duplicate_string(port, &p_uri->port) : false;
}

/*****************************************************************************/
bool vc_uri_set_path( VC_URI_PARTS_T *p_uri, const char *path )
{
   bool ret = p_uri ? duplicate_string(path, &p_uri->path) : false;
   vc_uri_set_path_extension(p_uri);
   return ret;
}

/*****************************************************************************/
bool vc_uri_set_fragment( VC_URI_PARTS_T *p_uri, const char *fragment )
{
   return p_uri ? duplicate_string(fragment, &p_uri->fragment) : false;
}

/*****************************************************************************/
bool vc_uri_add_query( VC_URI_PARTS_T *p_uri, const char *name, const char *value )
{
   VC_URI_QUERY_T *queries;
   uint32_t count;

   if (!p_uri || !name)
      return false;

   count = p_uri->num_queries;
   if (p_uri->queries)
      queries = (VC_URI_QUERY_T *)realloc(p_uri->queries, (count + 1) * sizeof(VC_URI_QUERY_T));
   else
      queries = (VC_URI_QUERY_T *)malloc(sizeof(VC_URI_QUERY_T));

   if (!queries)
      return false;

   /* Always store the pointer, in case it has changed, and even if we fail to copy name/value */
   p_uri->queries = queries;
   queries[count].name = NULL;
   queries[count].value = NULL;

   if (duplicate_string(name, &queries[count].name))
   {
      if (duplicate_string(value, &queries[count].value))
      {
         /* Successful exit path */
         p_uri->num_queries++;
         return true;
      }

      release_string(&queries[count].name);
   }

   return false;
}

/*****************************************************************************/
bool vc_uri_merge( const VC_URI_PARTS_T *base_uri, VC_URI_PARTS_T *relative_uri )
{
   bool success = true;
   const char *relative_path;

   /* If scheme is already set, the URI is already absolute */
   if (relative_uri->scheme)
      return true;

   /* Otherwise, copy the base scheme */
   if (!duplicate_string(base_uri->scheme, &relative_uri->scheme))
      return false;

   /* If any of the network info is set, use the rest of the relative URI as-is */
   if (relative_uri->host || relative_uri->port || relative_uri->userinfo)
      return true;

   /* Otherwise, copy the base network info */
   if (!duplicate_string(base_uri->host, &relative_uri->host) ||
         !duplicate_string(base_uri->port, &relative_uri->port) ||
         !duplicate_string(base_uri->userinfo, &relative_uri->userinfo))
      return false;

   relative_path = relative_uri->path;

   if (!relative_path || !*relative_path)
   {
      /* No relative path (could be queries and/or fragment), so take base path */
      success = vc_uri_copy_base_path(base_uri, relative_uri);
   }
   else if (*relative_path != '/')
   {
      const char *base_path = base_uri->path;
      char *merged_path;
      char *slash;
      size_t len;

      /* Path is relative, merge in with base path */
      if (!base_path || !*base_path)
      {
         if (relative_uri->host || relative_uri->port || relative_uri->userinfo)
            base_path = "/";  /* Need a separator to split network info from path */
         else
            base_path = "";
      }

      len = strlen(base_path) + strlen(relative_path) + 1;

      /* Allocate space for largest possible combined path */
      merged_path = (char *)malloc(len);
      if (!merged_path)
         return false;

      strncpy(merged_path, base_path, len);

      slash = strrchr(merged_path, '/');  /* Note: reverse search */
      if (*relative_path == ';')
      {
         char *semi;

         /* Relative path is just parameters, so remove any base parameters in final segment */
         if (!slash)
            slash = merged_path;
         semi = strchr(slash, ';');
         if (semi)
            semi[0] = '\0';
      } else {
         /* Remove final segment */
         if (slash)
            slash[1] = '\0';
         else
            merged_path[0] = '\0';
      }
      strncat(merged_path, relative_path, len - strlen(merged_path) - 1);

      vc_uri_remove_single_dot_segments(merged_path);
      vc_uri_remove_double_dot_segments(merged_path);

      success = duplicate_string(merged_path, &relative_uri->path);

      free(merged_path);
   }
   /* Otherwise path is absolute, which can be left as-is */

   return success;
}
