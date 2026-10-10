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

/* The MCP server: the command interface over the Model Context
 * Protocol's Streamable HTTP transport, for AI assistants. It is one
 * more command interface beside UDP and stdin, polled with them once
 * per input poll: the tools it offers are the commands in command.h,
 * listed from the same rows HELP prints, and a tool call runs the
 * command through the same handlers.
 *
 * Both protocol eras are answered. 2026-07-28 is stateless: each
 * request names its protocol version in params._meta, which the
 * MCP-Protocol-Version header must match, and server/discover
 * describes the server. Clients of 2025-11-25 and earlier open with
 * initialize; no session is kept, which those revisions allow.
 *
 * Nothing here waits. The listening socket and every connection are
 * non-blocking; a request that has not fully arrived, or a response
 * that has not fully left, is carried to the next poll. One request is
 * answered at a time; a command that answers later (PLAY_REPLAY_SLOT)
 * holds its connection open through reply_dest/reply_to until it does,
 * or until MCP_TIMEOUT_US.
 *
 * Access: a bearer token on every request, an Origin header - when a
 * browser sends one - naming this machine (DNS rebinding), and the
 * address to listen on, 127.0.0.1 unless the user chooses otherwise. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <formats/rjson.h>
#include <features/features_cpu.h>

#include "mcp_server.h"
#include "../command.h"
#include "../verbosity.h"
#include "../version.h"

#define MCP_MAX_CONNS      4
#define MCP_MAX_REQUEST    65536
#define MCP_TIMEOUT_US     10000000
#define MCP_TOOLS_TTL_MS   3600000

/* a literal, its length from the compiler rather than counted by hand */
#define MCP_RAW(w, lit)    rjsonwriter_raw((w), (lit), (int)(sizeof(lit) - 1))

/* Protocol versions answered, newest first. The first is the
 * stateless revision; the rest open with initialize. */
static const char *const mcp_versions[] = {
   "2026-07-28", "2025-11-25", "2025-06-18", "2025-03-26"
};
#define MCP_MODERN_VERSION "2026-07-28"

/* JSON-RPC and MCP error codes */
#define MCP_ERR_PARSE            -32700
#define MCP_ERR_INVALID_REQUEST  -32600
#define MCP_ERR_METHOD           -32601
#define MCP_ERR_PARAMS           -32602
#define MCP_ERR_HEADER_MISMATCH  -32020
#define MCP_ERR_VERSION          -32022

enum mcp_conn_state
{
   MCP_CONN_FREE = 0,
   MCP_CONN_READING,     /* request arriving */
   MCP_CONN_WAITING,     /* a command owes the reply */
   MCP_CONN_WRITING      /* response leaving */
};

struct mcp_conn
{
   char    *buf;         /* request in, then response out */
   size_t   len;         /* bytes held */
   size_t   sent;        /* bytes of the response written */
   retro_time_t since;   /* last progress, for the timeout */
   int      fd;
   unsigned serial;      /* tells a reused slot from the one a reply was owed to */
   enum mcp_conn_state state;
   /* for a reply owed by a command: the request it answers */
   char     id[64];      /* raw JSON id */
   bool     modern;
};

/* What a request says, picked out of its JSON body. */
struct mcp_request
{
   char id[64];          /* raw JSON: a quoted string or a number */
   char method[64];
   char name[64];        /* params.name */
   char version[16];     /* params._meta protocolVersion */
   char init_version[16];/* params.protocolVersion (initialize) */
   char *argument;       /* params.arguments.argument, heap */
   bool has_id;
   bool is_batch;
   bool has_meta;
   bool has_caps;        /* params._meta clientCapabilities */
};

typedef struct
{
   struct mcp_conn conns[MCP_MAX_CONNS];
   char    *reply;       /* what the running command replied */
   size_t   reply_len;
   size_t   reply_cap;
   int      listen_fd;
   int      current;     /* connection whose request is running, or -1 */
   unsigned serials;
   char     token[128];
} mcp_server_t;

/* The destination a command owing its reply is handed. */
struct mcp_owed
{
   int      conn;
   unsigned serial;
};

/* Compares without an early exit, so the time taken says nothing
 * about where a guessed token first differs. */
static bool mcp_equal_ct(const char *a, const char *b, size_t len)
{
   unsigned char diff = 0;
   size_t        i;
   for (i = 0; i < len; i++)
      diff |= (unsigned char)(a[i] ^ b[i]);
   return diff == 0;
}

/* The first @n characters of @a and @b match, ignoring ASCII case. */
static bool mcp_prefix_nocase(const char *a, const char *b, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      char x = a[i], y = b[i];
      if (x >= 'A' && x <= 'Z')
         x = (char)(x - 'A' + 'a');
      if (y >= 'A' && y <= 'Z')
         y = (char)(y - 'A' + 'a');
      if (x != y || !x)
         return false;
   }
   return true;
}

static void mcp_wipe(void *p, size_t len)
{
   volatile unsigned char *v = (volatile unsigned char*)p;
   while (len--)
      *v++ = 0;
}

/* ------------------------------------------------------------------ */
/* Responses                                                           */

static void mcp_conn_close(struct mcp_conn *c)
{
   if (c->fd >= 0)
      socket_close(c->fd);
   free(c->buf);
   memset(c, 0, sizeof(*c));
   c->fd = -1;
}

/* Replaces the request in @c with an HTTP response and starts it
 * leaving. @body may be NULL for none. */
static void mcp_respond(struct mcp_conn *c, int status, const char *reason,
      const char *extra_headers, const char *body, size_t body_len)
{
   char   head[512];
   char  *out;
   int    n = snprintf(head, sizeof(head),
         "HTTP/1.1 %d %s\r\n"
         "Content-Length: %u\r\n"
         "%s"
         "%s"
         "Cache-Control: no-store\r\n"
         "Connection: close\r\n\r\n",
         status, reason, (unsigned)body_len,
         body ? "Content-Type: application/json\r\n" : "",
         extra_headers ? extra_headers : "");
   if (n < 0 || (size_t)n >= sizeof(head)
         || !(out = (char*)malloc((size_t)n + body_len)))
   {
      mcp_conn_close(c);
      return;
   }
   memcpy(out, head, (size_t)n);
   if (body_len)
      memcpy(out + n, body, body_len);
   free(c->buf);
   c->buf   = out;
   c->len   = (size_t)n + body_len;
   c->sent  = 0;
   c->state = MCP_CONN_WRITING;
   c->since = cpu_features_get_time_usec();
}

static void mcp_respond_writer(struct mcp_conn *c, rjsonwriter_t *w)
{
   int   len  = 0;
   char *body = rjsonwriter_get_memory_buffer(w, &len);
   mcp_respond(c, 200, "OK", NULL, body, body ? (size_t)len : 0);
   rjsonwriter_free(w);
}

/* {"jsonrpc":"2.0","id":<id>, */
static rjsonwriter_t *mcp_envelope(const char *id)
{
   rjsonwriter_t *w = rjsonwriter_open_memory();
   if (!w)
      return NULL;
   MCP_RAW(w, "{\"jsonrpc\":\"2.0\",\"id\":");
   rjsonwriter_raw(w, id && *id ? id : "null", (int)strlen(id && *id ? id : "null"));
   MCP_RAW(w, ",");
   return w;
}

static void mcp_error(struct mcp_conn *c, const char *id, int code,
      const char *message, bool list_versions)
{
   unsigned i;
   rjsonwriter_t *w = mcp_envelope(id);
   if (!w)
   {
      mcp_conn_close(c);
      return;
   }
   rjsonwriter_rawf(w, "\"error\":{\"code\":%d,\"message\":", code);
   rjsonwriter_add_string(w, message);
   if (list_versions)
   {
      MCP_RAW(w, ",\"data\":{\"supported\":[");
      for (i = 0; i < sizeof(mcp_versions) / sizeof(mcp_versions[0]); i++)
      {
         if (i)
            MCP_RAW(w, ",");
         rjsonwriter_add_string(w, mcp_versions[i]);
      }
      MCP_RAW(w, "]}");
   }
   MCP_RAW(w, "}}");
   /* HeaderMismatch is a 400; every other error is an answer */
   if (code == MCP_ERR_HEADER_MISMATCH)
   {
      int   len  = 0;
      char *body = rjsonwriter_get_memory_buffer(w, &len);
      mcp_respond(c, 400, "Bad Request", NULL, body, body ? (size_t)len : 0);
      rjsonwriter_free(w);
      return;
   }
   mcp_respond_writer(c, w);
}

/* The result's closing: resultType and the server's identity in _meta
 * in the stateless era, then the envelope's end. */
static void mcp_result_end(rjsonwriter_t *w, bool modern)
{
   if (modern)
      MCP_RAW(w, ",\"resultType\":\"complete\",\"_meta\":"
            "{\"io.modelcontextprotocol/serverInfo\":"
            "{\"name\":\"RetroArch\",\"version\":\"" PACKAGE_VERSION "\"}}");
   MCP_RAW(w, "}}");
}

/* ------------------------------------------------------------------ */
/* Tools: the command tables                                           */

static void mcp_tool(rjsonwriter_t *w, const char *name, const char *desc,
      const char *arg_desc, unsigned flags)
{
   bool has_arg  = arg_desc && !string_is_equal(arg_desc, "No argument");
   bool optional = has_arg && arg_desc[0] == '[';
   bool ro       = (flags & CMD_INFO_READ_ONLY)   != 0;
   bool de       = (flags & CMD_INFO_DESTRUCTIVE) != 0;

   MCP_RAW(w, "{\"name\":");
   rjsonwriter_add_string(w, name);
   MCP_RAW(w, ",\"description\":");
   rjsonwriter_add_string(w, desc ? desc : "");
   MCP_RAW(w, ",\"inputSchema\":{\"type\":\"object\",\"properties\":{");
   if (has_arg)
   {
      MCP_RAW(w, "\"argument\":{\"type\":\"string\",\"description\":");
      rjsonwriter_add_string(w, arg_desc);
      MCP_RAW(w, "}");
   }
   MCP_RAW(w, "}");
   if (has_arg && !optional)
      MCP_RAW(w, ",\"required\":[\"argument\"]");
   MCP_RAW(w, ",\"additionalProperties\":false}");
   rjsonwriter_rawf(w, ",\"annotations\":{\"readOnlyHint\":%s,"
         "\"destructiveHint\":%s,\"openWorldHint\":false}}",
         ro ? "true" : "false", de ? "true" : "false");
}

static void mcp_tools_list(struct mcp_conn *c, const struct mcp_request *r,
      bool modern)
{
   size_t n, i;
   bool   first = true;
   const struct cmd_action_map *acts = command_action_list(&n);
   rjsonwriter_t *w = mcp_envelope(r->id);
   if (!w)
   {
      mcp_conn_close(c);
      return;
   }
   MCP_RAW(w, "\"result\":{\"tools\":[");
   for (i = 0; i < n; i++)
   {
      if (acts[i].flags & CMD_INFO_NO_TOOL)
         continue;
      if (!first)
         MCP_RAW(w, ",");
      first = false;
      mcp_tool(w, acts[i].str, acts[i].desc, acts[i].arg_desc, acts[i].flags);
   }
   {
      const struct cmd_map *keys = command_hotkey_list(&n);
      for (i = 0; i < n; i++)
      {
         if (keys[i].flags & CMD_INFO_NO_TOOL)
            continue;
         if (!first)
            MCP_RAW(w, ",");
         first = false;
         mcp_tool(w, keys[i].str, keys[i].desc, NULL, keys[i].flags);
      }
   }
   MCP_RAW(w, "]");
   /* the list is fixed for this build: cacheable by anyone */
   if (modern)
      rjsonwriter_rawf(w, ",\"ttlMs\":%d,\"cacheScope\":\"public\"",
            MCP_TOOLS_TTL_MS);
   mcp_result_end(w, modern);
   mcp_respond_writer(c, w);
}

static bool mcp_tool_known(const char *name)
{
   size_t n, i;
   const struct cmd_action_map *acts = command_action_list(&n);
   const struct cmd_map        *keys;
   for (i = 0; i < n; i++)
      if (     string_is_equal(acts[i].str, name)
            && !(acts[i].flags & CMD_INFO_NO_TOOL))
         return true;
   keys = command_hotkey_list(&n);
   for (i = 0; i < n; i++)
      if (     string_is_equal(keys[i].str, name)
            && !(keys[i].flags & CMD_INFO_NO_TOOL))
         return true;
   return false;
}

/* {"content":[{"type":"text","text":<text>}],"isError":<b>} */
static void mcp_tool_result(struct mcp_conn *c, const char *id, bool modern,
      const char *text, size_t text_len, bool is_error)
{
   rjsonwriter_t *w = mcp_envelope(id);
   if (!w)
   {
      mcp_conn_close(c);
      return;
   }
   MCP_RAW(w, "\"result\":{\"content\":[{\"type\":\"text\",\"text\":");
   rjsonwriter_add_string_len(w, text, (int)text_len);
   rjsonwriter_rawf(w, "}],\"isError\":%s", is_error ? "true" : "false");
   mcp_result_end(w, modern);
   mcp_respond_writer(c, w);
}

/* ------------------------------------------------------------------ */
/* The command interface                                               */

static void mcp_reply(command_t *cmd, const char *data, size_t len)
{
   mcp_server_t *mcp = (mcp_server_t*)cmd->userptr;
   if (mcp->reply_len + len + 1 > mcp->reply_cap)
   {
      size_t cap = (mcp->reply_len + len + 1) * 2;
      char  *nb  = (char*)realloc(mcp->reply, cap);
      if (!nb)
         return;
      mcp->reply     = nb;
      mcp->reply_cap = cap;
   }
   memcpy(mcp->reply + mcp->reply_len, data, len);
   mcp->reply_len += len;
   mcp->reply[mcp->reply_len] = '\0';
}

/* A command that answers after it returns asks where to; it is the
 * connection whose request is running. */
static void *mcp_reply_dest(command_t *cmd)
{
   mcp_server_t    *mcp = (mcp_server_t*)cmd->userptr;
   struct mcp_owed *o;
   if (mcp->current < 0 || !(o = (struct mcp_owed*)malloc(sizeof(*o))))
      return NULL;
   o->conn   = mcp->current;
   o->serial = mcp->conns[mcp->current].serial;
   mcp->conns[mcp->current].state = MCP_CONN_WAITING;
   return o;
}

static void mcp_reply_to(command_t *cmd, void *dest, const char *data,
      size_t len)
{
   mcp_server_t    *mcp = (mcp_server_t*)cmd->userptr;
   struct mcp_owed *o   = (struct mcp_owed*)dest;
   struct mcp_conn *c;
   if (!o || o->conn < 0 || o->conn >= MCP_MAX_CONNS)
      return;
   c = &mcp->conns[o->conn];
   /* the connection timed out or was reused meanwhile */
   if (c->state != MCP_CONN_WAITING || c->serial != o->serial)
      return;
   mcp_tool_result(c, c->id, c->modern, data, len, cmd->error);
   cmd->error = false;
}

/* A reply carrying an image, SCREENSHOT's: the picture, then the text
 * (its path) - an MCP client can see what is on the screen without
 * reaching the file. */
static void mcp_reply_image_to(command_t *cmd, void *dest, const char *text,
      size_t len, const char *mime, const char *image, size_t image_len)
{
   mcp_server_t    *mcp = (mcp_server_t*)cmd->userptr;
   struct mcp_owed *o   = (struct mcp_owed*)dest;
   struct mcp_conn *c;
   rjsonwriter_t   *w;
   if (!o || o->conn < 0 || o->conn >= MCP_MAX_CONNS)
      return;
   c = &mcp->conns[o->conn];
   /* the connection timed out or was reused meanwhile */
   if (c->state != MCP_CONN_WAITING || c->serial != o->serial)
      return;
   if (!(w = mcp_envelope(c->id)))
   {
      mcp_conn_close(c);
      return;
   }
   MCP_RAW(w, "\"result\":{\"content\":[{\"type\":\"image\",\"data\":");
   rjsonwriter_add_string_len(w, image, (int)image_len);
   MCP_RAW(w, ",\"mimeType\":");
   rjsonwriter_add_string(w, mime);
   MCP_RAW(w, "},{\"type\":\"text\",\"text\":");
   rjsonwriter_add_string_len(w, text, (int)len);
   MCP_RAW(w, "}],\"isError\":false");
   mcp_result_end(w, c->modern);
   mcp_respond_writer(c, w);
}

/* ------------------------------------------------------------------ */
/* Requests                                                            */

/* Copies the current string token into @dst (truncating). */
static void mcp_take(rjson_t *j, char *dst, size_t size)
{
   size_t      len = 0;
   const char *s   = rjson_get_string(j, &len);
   if (len >= size)
      len = size - 1;
   memcpy(dst, s, len);
   dst[len] = '\0';
}

/* Picks the fields this server reads out of a JSON-RPC request. Keys
 * are matched by their position: depth 1 is the request, depth 2
 * params, depth 3 params.arguments / params._meta. */
static int mcp_parse(const char *body, size_t len, struct mcp_request *r)
{
   char key[4][48];
   int  expect_key[5] = { 0, 1, 0, 0, 0 };
   unsigned depth     = 0;
   rjson_t *j         = rjson_open_buffer(body, len);
   int      err       = 0;

   if (!j)
      return MCP_ERR_PARSE;
   rjson_set_max_depth(j, 32);
   memset(key, 0, sizeof(key));

   for (;;)
   {
      enum rjson_type t = rjson_next(j);
      bool is_key;

      if (t == RJSON_DONE)
         break;
      if (t == RJSON_ERROR)
      {
         err = MCP_ERR_PARSE;
         break;
      }
      if (depth == 0)
      {
         if (t == RJSON_ARRAY)
            r->is_batch = true;
         if (t != RJSON_OBJECT)
         {
            err = MCP_ERR_INVALID_REQUEST;
            break;
         }
         depth = 1;
         expect_key[1] = 1;
         continue;
      }
      if (t == RJSON_OBJECT_END || t == RJSON_ARRAY_END)
      {
         if (--depth == 0)
            break;
         continue;
      }
      is_key = rjson_get_context_type(j) == RJSON_OBJECT
            && (rjson_get_context_count(j) & 1);
      if (is_key && t == RJSON_STRING && depth <= 4)
      {
         mcp_take(j, key[depth - 1], sizeof(key[0]));
         continue;
      }
      /* a value: where it sits */
      if (depth == 1)
      {
         if (string_is_equal(key[0], "id") && (t == RJSON_STRING || t == RJSON_NUMBER))
         {
            size_t      l = 0;
            const char *s = rjson_get_string(j, &l);
            r->has_id = true;
            if (t == RJSON_STRING)
            {
               /* re-quote: the id goes back verbatim */
               rjsonwriter_t *w = rjsonwriter_open_memory();
               if (w)
               {
                  int   wl  = 0;
                  char *out;
                  rjsonwriter_add_string_len(w, s, (int)l);
                  out = rjsonwriter_get_memory_buffer(w, &wl);
                  if (out && wl > 0 && (size_t)wl < sizeof(r->id))
                  {
                     memcpy(r->id, out, (size_t)wl);
                     r->id[wl] = '\0';
                  }
                  rjsonwriter_free(w);
               }
            }
            else if (l < sizeof(r->id))
            {
               memcpy(r->id, s, l);
               r->id[l] = '\0';
            }
         }
         else if (string_is_equal(key[0], "method") && t == RJSON_STRING)
            mcp_take(j, r->method, sizeof(r->method));
      }
      else if (depth == 2 && string_is_equal(key[0], "params"))
      {
         if (string_is_equal(key[1], "name") && t == RJSON_STRING)
            mcp_take(j, r->name, sizeof(r->name));
         else if (string_is_equal(key[1], "protocolVersion") && t == RJSON_STRING)
            mcp_take(j, r->init_version, sizeof(r->init_version));
      }
      else if (depth == 3 && string_is_equal(key[0], "params"))
      {
         if (string_is_equal(key[1], "arguments")
               && string_is_equal(key[2], "argument") && t == RJSON_STRING)
         {
            size_t      l = 0;
            const char *s = rjson_get_string(j, &l);
            free(r->argument);
            if ((r->argument = (char*)malloc(l + 1)))
            {
               memcpy(r->argument, s, l);
               r->argument[l] = '\0';
            }
         }
         else if (string_is_equal(key[1], "_meta"))
         {
            r->has_meta = true;
            if (string_is_equal(key[2], "io.modelcontextprotocol/protocolVersion")
                  && t == RJSON_STRING)
               mcp_take(j, r->version, sizeof(r->version));
         }
      }
      else if (depth == 4 && string_is_equal(key[0], "params")
            && string_is_equal(key[1], "_meta")
            && string_is_equal(key[2], "io.modelcontextprotocol/clientCapabilities"))
         r->has_caps = true;
      if (t == RJSON_OBJECT || t == RJSON_ARRAY)
      {
         if (depth == 3 && string_is_equal(key[0], "params")
               && string_is_equal(key[1], "_meta"))
         {
            r->has_meta = true;
            if (string_is_equal(key[2], "io.modelcontextprotocol/clientCapabilities"))
               r->has_caps = true;
         }
         if (depth == 2 && string_is_equal(key[0], "params")
               && string_is_equal(key[1], "_meta"))
            r->has_meta = true;
         depth++;
         if (depth < 5)
            key[depth - 1][0] = '\0';
      }
   }
   (void)expect_key;
   rjson_free(j);
   return err;
}

static bool mcp_version_known(const char *v)
{
   unsigned i;
   for (i = 0; i < sizeof(mcp_versions) / sizeof(mcp_versions[0]); i++)
      if (string_is_equal(v, mcp_versions[i]))
         return true;
   return false;
}

/* The value of header @name in the header block @head, or NULL; the
 * block is edited to terminate it. Names match without case. */
static char *mcp_header(char *head, const char *name)
{
   size_t nl = strlen(name);
   char  *p  = strstr(head, "\r\n");
   while (p && p[2] && !(p[2] == '\r' && p[3] == '\n'))
   {
      char *line = p + 2;
      char *eol  = strstr(line, "\r\n");
      if (!eol)
         return NULL;
      if ((size_t)(eol - line) > nl && line[nl] == ':'
            && mcp_prefix_nocase(line, name, nl))
      {
         char *v = line + nl + 1;
         while (*v == ' ' || *v == '\t')
            v++;
         /* terminate a copy-free view: the caller reads before the
          * next lookup, and every lookup scans for "\r\n" afresh */
         return v;
      }
      p = eol;
   }
   return NULL;
}

/* Length of a header value up to its "\r\n". */
static size_t mcp_header_len(const char *v)
{
   const char *e = strstr(v, "\r\n");
   return e ? (size_t)(e - v) : strlen(v);
}

static bool mcp_header_is(const char *v, const char *want)
{
   size_t l = strlen(want);
   return v && mcp_header_len(v) == l && strncmp(v, want, l) == 0;
}

/* An Origin naming this machine: http(s)://localhost, 127.0.0.1 or
 * [::1], any port. A browser page elsewhere must not reach the server
 * by pointing a host name at 127.0.0.1. */
static bool mcp_origin_ok(const char *v)
{
   size_t      l = mcp_header_len(v);
   const char *h;
   size_t      hl;
   static const char *const hosts[] = { "localhost", "127.0.0.1", "[::1]" };
   unsigned    i;
   if (l > 7 && strncmp(v, "http://", 7) == 0)
      h = v + 7;
   else if (l > 8 && strncmp(v, "https://", 8) == 0)
      h = v + 8;
   else
      return false;
   hl = l - (size_t)(h - v);
   for (i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++)
   {
      size_t n = strlen(hosts[i]);
      if (hl >= n && strncmp(h, hosts[i], n) == 0
            && (hl == n || h[n] == ':'))
         return true;
   }
   return false;
}

static bool mcp_token_ok(const mcp_server_t *mcp, const char *v)
{
   size_t tl = strlen(mcp->token);
   size_t l;
   if (!v || !tl)
      return false;
   l = mcp_header_len(v);
   if (l != 7 + tl || !mcp_prefix_nocase(v, "Bearer ", 7))
      return false;
   return mcp_equal_ct(v + 7, mcp->token, tl);
}

static void mcp_handle(mcp_server_t *mcp, command_t *cmd, int ci,
      char *head, const char *body, size_t body_len)
{
   struct mcp_conn   *c = &mcp->conns[ci];
   struct mcp_request r;
   char *hv, *hm, *hn;
   bool  modern;
   int   err;

   memset(&r, 0, sizeof(r));

   if ((err = mcp_parse(body, body_len, &r)))
   {
      mcp_error(c, NULL, r.is_batch ? MCP_ERR_INVALID_REQUEST : err,
            r.is_batch ? "Batch requests are not supported."
                       : "The request is not valid JSON-RPC.", false);
      goto end;
   }
   if (!*r.method)
   {
      mcp_error(c, r.id, MCP_ERR_INVALID_REQUEST, "No method.", false);
      goto end;
   }
   /* notifications (no id) are acknowledged and otherwise ignored */
   if (!r.has_id)
   {
      mcp_respond(c, 202, "Accepted", NULL, NULL, 0);
      goto end;
   }

   hv     = mcp_header(head, "MCP-Protocol-Version");
   hm     = mcp_header(head, "Mcp-Method");
   hn     = mcp_header(head, "Mcp-Name");
   modern = *r.version != '\0';

   if (modern)
   {
      /* the stateless era: the version travels with every request */
      if (!mcp_version_known(r.version)
            || !string_is_equal(r.version, MCP_MODERN_VERSION))
      {
         mcp_error(c, r.id, MCP_ERR_VERSION,
               "Unsupported protocol version.", true);
         goto end;
      }
      if (     (hv && !mcp_header_is(hv, r.version))
            || (hm && !mcp_header_is(hm, r.method))
            || (hn && *r.name && !mcp_header_is(hn, r.name)))
      {
         mcp_error(c, r.id, MCP_ERR_HEADER_MISMATCH,
               "HTTP headers do not match the request body.", false);
         goto end;
      }
   }

   if (string_is_equal(r.method, "server/discover"))
   {
      unsigned i;
      rjsonwriter_t *w = mcp_envelope(r.id);
      if (!w)
      {
         mcp_conn_close(c);
         goto end;
      }
      MCP_RAW(w, "\"result\":{\"supportedVersions\":[");
      for (i = 0; i < sizeof(mcp_versions) / sizeof(mcp_versions[0]); i++)
      {
         if (i)
            MCP_RAW(w, ",");
         rjsonwriter_add_string(w, mcp_versions[i]);
      }
      rjsonwriter_rawf(w, "],\"capabilities\":{\"tools\":{\"listChanged\":false}},"
            "\"instructions\":\"Controls a running RetroArch: each tool is a "
            "RetroArch command. Tools marked destructive can lose unsaved "
            "progress or load code.\",\"ttlMs\":%d,\"cacheScope\":\"public\"",
            MCP_TOOLS_TTL_MS);
      mcp_result_end(w, true);
      mcp_respond_writer(c, w);
   }
   else if (string_is_equal(r.method, "initialize"))
   {
      /* the handshake of 2025-11-25 and earlier: answer with the
       * client's version when it is one of ours, else the newest of
       * those that handshake */
      const char    *v = mcp_version_known(r.init_version)
            && !string_is_equal(r.init_version, MCP_MODERN_VERSION)
         ? r.init_version : mcp_versions[1];
      rjsonwriter_t *w = mcp_envelope(r.id);
      if (!w)
      {
         mcp_conn_close(c);
         goto end;
      }
      MCP_RAW(w, "\"result\":{\"protocolVersion\":");
      rjsonwriter_add_string(w, v);
      MCP_RAW(w, ",\"capabilities\":{\"tools\":{\"listChanged\":false}},"
            "\"serverInfo\":{\"name\":\"RetroArch\",\"version\":\""
            PACKAGE_VERSION "\"}");
      mcp_result_end(w, false);
      mcp_respond_writer(c, w);
   }
   else if (string_is_equal(r.method, "ping") && !modern)
   {
      rjsonwriter_t *w = mcp_envelope(r.id);
      if (!w)
      {
         mcp_conn_close(c);
         goto end;
      }
      MCP_RAW(w, "\"result\":{}}");
      mcp_respond_writer(c, w);
   }
   else if (string_is_equal(r.method, "tools/list"))
      mcp_tools_list(c, &r, modern);
   else if (string_is_equal(r.method, "tools/call"))
   {
      bool ok;
      if (!mcp_tool_known(r.name))
      {
         mcp_error(c, r.id, MCP_ERR_PARAMS, "Unknown tool.", false);
         goto end;
      }
      /* the command runs now; what it replies is the result */
      mcp->reply_len = 0;
      mcp->current   = ci;
      strlcpy(c->id, r.id, sizeof(c->id));
      c->modern      = modern;
      ok = command_run(cmd, r.name, r.argument);
      mcp->current   = -1;
      if (c->state == MCP_CONN_WAITING)
         goto end;                         /* answered by reply_to */
      if (mcp->reply_len)
         mcp_tool_result(c, r.id, modern, mcp->reply, mcp->reply_len,
               cmd->error || !ok);
      else
         mcp_tool_result(c, r.id, modern, ok ? "Done." : "The command failed.",
               ok ? 5 : 19, !ok);
   }
   else
      mcp_error(c, r.id, MCP_ERR_METHOD, "Method not found.", false);

end:
   free(r.argument);
}

/* A request has fully arrived in @c: check it as HTTP, then answer. */
static void mcp_request(mcp_server_t *mcp, command_t *cmd, int ci)
{
   struct mcp_conn *c    = &mcp->conns[ci];
   char            *head = c->buf;
   char            *end  = strstr(head, "\r\n\r\n");
   char            *v;
   size_t           body_len;

   /* the headers end is known here; terminate the block after it */
   end[2] = '\0';

   if (strncmp(head, "POST ", 5) != 0)
   {
      mcp_respond(c, 405, "Method Not Allowed", "Allow: POST\r\n", NULL, 0);
      return;
   }
   if (strncmp(head + 5, "/mcp ", 5) != 0 && strncmp(head + 5, "/ ", 2) != 0)
   {
      mcp_respond(c, 404, "Not Found", NULL, NULL, 0);
      return;
   }
   if ((v = mcp_header(head, "Origin")) && !mcp_origin_ok(v))
   {
      mcp_respond(c, 403, "Forbidden", NULL, NULL, 0);
      return;
   }
   if (!mcp_token_ok(mcp, mcp_header(head, "Authorization")))
   {
      mcp_respond(c, 401, "Unauthorized",
            "WWW-Authenticate: Bearer realm=\"RetroArch\"\r\n", NULL, 0);
      return;
   }
   body_len = c->len - (size_t)(end + 4 - c->buf);
   mcp_handle(mcp, cmd, ci, head, end + 4, body_len);
}

/* How much of the body the headers promise, or -1 when the request is
 * not complete yet; -2 when it can never be. */
static long mcp_want(struct mcp_conn *c)
{
   char  *end;
   char  *cl;
   char   save;
   long   n;
   if (!c->len || !(end = strstr(c->buf, "\r\n\r\n")))
      return c->len >= MCP_MAX_REQUEST ? -2 : -1;
   save   = end[2];
   end[2] = '\0';
   cl     = mcp_header(c->buf, "Content-Length");
   n      = cl ? strtol(cl, NULL, 10) : 0;
   end[2] = save;
   if (n < 0 || n > MCP_MAX_REQUEST)
      return -2;
   if (c->len - (size_t)(end + 4 - c->buf) < (size_t)n)
      return -1;
   /* the body ends where Content-Length says */
   c->len = (size_t)(end + 4 - c->buf) + (size_t)n;
   c->buf[c->len] = '\0';
   return n;
}

static void mcp_poll(command_t *cmd)
{
   mcp_server_t *mcp = (mcp_server_t*)cmd->userptr;
   retro_time_t  now = cpu_features_get_time_usec();
   int i;

   /* new connections, into free slots */
   for (;;)
   {
      int fd;
      for (i = 0; i < MCP_MAX_CONNS; i++)
         if (mcp->conns[i].state == MCP_CONN_FREE)
            break;
      if (i == MCP_MAX_CONNS)
         break;
      if ((fd = (int)accept(mcp->listen_fd, NULL, NULL)) < 0)
         break;
      if (!socket_nonblock(fd)
            || !(mcp->conns[i].buf = (char*)malloc(MCP_MAX_REQUEST + 1)))
      {
         socket_close(fd);
         break;
      }
      mcp->conns[i].fd     = fd;
      mcp->conns[i].len    = 0;
      mcp->conns[i].state  = MCP_CONN_READING;
      mcp->conns[i].serial = ++mcp->serials;
      mcp->conns[i].since  = now;
   }

   for (i = 0; i < MCP_MAX_CONNS; i++)
   {
      struct mcp_conn *c = &mcp->conns[i];
      switch (c->state)
      {
         case MCP_CONN_READING:
            {
               bool    error = false;
               ssize_t got   = socket_receive_all_nonblocking(c->fd, &error,
                     c->buf + c->len, MCP_MAX_REQUEST - c->len);
               long    want;
               if (error)
               {
                  mcp_conn_close(c);
                  break;
               }
               if (got > 0)
               {
                  c->len  += (size_t)got;
                  c->since = now;
               }
               c->buf[c->len] = '\0';
               if ((want = mcp_want(c)) == -2)
                  mcp_respond(c, 413, "Payload Too Large", NULL, NULL, 0);
               else if (want >= 0)
               {
                  mcp_request(mcp, cmd, i);
                  /* the other requests wait out the content load */
                  if (command_interfaces_held())
                     return;
               }
            }
            break;
         case MCP_CONN_WRITING:
            {
               ssize_t put = socket_send_all_nonblocking(c->fd,
                     c->buf + c->sent, c->len - c->sent, true);
               if (put < 0)
                  mcp_conn_close(c);
               else
               {
                  if (put > 0)
                     c->since = now;
                  c->sent += (size_t)put;
                  if (c->sent >= c->len)
                     mcp_conn_close(c);
               }
            }
            break;
         default:
            break;
      }
      if (c->state != MCP_CONN_FREE && now - c->since > MCP_TIMEOUT_US)
      {
         if (c->state == MCP_CONN_WAITING)
            mcp_tool_result(c, c->id, c->modern,
                  "The command did not answer in time.", 35, true);
         else
            mcp_conn_close(c);
      }
   }
}

static void mcp_destroy(command_t *cmd)
{
   mcp_server_t *mcp = (mcp_server_t*)cmd->userptr;
   int i;
   if (mcp)
   {
      for (i = 0; i < MCP_MAX_CONNS; i++)
      {
         static const char gone[] =
               "RetroArch restarted its command interfaces before the "
               "command answered.";
         struct mcp_conn *c = &mcp->conns[i];
         if (c->state == MCP_CONN_WAITING)
            mcp_tool_result(c, c->id, c->modern, gone, sizeof(gone) - 1, true);
         /* a content load tears this down in the frame of the request
          * that started it, before the next poll would send the answer */
         if (c->state == MCP_CONN_WRITING && c->sent < c->len)
            socket_send_all_nonblocking(c->fd, c->buf + c->sent,
                  c->len - c->sent, true);
         if (c->state != MCP_CONN_FREE)
            mcp_conn_close(c);
      }
      if (mcp->listen_fd >= 0)
         socket_close(mcp->listen_fd);
      mcp_wipe(mcp->token, sizeof(mcp->token));
      free(mcp->reply);
      free(mcp);
   }
   free(cmd);
}

command_t *command_mcp_new(uint16_t port, const char *bind_address,
      const char *token)
{
   struct addrinfo *res = NULL;
   command_t       *cmd;
   mcp_server_t    *mcp;
   int              fd;
   int              i;

   if (string_is_empty(token) || !network_init())
      return NULL;
   if (!(cmd = (command_t*)calloc(1, sizeof(*cmd))))
      return NULL;
   if (!(mcp = (mcp_server_t*)calloc(1, sizeof(*mcp))))
   {
      free(cmd);
      return NULL;
   }
   mcp->listen_fd = -1;
   mcp->current   = -1;
   for (i = 0; i < MCP_MAX_CONNS; i++)
      mcp->conns[i].fd = -1;
   strlcpy(mcp->token, token, sizeof(mcp->token));

   if (string_is_empty(bind_address))
      bind_address = "127.0.0.1";
   fd = socket_init((void**)&res, port, bind_address,
         SOCKET_TYPE_STREAM, AF_INET);
   if (fd < 0 || !res || !socket_bind(fd, res) || listen(fd, 8) != 0
         || !socket_nonblock(fd))
   {
      RARCH_ERR("[MCP] Could not listen on %s:%hu.\n",
            bind_address, (unsigned short)port);
      if (fd >= 0)
         socket_close(fd);
      if (res)
         freeaddrinfo_retro(res);
      free(mcp);
      free(cmd);
      return NULL;
   }
   freeaddrinfo_retro(res);
   mcp->listen_fd = fd;

   cmd->userptr    = mcp;
   cmd->poll       = mcp_poll;
   cmd->replier    = mcp_reply;
   cmd->reply_dest = mcp_reply_dest;
   cmd->reply_to   = mcp_reply_to;
   cmd->reply_image_to = mcp_reply_image_to;
   cmd->destroy    = mcp_destroy;
   cmd->structured = true;
   RARCH_LOG("[MCP] Listening on http://%s:%hu/mcp.\n",
         bind_address, (unsigned short)port);
   return cmd;
}
