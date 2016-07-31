#include <libretro.h>
#include "system.h"
#include "runloop.h"
#include "compat/zlib.h"
#include "civetweb.h"

#include <string.h>
#include <stdarg.h>

static struct mg_callbacks s_httpserver_callbacks;
static struct mg_context* s_httpserver_ctx;

/* Based on https://github.com/zeromq/rfc/blob/master/src/spec_32.c */
static void httpserver_z85_encode_inplace(Bytef* data, size_t size)
{
  static char digits[85 + 1] =
  {
    "0123456789"
    "abcdefghij"
    "klmnopqrst"
    "uvwxyzABCD"
    "EFGHIJKLMN"
    "OPQRSTUVWX"
    "YZ.-:+=^!/"
    "*?&<>()[]{"
    "}@%$#"
  };

  Bytef* source = data + size - 4;
  Bytef* dest = data + size * 5 / 4 - 5;
  uLong value;

  dest[5] = 0;

  if (source >= data)
  {
    do
    {
      value  = source[0] * 256 * 256 * 256;
      value += source[1] * 256 * 256;
      value += source[2] * 256;
      value += source[3];
      source -= 4;

      dest[4] = digits[value % 85];
      value /= 85;
      dest[3] = digits[value % 85];
      value /= 85;
      dest[2] = digits[value % 85];
      value /= 85;
      dest[1] = digits[value % 85];
      dest[0] = digits[value / 85];
      dest -= 5;

    } while (source >= data);
  }
}

static int httpserver_error(struct mg_connection* conn, unsigned code, const char* fmt, ...)
{
  const char* reason;
  char buffer[1024];
  va_list args;

  switch (code)
  {
  case 404:
    reason = "Not Found";
    break;

  case 405:
    reason = "Method Not Allowed";
    break;

  default:
    /* Send unknown codes as 500 */
    code = 500;
    reason = "Internal Server Error";
    break;
  }

  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  buffer[sizeof(buffer) - 1] = 0;

  mg_printf(conn, "HTTP/1.1 %u %s\r\nContent-Type: text/plain\r\n\r\n", code, reason);
  mg_printf(conn, "%u %s\r\n\r\n%s", code, reason, buffer);
  return 1;
}

static int httpserver_handle_get_mmaps(struct mg_connection* conn, void* cbdata)
{
  const struct mg_request_info* req = mg_get_request_info(conn);
  const char* comma = "";
  rarch_system_info_t* system;
  const struct retro_memory_map* mmaps;
  const struct retro_memory_descriptor* mmap;
  unsigned id;

  if (strcmp(req->request_method, "GET"))
  {
    return httpserver_error(conn, 405, "Unimplemented method in %s: %s", __FUNCTION__, req->request_method);
  }

  if (!runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system))
  {
    return httpserver_error(conn, 500, "Could not get system information in %s", __FUNCTION__);
  }

  mmaps = &system->mmaps;
  mmap = mmaps->descriptors;

  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
  mg_printf(conn, "[");

  for (id = 0; id < mmaps->num_descriptors; id++, mmap++, comma = ",")
  {
    mg_printf(conn, "%s", comma);

    mg_printf(conn,
      "{"
      "\"id\":%u,"
      "\"flags\":" STRING_REP_UINT64 ","
      "\"ptr\":\"%p\","
      "\"offset\":" STRING_REP_UINT64 ","
      "\"start\":" STRING_REP_UINT64 ","
      "\"select\":" STRING_REP_UINT64 ","
      "\"disconnect\":" STRING_REP_UINT64 ","
      "\"len\":" STRING_REP_UINT64 ","
      "\"addrspace\":",
      id,
      mmap->flags,
      mmap->ptr,
      mmap->offset,
      mmap->start,
      mmap->select,
      mmap->disconnect,
      mmap->len
      );

    if (mmap->addrspace)
    {
      mg_printf(conn, "\"%s\"", mmap->addrspace);
    }
    else
    {
      mg_printf(conn, "null");
    }

    mg_printf(conn, "}");
  }

  mg_printf(conn, "]");
  return 1;
}

static int httpserver_handle_get_mmap(struct mg_connection* conn, void* cbdata)
{
  static const char* hexdigits = "0123456789ABCDEF";

  const struct mg_request_info* req = mg_get_request_info(conn);
  const char* comma = "";
  rarch_system_info_t* system;
  const struct retro_memory_map* mmaps;
  const struct retro_memory_descriptor* mmap;
  unsigned id;
  uLong buflen;
  Bytef* buffer;

  if (strcmp(req->request_method, "GET"))
  {
    return httpserver_error(conn, 405, "Unimplemented method in %s: %s", __FUNCTION__, req->request_method);
  }

  if (sscanf(req->request_uri, "/mmaps/%u", &id) != 1)
  {
    return httpserver_error(conn, 500, "Malformed request in %s: %s", __FUNCTION__, req->request_uri);
  }

  if (!runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system))
  {
    return httpserver_error(conn, 500, "Could not get system information in %s", __FUNCTION__);
  }

  mmaps = &system->mmaps;

  if (id >= mmaps->num_descriptors)
  {
    return httpserver_error(conn, 404, "Invalid memory map id in %s: %u", __FUNCTION__, id);
  }

  mmap = mmaps->descriptors + id;
  buflen = compressBound(mmap->len);
  buffer = (Bytef*)malloc(((buflen + 3) / 4) * 5);

  if (buffer == NULL)
  {
    return httpserver_error(conn, 500, "Out of memory in %s", __FUNCTION__);
  }

  if (compress2(buffer, &buflen, (Bytef*)mmap->ptr, mmap->len, Z_BEST_COMPRESSION) != Z_OK)
  {
    free((void*)buffer);
    return httpserver_error(conn, 500, "Error during compression in %s", __FUNCTION__);
  }

  buffer[buflen] = 0;
  buffer[buflen + 1] = 0;
  buffer[buflen + 2] = 0;
  httpserver_z85_encode_inplace(buffer, (buflen + 3) & ~3);

  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
  mg_printf(conn,
    "{"
    "\"length\":" STRING_REP_UINT64 ","
    "\"compressedLength\":" STRING_REP_ULONG ","
    "\"bytesZ85\":\"%s\""
    "}",
    mmap->len,
    (size_t)buflen,
    (char*)buffer
  );

  free((void*)buffer);
  return 1;
}

static int httpserver_handle_mmaps(struct mg_connection* conn, void* cbdata)
{
  const struct mg_request_info* req = mg_get_request_info(conn);
  unsigned id;

  if (sscanf(req->request_uri, "/mmaps/%u", &id) == 1)
  {
    return httpserver_handle_get_mmap(conn, cbdata);
  }
  else
  {
    return httpserver_handle_get_mmaps(conn, cbdata);
  }
}

int httpserver_init(unsigned port)
{
  char str[16];
  snprintf(str, sizeof(str), "%u", port);
  str[sizeof(str) - 1] = 0;

  const char* options[] =
  {
    "listening_ports", str,
    NULL, NULL
  };

  memset(&s_httpserver_callbacks, 0, sizeof(s_httpserver_callbacks));
  s_httpserver_ctx = mg_start(&s_httpserver_callbacks, NULL, options);

  if (s_httpserver_ctx == NULL)
  {
    return -1;
  }

  mg_set_request_handler(s_httpserver_ctx, "/mmaps", httpserver_handle_mmaps, NULL);
  mg_set_request_handler(s_httpserver_ctx, "/mmaps/", httpserver_handle_mmaps, NULL);
  return 0;
}

void httpserver_destroy()
{
  mg_stop(s_httpserver_ctx);
}
