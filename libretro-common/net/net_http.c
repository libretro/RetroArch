/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_http.c).
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
#include <stdlib.h>
#include <ctype.h>

#include <net/net_http.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#ifdef HAVE_SSL
#include <net/net_socket_ssl.h>
#endif
#include <compat/strl.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <string.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <verbosity.h>
#include <config.h>

enum
{
   P_HEADER_TOP = 0,
   P_HEADER,
   P_BODY,
   P_BODY_CHUNKLEN,
   P_DONE,
   P_ERROR
};

enum
{
   T_FULL = 0,
   T_LEN,
   T_CHUNK,
   T_NONE
};

struct http_socket_state_t
{
   void *ssl_ctx;
   int fd;
   bool ssl;
};

struct http_response_t
{
   int status;
   bool error;

   struct http_headers_t *headers;
   uint8_t *data;
   size_t data_len;
};

struct http_t
{
   uint8_t *data;
   RFILE *response_file;
   struct http_socket_state_t sock_state; /* ptr alignment */
   size_t pos;
   size_t len;
   size_t buflen;
   int status;
   char part;
   char bodytype;
   bool error;
   struct http_headers_t *headers;
   struct http_response_t *response;
};

struct http_url_parameter_t
{
   char *name;
   char **values;
   size_t len;
};

struct http_url_parameters_t
{
   struct http_url_parameter_t *params;
   size_t len;
};

struct http_header_t
{
   char *name;
   char *value;
};

struct http_headers_t
{
   struct http_header_t *headers;
   size_t len;
};

enum http_request_body_type_t
{
   HTTP_REQUEST_BODY_EMPTY,
   HTTP_REQUEST_BODY_FROM_RAW,
   HTTP_REQUEST_BODY_FROM_FILE
};

struct http_request_body_raw_t
{
   uint8_t *data;
   size_t length;
};

struct http_request_body_file_t
{
   RFILE *file;
   int64_t max_bytes;
};

struct http_request_body_t
{
   enum http_request_body_type_t body_type;
   union
   {
      struct http_request_body_raw_t raw;
      struct http_request_body_file_t file;
   } value;
};

struct http_request_t
{
   char *url;
   char *method;
   struct http_url_parameters_t url_params;
   struct http_headers_t *headers;
   struct http_request_body_t body;
   char *response_filename;
};

struct http_connection_t
{
   char *domain;
   char *location;
   char *url;
   char *scan;
   struct http_request_t *request;
   struct http_socket_state_t sock_state; /* ptr alignment */
   int port;
   bool log_request_body;
   bool log_response_body;
};

#define MAX_LOG_LINE_LENGTH 60

/* URL Encode a string
   caller is responsible for deleting the destination buffer */
void net_http_urlencode(char **dest, const char *source)
{
   static const char urlencode_lut[256] = 
   {
      0,       /* 0   */
      0,       /* 1   */
      0,       /* 2   */
      0,       /* 3   */
      0,       /* 4   */
      0,       /* 5   */
      0,       /* 6   */
      0,       /* 7   */
      0,       /* 8   */
      0,       /* 9   */
      0,       /* 10  */
      0,       /* 11  */
      0,       /* 12  */
      0,       /* 13  */
      0,       /* 14  */
      0,       /* 15  */
      0,       /* 16  */
      0,       /* 17  */
      0,       /* 18  */
      0,       /* 19  */
      0,       /* 20  */
      0,       /* 21  */
      0,       /* 22  */
      0,       /* 23  */
      0,       /* 24  */
      0,       /* 25  */
      0,       /* 26  */
      0,       /* 27  */
      0,       /* 28  */
      0,       /* 29  */
      0,       /* 30  */
      0,       /* 31  */
      0,       /* 32  */
      0,       /* 33  */
      0,       /* 34  */
      0,       /* 35  */
      0,       /* 36  */
      0,       /* 37  */
      0,       /* 38  */
      0,       /* 39  */
      0,       /* 40  */
      0,       /* 41  */
      '*',     /* 42  */
      0,       /* 43  */
      0,       /* 44  */
      '-',     /* 45  */
      '.',     /* 46  */
      '/',     /* 47  */
      '0',     /* 48  */
      '1',     /* 49  */
      '2',     /* 50  */
      '3',     /* 51  */
      '4',     /* 52  */
      '5',     /* 53  */
      '6',     /* 54  */
      '7',     /* 55  */
      '8',     /* 56  */
      '9',     /* 57  */
      0,       /* 58  */
      0,       /* 59  */
      0,       /* 60  */
      0,       /* 61  */
      0,       /* 62  */
      0,       /* 63  */
      0,       /* 64  */
      'A',     /* 65  */
      'B',     /* 66  */
      'C',     /* 67  */
      'D',     /* 68  */
      'E',     /* 69  */
      'F',     /* 70  */
      'G',     /* 71  */
      'H',     /* 72  */
      'I',     /* 73  */
      'J',     /* 74  */
      'K',     /* 75  */
      'L',     /* 76  */
      'M',     /* 77  */
      'N',     /* 78  */
      'O',     /* 79  */
      'P',     /* 80  */
      'Q',     /* 81  */
      'R',     /* 82  */
      'S',     /* 83  */
      'T',     /* 84  */
      'U',     /* 85  */
      'V',     /* 86  */
      'W',     /* 87  */
      'X',     /* 88  */
      'Y',     /* 89  */
      'Z',     /* 90  */
      0,       /* 91  */
      0,       /* 92  */
      0,       /* 93  */
      0,       /* 94  */
      '_',     /* 95  */
      0,       /* 96  */
      'a',     /* 97  */
      'b',     /* 98  */
      'c',     /* 99  */
      'd',     /* 100 */
      'e',     /* 101 */
      'f',     /* 102 */
      'g',     /* 103 */
      'h',     /* 104 */
      'i',     /* 105 */
      'j',     /* 106 */
      'k',     /* 107 */
      'l',     /* 108 */
      'm',     /* 109 */
      'n',     /* 110 */
      'o',     /* 111 */
      'p',     /* 112 */
      'q',     /* 113 */
      'r',     /* 114 */
      's',     /* 115 */
      't',     /* 116 */
      'u',     /* 117 */
      'v',     /* 118 */
      'w',     /* 119 */
      'x',     /* 120 */
      'y',     /* 121 */
      'z',     /* 122 */
      0,       /* 123 */
      0,       /* 124 */
      0,       /* 125 */
      0,       /* 126 */
      0,       /* 127 */
      0,       /* 128 */
      0,       /* 129 */
      0,       /* 130 */
      0,       /* 131 */
      0,       /* 132 */
      0,       /* 133 */
      0,       /* 134 */
      0,       /* 135 */
      0,       /* 136 */
      0,       /* 137 */
      0,       /* 138 */
      0,       /* 139 */
      0,       /* 140 */
      0,       /* 141 */
      0,       /* 142 */
      0,       /* 143 */
      0,       /* 144 */
      0,       /* 145 */
      0,       /* 146 */
      0,       /* 147 */
      0,       /* 148 */
      0,       /* 149 */
      0,       /* 150 */
      0,       /* 151 */
      0,       /* 152 */
      0,       /* 153 */
      0,       /* 154 */
      0,       /* 155 */
      0,       /* 156 */
      0,       /* 157 */
      0,       /* 158 */
      0,       /* 159 */
      0,       /* 160 */
      0,       /* 161 */
      0,       /* 162 */
      0,       /* 163 */
      0,       /* 164 */
      0,       /* 165 */
      0,       /* 166 */
      0,       /* 167 */
      0,       /* 168 */
      0,       /* 169 */
      0,       /* 170 */
      0,       /* 171 */
      0,       /* 172 */
      0,       /* 173 */
      0,       /* 174 */
      0,       /* 175 */
      0,       /* 176 */
      0,       /* 177 */
      0,       /* 178 */
      0,       /* 179 */
      0,       /* 180 */
      0,       /* 181 */
      0,       /* 182 */
      0,       /* 183 */
      0,       /* 184 */
      0,       /* 185 */
      0,       /* 186 */
      0,       /* 187 */
      0,       /* 188 */
      0,       /* 189 */
      0,       /* 190 */
      0,       /* 191 */
      0,       /* 192 */
      0,       /* 193 */
      0,       /* 194 */
      0,       /* 195 */
      0,       /* 196 */
      0,       /* 197 */
      0,       /* 198 */
      0,       /* 199 */
      0,       /* 200 */
      0,       /* 201 */
      0,       /* 202 */
      0,       /* 203 */
      0,       /* 204 */
      0,       /* 205 */
      0,       /* 206 */
      0,       /* 207 */
      0,       /* 208 */
      0,       /* 209 */
      0,       /* 210 */
      0,       /* 211 */
      0,       /* 212 */
      0,       /* 213 */
      0,       /* 214 */
      0,       /* 215 */
      0,       /* 216 */
      0,       /* 217 */
      0,       /* 218 */
      0,       /* 219 */
      0,       /* 220 */
      0,       /* 221 */
      0,       /* 222 */
      0,       /* 223 */
      0,       /* 224 */
      0,       /* 225 */
      0,       /* 226 */
      0,       /* 227 */
      0,       /* 228 */
      0,       /* 229 */
      0,       /* 230 */
      0,       /* 231 */
      0,       /* 232 */
      0,       /* 233 */
      0,       /* 234 */
      0,       /* 235 */
      0,       /* 236 */
      0,       /* 237 */
      0,       /* 238 */
      0,       /* 239 */
      0,       /* 240 */
      0,       /* 241 */
      0,       /* 242 */
      0,       /* 243 */
      0,       /* 244 */
      0,       /* 245 */
      0,       /* 246 */
      0,       /* 247 */
      0,       /* 248 */
      0,       /* 249 */
      0,       /* 250 */
      0,       /* 251 */
      0,       /* 252 */
      0,       /* 253 */
      0,       /* 254 */
      0        /* 255 */
   };

   /* Assume every character will be encoded, so we need 3 times the space. */
   size_t len                       = strlen(source) * 3 + 1;
   size_t count                     = len;
   char *enc                        = (char*)calloc(1, len);
   *dest                            = enc;

   for (; *source; source++)
   {
      int written = 0;

      /* any non-ASCII character will just be encoded without question */
      if ((unsigned)*source < sizeof(urlencode_lut) && urlencode_lut[(unsigned)*source])
         written = snprintf(enc, count, "%c", urlencode_lut[(unsigned)*source]);
      else
         written = snprintf(enc, count, "%%%02X", *source & 0xFF);

      if (written > 0)
         count -= written;

      while (*++enc);
   }

   (*dest)[len - 1] = '\0';
}

/* Re-encode a full URL */
void net_http_urlencode_full(char *dest,
      const char *source, size_t size)
{
   size_t buf_pos                    = 0;
   char *tmp                         = NULL;
   char url_domain[PATH_MAX_LENGTH]  = {0};
   char url_path[PATH_MAX_LENGTH]    = {0};
   int count                         = 0;

   strlcpy(url_path, source, sizeof(url_path));
   tmp = url_path;

   while (count < 3 && tmp[0] != '\0')
   {
      tmp = strchr(tmp, '/');
      count++;
      tmp++;
   }

   strlcpy(url_domain, source, tmp - url_path);

   strlcpy(url_path,
         source + strlen(url_domain) + 1,
         strlen(tmp) + 1
         );

   tmp            = NULL;
   net_http_urlencode(&tmp, url_path);
   buf_pos         = strlcpy(dest, url_domain, size);
   dest[buf_pos]   = '/';
   dest[buf_pos+1] = '\0';
   strlcat(dest, tmp, size);
   free (tmp);
}

struct http_request_t *net_http_request_new()
{
   struct http_request_t *request;

   request = (struct http_request_t *)calloc(1, sizeof(struct http_request_t));
   request->headers = (struct http_headers_t *)calloc(1, sizeof(struct http_headers_t));
   request->body.body_type = HTTP_REQUEST_BODY_EMPTY;

   return request;
}

void net_http_request_set_url(struct http_request_t *request, const char *url)
{
   request->url = (char *)calloc(strlen(url) + 1, sizeof(char));
   strcpy(request->url, url);
}

void net_http_request_set_method(struct http_request_t *request, const char *method)
{
   request->method = (char *)calloc(strlen(method) + 1, sizeof(char));
   strcpy(request->method, method);
}

static void net_http_url_parameter_set_value(
   struct http_url_parameters_t *params,
   const char *name,
   const char *value,
   bool replace)
{
   struct http_url_parameter_t *param = NULL;
   size_t i;
   size_t name_len;

   if (!params)
      return;

   name_len = strlen(name);
   for (i = 0;i < params->len;i++)
   {
      if (!strncmp(name, params->params[i].name, name_len))
      {
         param = &(params->params[i]);
         break;
      }
   }

   if (param)
   {
      if (replace)
      {
         for (i = 0;i < param->len;i++)
         {
            free(param->values[i]);
         }
         free(param->values);

         param->values = (char **)calloc(4, sizeof(char *));
         param->values[0] = (char *)calloc(strlen(value) + 1, sizeof(char));
         strcpy(param->values[param->len], value);
         param->len = 1;
      } else
      {
         if (param->len % 4 == 0) {
            param->values = (char **)realloc(param->values, sizeof(char *) * (param->len + 4));
         }

         param->values[param->len] = (char *)calloc(strlen(value) + 1, sizeof(char));
         strcpy(param->values[param->len], value);
         param->len++;
      }
   } else
   {
      if (params->len == 0)
      {
         params->params = (struct http_url_parameter_t *)malloc(sizeof(struct http_url_parameter_t) * 4);
      } else if (params->len % 4 == 0)
      {
         params->params = (struct http_url_parameter_t *)realloc(params->params, sizeof(struct http_url_parameter_t) * (params->len + 4));
      }

      params->params[params->len].name = (char *)calloc(strlen(name) + 1, sizeof(char));
      strcpy(params->params[params->len].name, name);

      params->params[params->len].values = (char **)malloc(sizeof(char *) * 4);
      params->params[params->len].values[0] = (char *)calloc(strlen(value) + 1, sizeof(char));
      strcpy(params->params[params->len].values[0], value);
      params->params[params->len].len = 1;
      params->len++;
   }
}

void net_http_request_set_url_param(struct http_request_t *request, const char *name, const char *value, bool replace)
{
   net_http_url_parameter_set_value(&(request->url_params), name, value, replace);
}

static void net_http_headers_add_value(struct http_headers_t *headers, const char *name, const char *value, bool replace)
{
   if (!headers)
      return;

   if (headers->len == 0)
   {
      headers->headers = (struct http_header_t *)malloc(sizeof(struct http_header_t) * 4);

      headers->headers[headers->len].name = (char *)calloc(strlen(name) + 1, sizeof(char));
      strcpy(headers->headers[headers->len].name, name);
      headers->headers[headers->len].value = (char *)calloc(strlen(value) + 1, sizeof(char));
      strcpy(headers->headers[headers->len++].value, value);

      return;
   } else if (headers->len % 4 == 0)
   {
      headers->headers = (struct http_header_t *)realloc(headers->headers, sizeof(struct http_header_t) * (headers->len + 4));
   }

   if (replace)
   {
      size_t new_len = headers->len + 1;
      size_t i;

      for (i = 0;i < headers->len;i++)
      {
         if (!strcmp(headers->headers[i].name, name))
         {
            free(headers->headers[i].name);
            headers->headers[i].name = NULL;
            free(headers->headers[i].value);
            headers->headers[i].value = NULL;
            new_len--;
         }
      }

      if (new_len > headers->len)
      {
         if (headers->len % 4 == 0)
         {
            headers->headers = (struct http_header_t *)realloc(headers->headers, ((new_len % 4) + 1) * sizeof(struct http_header_t));
         }

         headers->headers[headers->len].name = (char *)calloc(strlen(name) + 1, sizeof(char));
         strcpy(headers->headers[headers->len].name, name);
         headers->headers[headers->len].value = (char *)calloc(strlen(value) + 1, sizeof(char));
         strcpy(headers->headers[headers->len].value, value);
         headers->len++;
      } else if (new_len == headers->len)
      {
         for (i = 0;i < headers->len;i++)
         {
            if (headers->headers[i].name == NULL)
            {
               headers->headers[i].value = (char *)calloc(strlen(value) + 1, sizeof(char));
               strcpy(headers->headers[i].value, value);
            }
         }
      } else
      {
         struct http_header_t *new_headers_list;
         size_t j = 0;

         new_headers_list = (struct http_header_t *)malloc(((new_len % 4) + 1) * sizeof(struct http_header_t));
         for (i = 0;i < headers->len;i++)
         {
            if (headers->headers[i].name)
            {
               new_headers_list[j].name = headers->headers[i].name;
               new_headers_list[j].value = headers->headers[i].value;
               j++;
            }
         }

         free(headers->headers);
         headers->headers = new_headers_list;
         headers->len = new_len;

         headers->headers[headers->len++].name = (char *)calloc(strlen(name) + 1, sizeof(char));
         strcpy(headers->headers[headers->len].name, name);
         headers->headers[headers->len].value = (char *)calloc(strlen(value) + 1, sizeof(char));
         strcpy(headers->headers[headers->len].value, value);
      }
   } else
   {
      if (headers->len % 4 == 0)
      {
         headers->headers = (struct http_header_t *)realloc(headers->headers, sizeof(struct http_header_t) * (headers->len + 4));
      }

      headers->headers[headers->len].name = (char *)calloc(strlen(name) + 1, sizeof(char));
      strcpy(headers->headers[headers->len].name, name);

      headers->headers[headers->len].value = (char *)calloc(strlen(value) + 1, sizeof(char));
      strcpy(headers->headers[headers->len].value, value);
      headers->len++;
   }
}

void net_http_request_set_header(struct http_request_t *request, const char *name, const char *value, bool replace)
{
   net_http_headers_add_value(request->headers, name, value, replace);
}

void net_http_request_set_body_raw(struct http_request_t *request, uint8_t *data, const size_t len)
{
   request->body.body_type = HTTP_REQUEST_BODY_FROM_RAW;
   request->body.value.raw.data = (uint8_t *)malloc(len);
   memcpy(request->body.value.raw.data, data, len);
   request->body.value.raw.length = len;
}

void net_http_request_set_body_file(struct http_request_t *request, RFILE *file, int64_t max_bytes)
{
   request->body.body_type = HTTP_REQUEST_BODY_FROM_FILE;
   request->body.value.file.file = file;
   request->body.value.file.max_bytes = max_bytes;
}

void net_http_request_set_response_file(struct http_request_t *request, const char *filename)
{
   request->response_filename = (char *)calloc(strlen(filename) + 1, sizeof(char));
   strcpy(request->response_filename, filename);
}

static void net_http_url_parameters_free(struct http_url_parameters_t params)
{
   int i, j;

   if (!params.len)
     return;

   for (i = 0;i < params.len;i++)
   {
      free(params.params[i].name);
      for (j = 0;j < params.params[i].len;j++)
      {
         free(params.params[i].values[j]);
      }
      free(params.params[i].values);
   }

   free(params.params);
}

static void net_http_headers_free(struct http_headers_t *headers)
{
   int i;

   if (!headers->len)
     return;

   for (i = 0;i < headers->len;i++)
   {
      free(headers->headers[i].name);
      free(headers->headers[i].value);
   }

   free(headers->headers);
   free(headers);
}

void net_http_request_free(struct http_request_t *request)
{
   if (request->url)
   {
      free(request->url);
   }
   if (request->method)
   {
      free(request->method);
   }
   net_http_url_parameters_free(request->url_params);
   net_http_headers_free(request->headers);

   switch (request->body.body_type)
   {
      case HTTP_REQUEST_BODY_FROM_RAW:
         free(request->body.value.raw.data);
         break;
      case HTTP_REQUEST_BODY_FROM_FILE:
         filestream_close(request->body.value.file.file);
         break;
      default:
         break;
   }

   free(request);
}

static int net_http_new_socket(struct http_connection_t *conn)
{
   int ret;
   struct addrinfo *addr = NULL, *next_addr = NULL;
   int fd;

#ifdef HAVE_DEBUG
   RARCH_ERR("[HTTP] Connecting to %s:%d\n", conn->domain, conn->port);
#endif
   fd = socket_init(
         (void**)&addr, conn->port, conn->domain, SOCKET_TYPE_STREAM);
#ifdef HAVE_SSL
   if (conn->sock_state.ssl)
   {
      if (!(conn->sock_state.ssl_ctx = ssl_socket_init(fd, conn->domain)))
         return -1;
   }
#endif

   next_addr = addr;

   while (fd >= 0)
   {
#ifdef HAVE_SSL
      if (conn->sock_state.ssl)
      {
         ret = ssl_socket_connect(conn->sock_state.ssl_ctx,
               (void*)next_addr, true, true);

         if (ret >= 0)
            break;

         ssl_socket_close(conn->sock_state.ssl_ctx);
      }
      else
#endif
      {
         ret = socket_connect(fd, (void*)next_addr, true);

         if (ret >= 0 && socket_nonblock(fd))
            break;

         socket_close(fd);
      }

      fd = socket_next((void**)&next_addr);
   }

   if (addr)
      freeaddrinfo_retro(addr);

   conn->sock_state.fd = fd;

   return fd;
}

static void net_http_send_bytes(
      struct http_socket_state_t *sock_state, bool *error, const uint8_t *bytes, size_t bytes_len)
{
   size_t offset = 0;

   if (*error)
      return;

   while (offset < bytes_len)
   {
      size_t bytes_to_send;

      if (offset + (1 << 14) > bytes_len)
      {
         bytes_to_send = bytes_len - offset;
      } else
      {
         bytes_to_send = 1 << 14;
      }
#ifdef HAVE_SSL
      if (sock_state->ssl)
      {
         if (!ssl_socket_send_all_blocking(
                  sock_state->ssl_ctx, bytes + offset, bytes_to_send, true))
            *error = true;
      } else
      {
#endif
         if (!socket_send_all_blocking(
                  sock_state->fd, bytes + offset, bytes_to_send, true))
            *error = true;
#ifdef HAVE_SSL
      }
#endif
      offset += bytes_to_send;
   }
}

static void net_http_send_str(
      struct http_socket_state_t *sock_state, bool *error, const char *text)
{
   net_http_send_bytes(sock_state, error, (const uint8_t *)text, strlen(text));
}

static char *net_http_generate_url(const char *url, struct http_url_parameters_t params)
{
   size_t param_pairs_count = 0;
   int i, j, param_pairs_index, total_length;
   char **param_pairs;
   char *encoded_name;
   char *encoded_value;
   char *full_url;

   if (!url)
      return NULL;

   if (!params.len)
   {
      full_url = (char *)calloc(strlen(url) + 1, sizeof(char));
      strcpy(full_url, url);
      return full_url;
   }

   for (i = 0;i < params.len;i++)
   {
      param_pairs_count += params.params[i].len;
   }

   param_pairs = param_pairs_count > 0 ? (char **)malloc(sizeof(char *) * param_pairs_count) : NULL;
   param_pairs_index = 0;
   for (i = 0;i < params.len;i++)
   {
      for (j = 0;j < params.params[i].len;j++)
      {
         net_http_urlencode(&encoded_name, params.params[i].name);
         net_http_urlencode(&encoded_value, params.params[i].values[j]);

         param_pairs[param_pairs_index] = (char *)calloc(strlen(encoded_name) + strlen(encoded_value) + 2, sizeof(char));
         strcpy(param_pairs[param_pairs_index], encoded_name);
         free(encoded_name);

         param_pairs[param_pairs_index][strlen(param_pairs[param_pairs_index])] = '=';
         strcpy(param_pairs[param_pairs_index] + strlen(param_pairs[param_pairs_index]), encoded_value);
         free(encoded_value);

         param_pairs_index++;
      }
   }

   total_length = strlen(url);
   if (param_pairs_count > 0)
   {
      if (!strchr(url, '?'))
      {
         total_length++;
      } else
      {
         if (url[strlen(url) - 1] != '&')
            total_length++;
      }

      total_length += param_pairs_count - 1;
      for (i = 0;i < param_pairs_count;i++)
      {
         total_length += strlen(param_pairs[i]);
      }
   }

   full_url = (char *)calloc(total_length + 1, sizeof(char));
   strcpy(full_url, url);

   if (param_pairs_count > 0)
   {
      if (!strchr(url, '?'))
      {
         full_url[strlen(full_url)] = '?';
      } else if (full_url[strlen(full_url) - 1] != '?' && full_url[strlen(full_url) - 1] != '&')
      {
         full_url[strlen(full_url)] = '&';
      }

      for (i = 0;i < param_pairs_count;i++)
      {
         strcpy(full_url + strlen(full_url), param_pairs[i]);
         if (i < param_pairs_count - 1)
            full_url[strlen(full_url)] = '&';

         free(param_pairs[i]);
      }

      free(param_pairs);
   }

   return full_url;
}

struct http_connection_t *net_http_connection_new(struct http_request_t *request)
{
   struct http_connection_t *conn = (struct http_connection_t*)malloc(
         sizeof(*conn));

   if (!conn)
      goto error;

   if (!request->url)
      goto error;

   conn->request           = request;
   conn->url               = net_http_generate_url(request->url, request->url_params);
   conn->location          = NULL;
   conn->scan              = NULL;
   conn->port              = 0;
   conn->sock_state.fd     = 0;
   conn->sock_state.ssl    = false;
   conn->sock_state.ssl_ctx= NULL;

   if (!conn->url)
      goto error;

   if (!strncmp(conn->url, "http://", STRLEN_CONST("http://")))
      conn->scan           = conn->url + STRLEN_CONST("http://");
   else if (!strncmp(conn->url, "https://", STRLEN_CONST("https://")))
   {
      conn->scan           = conn->url + STRLEN_CONST("https://");
      conn->sock_state.ssl = true;
   }
   else
      goto error;

   if (string_is_empty(conn->scan))
      goto error;

   conn->domain = conn->scan;

   return conn;

error:
   if (conn->url)
      free(conn->url);
   if (conn->request)
      net_http_request_free(conn->request);
   conn->url          = NULL;
   conn->request      = NULL;
   free(conn);
   return NULL;
}

bool net_http_connection_iterate(struct http_connection_t *conn)
{
   if (!conn)
      return false;

   while (*conn->scan != '/' && *conn->scan != ':' && *conn->scan != '\0')
      conn->scan++;

   return true;
}

bool net_http_connection_done(struct http_connection_t *conn)
{
   int has_port = 0;

   if (!conn || !conn->domain || !*conn->domain)
      return false;

   if (*conn->scan == ':')
   {
      /* domain followed by port, split off the port */
      *conn->scan++ = '\0';

      if (!isdigit((int)(*conn->scan)))
         return false;

      conn->port = (int)strtoul(conn->scan, &conn->scan, 10);
      has_port = 1;
   }
   else if (conn->port == 0)
   {
      /* port not specified, default to standard HTTP or HTTPS port */
      if (conn->sock_state.ssl)
         conn->port = 443;
      else
         conn->port = 80;
   }

   if (*conn->scan == '/')
   {
      /* domain followed by location - split off the location */
      /*   site.com/path.html   or   site.com:80/path.html   */
      *conn->scan    = '\0';
      conn->location = conn->scan + 1;
      return true;
   }
   else if (!*conn->scan)
   {
      /* domain with no location - point location at empty string */
      /*   site.com   or   site.com:80   */
      conn->location = conn->scan;
      return true;
   }
   else if (*conn->scan == '?')
   {
      /* domain with no location, but still has query parms - point location at the query parms */
      /*   site.com?param=3   or  site.com:80?param=3   */
      if (!has_port)
      {
         /* if there wasn't a port, we have to expand the urlcopy so we can separate the two parts */
         size_t domain_len   = strlen(conn->domain);
         size_t location_len = strlen(conn->scan);
         char* urlcopy       = (char*)malloc(domain_len + location_len + 2);
         memcpy(urlcopy, conn->domain, domain_len);
         urlcopy[domain_len] = '\0';
         memcpy(urlcopy + domain_len + 1, conn->scan, location_len + 1);

         free(conn->url);
         conn->domain        = conn->url     = urlcopy;
         conn->location      = conn->scan    = urlcopy + domain_len + 1;
      }
      else
      {
         /* there was a port, so overwriting the : will terminate the domain and we can just point at the ? */
         conn->location      = conn->scan;
      }

      return true;
   }

   /* invalid character after domain/port */
   return false;
}

void net_http_connection_free(struct http_connection_t *conn, bool free_request)
{
   if (!conn)
      return;

   if (conn->url)
      free(conn->url);

   if (free_request && conn->request)
      net_http_request_free(conn->request);

   conn->url             = NULL;
   conn->request         = NULL;

   free(conn);
}

const char *net_http_connection_url(struct http_connection_t *conn)
{
   return conn->url;
}

char *net_http_headers_get_first_value(struct http_headers_t *headers, const char *name)
{
   int i;

   if (!headers)
      return NULL;

   for (i = 0;i < headers->len;i++)
   {
      if (!strcmp(name, headers->headers[i].name))
         return headers->headers[i].value;
   }

   return NULL;
}

char **net_http_headers_get_values(struct http_headers_t *headers, const char *name, size_t *values_len)
{
   int i;
   int j;
   char **values;

   *values_len = 0;
   if (!headers)
      return NULL;

   for (i = 0;i < headers->len;i++)
   {
      if (!strcmp(name, headers->headers[i].name))
         *values_len = *values_len + 1;
   }

   if (*values_len == 0)
      return NULL;

   j = 0;
   values = (char **)calloc(*values_len, sizeof(char *));
   for (i = 0;i < headers->len;i++)
   {
      if (!strcmp(name, headers->headers[i].name))
         values[j++] = headers->headers[i].value;
   }

   return values;
}

static void log_body(bool request, char *body, size_t length)
{
   char *carriageReturn;
   char *newline;

   carriageReturn = (char *)memchr(body, '\r', length);
   newline = (char *)memchr(body, '\n', length);

   while (length > 0)
   {
      char *end;

      if (carriageReturn && newline && carriageReturn < newline)
      {
         end = carriageReturn;
      } else if (newline)
      {
         end = newline;
      } else if (carriageReturn)
      {
         end = carriageReturn;
      } else {
         end = body + length;
      }

      if (end - body > MAX_LOG_LINE_LENGTH)
      {
         if (request)
         {
            RARCH_LOG("[HTTP] >> %.60s ...\n", body);
         } else
         {
            RARCH_LOG("[HTTP] << %.60s ...\n", body);
         }

         body += 60;
         length -= 60;
      } else
      {
         char *fmt_string;

         fmt_string = (char *)calloc(30, sizeof(char));
         if (request)
         {
            snprintf(fmt_string, 19, "[HTTP] >> %%.%lus\n", (unsigned long)(end - body));
         } else
         {
            snprintf(fmt_string, 19, "[HTTP] << %%.%lus\n", (unsigned long)(end - body));
         }

         RARCH_LOG(fmt_string, body);
         free(fmt_string);

         if (carriageReturn && newline && newline > carriageReturn)
         {
            length -= newline - body + 1;
            body = newline;
            body += 1;
            carriageReturn = (char *)memchr(body, '\r', length);
            newline = (char *)memchr(body, '\n', length);
         } else if (carriageReturn)
         {
            length -= carriageReturn - body + 1;
            body = carriageReturn;
            body += 1;
            carriageReturn = (char *)memchr(body, '\r', length);
            newline = (char *)memchr(body, '\n', length);
         } else if (newline)
         {
            length -= newline - body + 1;
            body = newline;
            body += 1;
            carriageReturn = (char *)memchr(body, '\r', length);
            newline = (char *)memchr(body, '\n', length);
         } else
         {
            body += length;
            length = 0;
         }
      }
   }
}

struct http_t *net_http_new(struct http_connection_t *conn)
{
   bool error            = false;
   int fd                = -1;
   struct http_t *state  = NULL;
   char *contenttype     = NULL;
   char *useragent       = NULL;
   int i;

   if (!conn)
      goto error;

   fd = net_http_new_socket(conn);

   if (fd < 0)
      goto error;

   error = false;

#ifdef HAVE_DEBUG
   RARCH_LOG("[HTTP] Sending Request to: %s\n", conn->url);
#endif

   /* This is a bit lazy, but it works. */
   if (conn->request->method)
   {
#ifdef HAVE_DEBUG
      RARCH_LOG("[HTTP] >> %s /%s\n", conn->request->method, conn->location);
#endif
      net_http_send_str(&conn->sock_state, &error, conn->request->method);
      net_http_send_str(&conn->sock_state, &error, " /");
   }
   else
   {
#ifdef HAVE_DEBUG
      RARCH_LOG("[HTTP] >> GET /%s\n", conn->location);
#endif
      net_http_send_str(&conn->sock_state, &error, "GET /");
   }

#ifdef HAVE_DEBUG
   RARCH_LOG("[HTTP] >> HTTP/1.1\n");
#endif
   net_http_send_str(&conn->sock_state, &error, conn->location);
   net_http_send_str(&conn->sock_state, &error, " HTTP/1.1\r\n");

   net_http_send_str(&conn->sock_state, &error, "Host: ");
   net_http_send_str(&conn->sock_state, &error, conn->domain);

   if (!conn->port)
   {
      char portstr[16];

      portstr[0] = '\0';

      snprintf(portstr, sizeof(portstr), ":%i", conn->port);
      net_http_send_str(&conn->sock_state, &error, portstr);
   }

   net_http_send_str(&conn->sock_state, &error, "\r\n");

   if (conn->request->headers->len)
   {
      for (i = 0;i < conn->request->headers->len;i++)
      {
         if (string_is_equal(conn->request->headers->headers[i].name, "Content-Type"))
            contenttype = conn->request->headers->headers[i].value;
         else if (string_is_equal(conn->request->headers->headers[i].name, "User-Agent"))
            useragent = conn->request->headers->headers[i].value;
      }
   }

   /* This is not being set anywhere yet */
   if (contenttype)
   {
#ifdef HAVE_DEBUG
      RARCH_LOG("[HTTP] >> Content-Type: %s\n", contenttype);
#endif
      net_http_send_str(&conn->sock_state, &error, "Content-Type: ");
      net_http_send_str(&conn->sock_state, &error, contenttype);
      net_http_send_str(&conn->sock_state, &error, "\r\n");
   }

   if (conn->request->method && (string_is_equal(conn->request->method, "POST") ||
        string_is_equal(conn->request->method, "PUT") || string_is_equal(conn->request->method, "PATCH")))
   {
      size_t post_len, len;
      char *len_str        = NULL;

      if (conn->request->body.body_type == HTTP_REQUEST_BODY_EMPTY)
         goto error;

      if (!contenttype)
      {
#ifdef HAVE_DEBUG
         RARCH_LOG("[HTTP] >> Content-Type: application/x-www-form-urlencoded\n");
#endif
         net_http_send_str(&conn->sock_state, &error,
               "Content-Type: application/x-www-form-urlencoded\r\n");
      }

      net_http_send_str(&conn->sock_state, &error, "Content-Length: ");

      if (conn->request->body.body_type == HTTP_REQUEST_BODY_FROM_RAW)
      {
         post_len = conn->request->body.value.raw.length;
      } else if (conn->request->body.body_type == HTTP_REQUEST_BODY_FROM_FILE)
      {
         post_len = filestream_get_size(conn->request->body.value.file.file) - filestream_tell(conn->request->body.value.file.file);
         if (post_len > conn->request->body.value.file.max_bytes)
         {
            post_len = conn->request->body.value.file.max_bytes;
         }
      }

#if defined(_WIN32) || defined(_WIN64)
      len     = snprintf(NULL, 0, "%" PRIuPTR, post_len);
      len_str = (char*)malloc(len + 1);
      snprintf(len_str, len + 1, "%" PRIuPTR, post_len);
#else
      len     = snprintf(NULL, 0, "%llu", (long long unsigned)post_len);
      len_str = (char*)malloc(len + 1);
      snprintf(len_str, len + 1, "%llu", (long long unsigned)post_len);
#endif

      len_str[len] = '\0';
#ifdef HAVE_DEBUG
      RARCH_LOG("[HTTP] >> Content-Length: %s\n", len_str);
#endif

      net_http_send_str(&conn->sock_state, &error, len_str);
      net_http_send_str(&conn->sock_state, &error, "\r\n");

      free(len_str);
   }

   if (conn->request->headers->len)
   {
      for (i = 0;i < conn->request->headers->len;i++)
      {
         if (!string_is_equal(conn->request->headers->headers[i].name, "Content-Type") &&
              !string_is_equal(conn->request->headers->headers[i].name, "User-Agent"))
         {
#ifdef HAVE_DEBUG
            RARCH_LOG("[HTTP] >> %s: %s\n", conn->request->headers->headers[i].name,
               conn->request->headers->headers[i].value);
#endif
            net_http_send_str(&conn->sock_state, &error, conn->request->headers->headers[i].name);
            net_http_send_str(&conn->sock_state, &error, ": ");
            net_http_send_str(&conn->sock_state, &error, conn->request->headers->headers[i].value);
            net_http_send_str(&conn->sock_state, &error, "\r\n");
         }
      }
   }

#ifdef HAVE_DEBUG
   RARCH_LOG("[HTTP] >> User-Agent: %s\n", useragent ? useragent : "libretro");
#endif
   net_http_send_str(&conn->sock_state, &error, "User-Agent: ");
   if (useragent)
   {
      net_http_send_str(&conn->sock_state, &error, useragent);
   } else
   {
      net_http_send_str(&conn->sock_state, &error, "libretro");
   }
   net_http_send_str(&conn->sock_state, &error, "\r\n");

#ifdef HAVE_DEBUG
   RARCH_LOG("[HTTP] >> Connection: close\n");
   RARCH_LOG("[HTTP] >>\n");
#endif

   net_http_send_str(&conn->sock_state, &error, "Connection: close\r\n");
   net_http_send_str(&conn->sock_state, &error, "\r\n");

   if (conn->request->method && (string_is_equal(conn->request->method, "POST") ||
          string_is_equal(conn->request->method, "PUT") || string_is_equal(conn->request->method, "PATCH")))
   {
#ifdef HAVE_DEBUG
      if (conn->request->body.body_type == HTTP_REQUEST_BODY_FROM_RAW)
      {
         log_body(true, (char *)conn->request->body.value.raw.data, conn->request->body.value.raw.length);
      } else
      {
         RARCH_LOG("[HTTP] >> ## omitted ##\n");
      }
#endif

      switch (conn->request->body.body_type)
      {
         case HTTP_REQUEST_BODY_FROM_RAW:
            net_http_send_bytes(&conn->sock_state, &error, conn->request->body.value.raw.data, conn->request->body.value.raw.length);
            break;
         case HTTP_REQUEST_BODY_FROM_FILE:
         {
            uint8_t buffer[4096];
            int64_t bytes_read;
            int64_t bytes_to_read;

            bytes_to_read = conn->request->body.value.file.max_bytes > 4096 ? 4096 : conn->request->body.value.file.max_bytes;

            bytes_read = filestream_read(conn->request->body.value.file.file, (void *)buffer, bytes_to_read);
            while (bytes_read > 0)
            {
               conn->request->body.value.file.max_bytes -= bytes_to_read;
               net_http_send_bytes(&conn->sock_state, &error, buffer, bytes_read);

               bytes_to_read = conn->request->body.value.file.max_bytes > 4096 ? 4096 : conn->request->body.value.file.max_bytes;
               if (bytes_to_read == 0)
               {
                  break;
               }
               bytes_read = filestream_read(conn->request->body.value.file.file, (void *)buffer, bytes_to_read);
            }

            filestream_close(conn->request->body.value.file.file);
            conn->request->body.body_type = HTTP_REQUEST_BODY_EMPTY;

            break;
         }
         default:
            break;
      }
   }

   if (error)
      goto error;

   state             = (struct http_t*)malloc(sizeof(struct http_t));
   state->sock_state = conn->sock_state;
   state->response   = NULL;
   state->status     = -1;
   state->data       = NULL;
   state->part       = P_HEADER_TOP;
   state->bodytype   = T_FULL;
   state->error      = false;
   state->pos        = 0;
   state->len        = 0;
   state->buflen     = 512;
   state->headers    = (struct http_headers_t *)calloc(1, sizeof(struct http_headers_t));
   state->data       = (uint8_t*)malloc(state->buflen);

   if (conn->request->response_filename)
   {
      state->response_file = filestream_open(conn->request->response_filename, RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (!state->response_file)
      {
         goto error;
      }
   } else
   {
      state->response_file = NULL;
   }

   if (!state->data)
      goto error;

   return state;

error:
#ifdef HAVE_SSL
   if (conn && conn->sock_state.ssl && conn->sock_state.ssl_ctx && fd >= 0)
   {
      ssl_socket_close(conn->sock_state.ssl_ctx);
      ssl_socket_free(conn->sock_state.ssl_ctx);
      conn->sock_state.ssl_ctx = NULL;
   }
#else
   if (fd >= 0)
      socket_close(fd);
#endif
   if (state)
   {
      if (state->response)
      {
         if (state->data)
            free(state->data);
         free(state->response);
      }

      if (state->response_file)
      {
         filestream_close(state->response_file);
      }

      free(state);
   }
   return NULL;
}

int net_http_fd(struct http_t *state)
{
   if (!state)
      return -1;
   return state->sock_state.fd;
}

bool net_http_update(struct http_t *state, size_t* progress, size_t* total)
{
   ssize_t newlen = 0;

   if (!state || state->error)
      goto fail;

   if (state->part < P_BODY)
   {
      if (state->error)
         newlen = -1;
      else
      {
#ifdef HAVE_SSL
         if (state->sock_state.ssl && state->sock_state.ssl_ctx)
            newlen = ssl_socket_receive_all_nonblocking(state->sock_state.ssl_ctx, &state->error,
               state->data + state->pos,
               state->buflen - state->pos);
         else
#endif
            newlen = socket_receive_all_nonblocking(state->sock_state.fd, &state->error,
               state->data + state->pos,
               state->buflen - state->pos);
      }

      if ((state->part < P_BODY && newlen < 0) || (state->part == P_BODY && state->len > 0 && newlen < 0))
         goto fail;
      else if (state->part == P_BODY && state->len == 0 && newlen == 0)
         state->part = P_DONE;

      if (state->pos + newlen >= state->buflen - 64)
      {
         state->buflen *= 2;
         state->data = (uint8_t*)realloc(state->data, state->buflen);
      }
      state->pos += newlen;

      while (state->part < P_BODY)
      {
         uint8_t *dataend = state->data + state->pos;
         char *lineend = (char*)memchr(state->data, '\n', state->pos);

         if (!lineend)
            break;

         *lineend='\0';

         if ((uint8_t*)lineend != state->data && lineend[-1]=='\r')
            lineend[-1]='\0';

         if (state->part == P_HEADER_TOP)
         {
            if (strncmp((char*)state->data, "HTTP/1.", STRLEN_CONST("HTTP/1."))!=0)
               goto fail;
#ifdef HAVE_DEBUG
            RARCH_LOG("[HTTP] Receiving Response\n");
            RARCH_LOG("[HTTP] << %s\n", state->data);
#endif
            state->status = (int)strtoul((char*)state->data
                  + STRLEN_CONST("HTTP/1.1 "), NULL, 10);
            state->part   = P_HEADER;
         }
         else
         {
            char *header_name;
            char *header_value;
            char *colon_pos;

            colon_pos = strchr((char *)state->data, ':');
            if (colon_pos)
            {
               header_name = (char *)calloc(colon_pos - (char *)state->data + 1, sizeof(char));
               strncpy(header_name, (char *)state->data, colon_pos - (char *)state->data);
               header_value = (char *)calloc(strlen(colon_pos + 2) + 1, sizeof(char));
               strcpy(header_value, colon_pos + 2);
#ifdef HAVE_DEBUG
               RARCH_LOG("[HTTP] << %s\n", state->data);
#endif
               net_http_headers_add_value(state->headers, header_name, header_value, false);
               free(header_name);
               free(header_value);
            }

            if (!strncmp((char*)state->data, "Content-Length: ",
                     STRLEN_CONST("Content-Length: ")))
            {
               state->bodytype = T_LEN;
               state->len = strtol((char*)state->data +
                     STRLEN_CONST("Content-Length: "), NULL, 10);
               if (state->len == 0)
               {
                  state->bodytype = T_NONE;
               }
            }
            if (state->bodytype != T_NONE && string_is_equal((char*)state->data, "Transfer-Encoding: chunked"))
               state->bodytype = T_CHUNK;

            if (state->data[0]=='\0')
            {
#ifdef HAVE_DEBUG
               RARCH_LOG("[HTTP] <<\n");
#endif
               state->part = P_BODY;
               if (state->bodytype == T_CHUNK)
                  state->part = P_BODY_CHUNKLEN;
            }
         }

         memmove(state->data, lineend + 1, dataend - ((uint8_t*)lineend + 1));
         state->pos = (dataend - ((uint8_t*)lineend + 1));
      }
      if (state->part >= P_BODY)
      {
         newlen = state->pos;
         state->pos = 0;
      }
   }

   if (state->part >= P_BODY && state->part < P_DONE)
   {
      if (state->bodytype != T_NONE && !newlen)
      {
         if (state->error)
            newlen = -1;
         else
         {
#ifdef HAVE_SSL
            if (state->sock_state.ssl && state->sock_state.ssl_ctx)
               newlen = ssl_socket_receive_all_nonblocking(
                  state->sock_state.ssl_ctx,
                  &state->error,
                  state->data + state->pos,
                  state->buflen - state->pos);
            else
#endif
               newlen = socket_receive_all_nonblocking(
                  state->sock_state.fd,
                  &state->error,
                  state->data + state->pos,
                  state->buflen - state->pos);
         }

         if (newlen < 0)
         {
            if (state->bodytype == T_FULL)
            {
               state->part = P_DONE;
               state->data = (uint8_t*)realloc(state->data, state->len ? state->len : state->pos);
            }
            else
               goto fail;
            newlen=0;
         } else if (state->response_file && newlen > 0)
         {
            filestream_write(state->response_file, state->data, newlen);
         }

         if (!state->response_file && state->pos + newlen >= state->buflen - 64)
         {
            state->buflen *= 2;
            state->data = (uint8_t*)realloc(state->data, state->buflen);
         }
      }

parse_again:
      if (state->bodytype == T_CHUNK)
      {
         if (state->part == P_BODY_CHUNKLEN)
         {
            state->pos += newlen;
            if (state->pos - state->len >= 2)
            {
               /*
                * len=start of chunk including \r\n
                * pos=end of data
                */

               char *fullend = (char*)state->data + state->pos;
               char *end     = (char*)memchr(state->data + state->len + 2, '\n',
                                             state->pos - state->len - 2);

               if (end)
               {
                  size_t chunklen = strtoul((char*)state->data + state->len, NULL, 16);
                  state->pos      = state->len;
                  end++;

                  memmove(state->data+state->len, end, fullend-end);

                  state->len      = chunklen;
                  newlen          = (fullend - end);

                  /*
                     len=num bytes
                     newlen=unparsed bytes after \n
                     pos=start of chunk including \r\n
                     */

                  state->part = P_BODY;
                  if (state->len == 0)
                  {
                     state->part = P_DONE;
                     state->len  = state->pos;
                     state->data = (uint8_t*)realloc(state->data, state->len);
                  }
                  goto parse_again;
               }
            }
         }
         else if (state->part == P_BODY)
         {
            if ((size_t)newlen >= state->len)
            {
               state->pos += state->len;
               newlen     -= state->len;
               state->len  = state->pos;
               state->part = P_BODY_CHUNKLEN;
               goto parse_again;
            }
            else
            {
               state->pos += newlen;
               state->len -= newlen;
            }
         }
      }
      else if (state->bodytype == T_NONE)
      {
         state->part = P_DONE;
      }
      else
      {
         state->pos += newlen;

         if (state->pos == state->len)
         {
            state->part = P_DONE;
            state->data = (uint8_t*)realloc(state->data, state->len);

            if (state->response_file)
            {
               filestream_write(state->response_file, state->data, state->len);
            }
         }
         if (state->len > 0 && state->pos > state->len)
            goto fail;
      }
   }

   if (state->part == P_DONE)
   {
      if (state->bodytype != T_NONE && state->len == 0)
      {
         state->len = state->pos;
         state->error = false;
      }
#ifdef HAVE_DEBUG

      if (state->response_file)
      {
         filestream_close(state->response_file);
      } else
      {
         if (state->bodytype != T_NONE)
         {
            log_body(false, (char *)state->data, state->len);
         }
      }
#endif
   }

   if (progress)
      *progress = state->pos;

   if (total)
   {
      if (state->bodytype == T_LEN)
         *total=state->len;
      else
         *total=0;
   }

   return (state->part == P_DONE);

fail:
   if (state)
   {
      state->error  = true;
      state->part   = P_ERROR;
      state->status = -1;
   }

   return true;
}

struct http_response_t *net_http_get_response(struct http_t *state)
{
   if (state && (state->error || state->part == P_DONE))
   {
      if (state->response)
         return state->response;
      else
      {
         state->response = (struct http_response_t *)calloc(1, sizeof(struct http_response_t));
         state->response->status   = state->status;
         state->response->error    = state->error;
         state->response->headers  = state->headers;
         state->response->data     = state->data;
         state->response->data_len = state->len;

         state->data    = NULL;
         state->len     = 0;
         state->headers = NULL;

         return state->response;
      }
   }

   return NULL;
}

int net_http_response_get_status(struct http_response_t *response)
{
   if (!response)
      return -1;
   return response->status;
}

char *net_http_response_get_header_first_value(struct http_response_t *response, const char *name)
{
   if (!response || !response->headers)
      return NULL;
   return net_http_headers_get_first_value(response->headers, name);
}

char **net_http_response_get_header_values(struct http_response_t *response, const char *name, size_t *count)
{
   if (!response || !response->headers)
      return NULL;
   return net_http_headers_get_values(response->headers, name, count);
}

uint8_t* net_http_response_get_data(struct http_response_t *response, size_t* len, bool accept_error)
{
   if (!response)
   {
      if (len)
         *len = 0;
      return NULL;
   }

   if (!accept_error && net_http_response_is_error(response))
   {
      if (len)
         *len = 0;
      return NULL;
   }

   if (len)
      *len = response->data_len;

   return response->data;
}

void net_http_response_release_data(struct http_response_t *response)
{
   response->data = NULL;
   response->data_len = 0;
}

void net_http_response_free(struct http_response_t *response)
{
   if (!response)
      return;

   if (response->headers)
      net_http_headers_free(response->headers);
   if (response->data)
      free(response->data);

   free(response);
}

void net_http_delete(struct http_t *state)
{
   if (!state)
      return;

   if (state->sock_state.fd >= 0)
   {
      socket_close(state->sock_state.fd);
#ifdef HAVE_SSL
      if (state->sock_state.ssl && state->sock_state.ssl_ctx)
      {
         ssl_socket_free(state->sock_state.ssl_ctx);
         state->sock_state.ssl_ctx = NULL;
      }
#endif
   }

   if (state->data)
   {
      free(state->data);
   }

   if (state->headers)
   {
      net_http_headers_free(state->headers);
      free(state->headers);
   }

   free(state);
}

bool net_http_response_is_error(struct http_response_t *response)
{
   return (!response || response->error || response->status<200 || response->status>299);
}
