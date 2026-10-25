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
#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "mcp_server.h"
#include "../command.h"
#include "../verbosity.h"
#include "../version.h"

#define MCP_MAX_CONNS      4
#define MCP_MAX_REQUEST    65536
#define MCP_TIMEOUT_US     10000000

/* a literal, its length from the compiler rather than counted by hand */
#define MCP_RAW(w, lit)    rjsonwriter_raw((w), (lit), (int)(sizeof(lit) - 1))

/* Protocol versions answered, newest first. The first is the
 * stateless revision; the rest open with initialize. */
static const char *const mcp_versions[] = {
   "2026-07-28", "2025-11-25", "2025-06-18", "2025-03-26"
};
#define MCP_MODERN_VERSION "2026-07-28"
/* The revision that added outputSchema and structuredContent. */
#define MCP_STRUCTURED_VERSION "2025-06-18"
/* Required on stateless list results. Not cacheable: the lists are
 * compiled in and change on upgrade, with listChanged false. */
#define MCP_NO_CACHE ",\"ttlMs\":0,\"cacheScope\":\"private\""

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
   /* the tool call being answered, for the shape of its reply */
   const struct cmd_action_map *act;
   size_t   len;         /* bytes held */
   size_t   sent;        /* bytes of the response written */
   retro_time_t since;   /* last progress, for the timeout */
   int      fd;
   unsigned serial;      /* tells a reused slot from the one a reply was owed to */
   enum mcp_conn_state state;
   /* for a reply owed by a command: the request it answers */
   char     id[64];      /* raw JSON id */
   bool     modern;
   bool     stdio;       /* the stdio transport's one connection */
   bool     structured;  /* its revision takes structuredContent */
};

/* One of params.arguments, by the name the schema gave it. */
struct mcp_arg
{
   char  name[32];
   char *value;          /* heap; a number arrives as its digits */
   bool  is_number;
   bool  is_other;       /* true, false, null, an object or an array */
};

/* More than any command's parameters, plus room for a client that
 * sends one this build does not know. */
#define MCP_MAX_ARGS 6

/* What a request says, picked out of its JSON body. */
struct mcp_request
{
   char id[64];          /* raw JSON: a quoted string or a number */
   char method[64];
   char name[64];        /* params.name */
   char version[16];     /* params._meta protocolVersion */
   char init_version[16];/* params.protocolVersion (initialize) */
   char level[16];       /* params.level (logging/setLevel) */
   struct mcp_arg args[MCP_MAX_ARGS];
   unsigned num_args;
   bool has_id;
   bool is_batch;
   bool has_meta;
   bool has_caps;        /* params._meta clientCapabilities */
};

static void mcp_request_free(struct mcp_request *r)
{
   unsigned i;
   for (i = 0; i < r->num_args; i++)
      free(r->args[i].value);
   r->num_args = 0;
}

/* What the client sent for @name, or NULL. */
static const char *mcp_arg(const struct mcp_request *r, const char *name)
{
   unsigned i;
   for (i = 0; i < r->num_args; i++)
      if (string_is_equal(r->args[i].name, name))
         return r->args[i].value;
   return NULL;
}

typedef struct
{
   struct mcp_conn conns[MCP_MAX_CONNS];
   char    *reply;       /* what the running command replied */
   size_t   reply_len;
   size_t   reply_cap;
   int      listen_fd;
   int      current;     /* connection whose request is running, or -1 */
   bool     answered;    /* reply_to answered it before command_run returned */
   unsigned serials;
   char     token[128];
   bool     stdio;       /* serving stdin and stdout, not a port */
} mcp_server_t;

/* The stdio transport: newline-delimited JSON-RPC on stdin and on the
 * stdout taken by command_mcp_stdio_claim(). This outlives the server,
 * which the command interfaces are torn down and made again around
 * (a content load): a request half read and answers not yet written
 * carry over to the next one. */
#define MCP_STDIO_OUT_MAX (32 * 1024 * 1024)

static const char *const mcp_log_levels[] = {
   "debug", "info", "notice", "warning", "error", "critical", "alert",
   "emergency"
};

static struct
{
   char    *in;          /* read, up to the next newline */
   char    *out;         /* answers queued for stdout */
   char    *log;         /* the log lines read for notifications */
   size_t   in_len;
   size_t   out_len;
   size_t   out_sent;
   size_t   log_next;    /* the next log line to send */
#if defined(_WIN32)
   HANDLE   in_h;
   HANDLE   out_h;
#else
   int      out_fd;
#endif
   int      log_level;   /* index into mcp_log_levels; -1: no logging */
   bool     claimed;
   bool     eof;
   bool     discarding;  /* reading past a message too long to take */
   /* initialize agreed on a revision that takes structuredContent: the
    * stream's, not the server's, which a settings change can restart
    * while the client goes on with the tool list it was given */
   bool     agreed;
   bool     quit_sent;
} mcp_stdio;

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
   /* stdin stays open: what follows is read as the next request */
   if (c->stdio)
   {
      free(c->buf);
      c->buf   = NULL;
      c->len   = 0;
      c->sent  = 0;
      c->state = MCP_CONN_READING;
      return;
   }
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
   int    n;

   /* stdio has no status line: an answer is its body and a newline,
    * queued; a notification's acceptance is nothing at all */
   if (c->stdio)
   {
      (void)status;
      (void)reason;
      (void)extra_headers;
      if (body && mcp_stdio.out_len + body_len + 1 <= MCP_STDIO_OUT_MAX)
      {
         char *nb = (char*)realloc(mcp_stdio.out,
               mcp_stdio.out_len + body_len + 1);
         if (nb)
         {
            memcpy(nb + mcp_stdio.out_len, body, body_len);
            nb[mcp_stdio.out_len + body_len] = '\n';
            mcp_stdio.out      = nb;
            mcp_stdio.out_len += body_len + 1;
         }
      }
      mcp_conn_close(c);
      return;
   }

   n = snprintf(head, sizeof(head),
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

/* The set a parameter accepts, written "on|off", as a schema enum. */
static void mcp_schema_enum(rjsonwriter_t *w, const char *values)
{
   const char *p = values;
   MCP_RAW(w, ",\"enum\":[");
   for (;;)
   {
      const char *bar = strchr(p, '|');
      size_t      len = bar ? (size_t)(bar - p) : strlen(p);
      rjsonwriter_add_string_len(w, p, (int)len);
      if (!bar)
         break;
      MCP_RAW(w, ",");
      p = bar + 1;
   }
   MCP_RAW(w, "]");
}

/* What a reply that is rows holds, as a JSON Schema: the rows and their
 * columns, the word that says the reply was cut short, and the cursor
 * the next page is asked with where there is one. All the columns are
 * strings: each is the text's own field, which the server never reads
 * as anything else. @describe is false where an earlier tool in the
 * list described the same columns: every byte of the list is context a
 * client pays for, and five tools answer with what WATCH watches. */
/* The columns' properties, as a row's or a record's schema takes
 * them, and then the ones every row has: "required":[...]. */
static void mcp_schema_columns(rjsonwriter_t *w, const struct cmd_rows *rows,
      bool describe)
{
   const struct cmd_column *col;
   bool first = true;
   for (col = rows->columns; col && col->name; col++)
   {
      if (!first)
         MCP_RAW(w, ",");
      first = false;
      rjsonwriter_add_string(w, col->name);
      MCP_RAW(w, ":{\"type\":\"string\"");
      if (describe && col->desc)
      {
         MCP_RAW(w, ",\"description\":");
         rjsonwriter_add_string(w, col->desc);
      }
      MCP_RAW(w, "}");
   }
}

static void mcp_schema_required(rjsonwriter_t *w,
      const struct cmd_rows *rows)
{
   const struct cmd_column *col;
   bool first = true;
   MCP_RAW(w, "\"required\":[");
   for (col = rows->columns; col && col->name; col++)
   {
      if (rows->required && (size_t)(col - rows->columns) >= rows->required)
         break;
      if (!first)
         MCP_RAW(w, ",");
      first = false;
      rjsonwriter_add_string(w, col->name);
   }
   MCP_RAW(w, "]");
}

static void mcp_schema_rows(rjsonwriter_t *w, const struct cmd_rows *rows,
      bool describe)
{
   const struct cmd_trailer *tr;
   bool wrote = false;

   MCP_RAW(w, ",\"outputSchema\":{\"type\":\"object\",\"properties\":{");
   /* a record: the columns are the fields themselves */
   if (rows->seps)
      mcp_schema_columns(w, rows, describe);
   else if (rows->columns)
   {
      MCP_RAW(w, "\"rows\":{\"type\":\"array\",\"items\":{\"type\":\"object\","
            "\"properties\":{");
      mcp_schema_columns(w, rows, describe);
      MCP_RAW(w, "},");
      mcp_schema_required(w, rows);
      MCP_RAW(w, "}}");
   }
   wrote = rows->columns != NULL;
   if (rows->previous)
   {
      if (wrote)
         MCP_RAW(w, ",");
      wrote = true;
      MCP_RAW(w, "\"previous\":{\"type\":\"object\"");
      if (describe)
         MCP_RAW(w, ",\"description\":\"How the queued STEPS before "
               "it ended.\"");
      MCP_RAW(w, "}");
   }
   /* the lines that are not rows, as mcp_structured answers them */
   for (tr = rows->trailers; tr && tr->word; tr++)
   {
      if (tr->flag)
      {
         if (wrote)
            MCP_RAW(w, ",");
         wrote = true;
         rjsonwriter_add_string(w, tr->flag);
         MCP_RAW(w, ":{\"type\":\"boolean\",\"description\":");
         /* where it carries a number too, that one's is the number's */
         rjsonwriter_add_string(w, tr->name || !tr->desc
               ? "The reply was cut short." : tr->desc);
         MCP_RAW(w, "}");
      }
      if (tr->name)
      {
         if (wrote)
            MCP_RAW(w, ",");
         wrote = true;
         rjsonwriter_add_string(w, tr->name);
         if (tr->text)
            MCP_RAW(w, ":{\"type\":\"string\"");
         else
            MCP_RAW(w, ":{\"type\":\"integer\"");
         /* none where the name says it all */
         if (tr->desc)
         {
            MCP_RAW(w, ",\"description\":");
            rjsonwriter_add_string(w, tr->desc);
         }
         MCP_RAW(w, "}");
      }
   }
   MCP_RAW(w, "}");
   /* a record's own fields, or only the rows, are always there */
   if (rows->seps)
   {
      MCP_RAW(w, ",");
      mcp_schema_required(w, rows);
   }
   else if (rows->columns)
      MCP_RAW(w, ",\"required\":[\"rows\"]");
   MCP_RAW(w, "}");
}

/* What a client is told once, at the handshake: how the tools fit
 * together to play a game, which no one tool's description says. */
#define MCP_INSTRUCTIONS \
   "Controls a running RetroArch. To play a game: find it with " \
   "LIST_PLAYLISTS and GET_PLAYLIST, and LOAD_CONTENT it with paused on. " \
   "GET_MEMORY_MAP gives the addresses READ_CORE_MEMORY, WATCH and " \
   "conditions take. WATCH the bytes each step needs - position, state, " \
   "enemies - and every step answers with them. Step with STEPS " \
   "(approach, jump and land in one request, with a stop for a death; " \
   "queue: on plays on without pausing - send the next stretch as soon " \
   "as one answers), " \
   "PRESS ... until, or RUN_UNTIL; RUN_FRAMES runs frames with nothing " \
   "pressed but what INPUT holds. STATE " \
   "checkpoint before a risky move, and rollback after a mistake. " \
   "GET_FRAME shows the screen where memory cannot say. MENU " \
   "reads and drives RetroArch's own menu - settings, playlists, cores - " \
   "and answers with what it shows. Tools " \
   "marked destructive can lose unsaved progress or load code. No tool " \
   "runs while achievements hardcore mode is on."

/* A tool's title, from its name: RUN_FRAMES is "Run frames". */
static void mcp_title(char *s, size_t len, const char *name)
{
   size_t i;
   for (i = 0; name[i] && i + 1 < len; i++)
   {
      char ch = name[i] == '_' ? ' ' : name[i];
      if (i && ch >= 'A' && ch <= 'Z')
         ch = (char)(ch - 'A' + 'a');
      s[i] = ch;
   }
   s[i] = '\0';
}

/* A tool: its parameters as a JSON Schema, from the table rather than
 * from the prose HELP prints, so a client is told each one's name, type
 * and whether it is needed. A hotkey, and a command that takes nothing,
 * has no properties. @rows, where the command answers rows, is the
 * shape of its result, its columns described where @describe says;
 * @structured is false for a client of a revision older than
 * outputSchema. */
static void mcp_tool(rjsonwriter_t *w, const char *name, const char *desc,
      const struct cmd_param *params, const struct cmd_rows *rows,
      unsigned flags, bool structured, bool describe)
{
   const struct cmd_param *p;
   bool first = true;
   bool ro    = (flags & CMD_INFO_READ_ONLY)   != 0;
   bool de    = (flags & CMD_INFO_DESTRUCTIVE) != 0;

   char title[64];

   MCP_RAW(w, "{\"name\":");
   rjsonwriter_add_string(w, name);
   mcp_title(title, sizeof(title), name);
   MCP_RAW(w, ",\"title\":");
   rjsonwriter_add_string(w, title);
   MCP_RAW(w, ",\"description\":");
   rjsonwriter_add_string(w, desc ? desc : "");
   MCP_RAW(w, ",\"inputSchema\":{\"type\":\"object\",\"properties\":{");
   for (p = params; p && p->name; p++)
   {
      if (!first)
         MCP_RAW(w, ",");
      first = false;
      rjsonwriter_add_string(w, p->name);
      rjsonwriter_rawf(w, ":{\"type\":\"%s\",\"description\":",
            p->type == CMD_PARAM_INT ? "integer" : "string");
      rjsonwriter_add_string(w, p->desc ? p->desc : "");
      if (p->values)
         mcp_schema_enum(w, p->values);
      MCP_RAW(w, "}");
   }
   MCP_RAW(w, "}");
   first = true;
   for (p = params; p && p->name; p++)
   {
      if (!p->required)
         continue;
      /* MCP_RAW takes the length from the literal, so each one is its
       * own call rather than a choice between two */
      if (first)
         MCP_RAW(w, ",\"required\":[");
      else
         MCP_RAW(w, ",");
      first = false;
      rjsonwriter_add_string(w, p->name);
   }
   if (!first)
      MCP_RAW(w, "]");
   MCP_RAW(w, "}");
   if (rows && structured)
      mcp_schema_rows(w, rows, describe);
   /* Only what a client acts on: readOnlyHint where it is true, and
    * otherwise destructiveHint, which defaults to true and means nothing
    * for a tool that changes nothing. openWorldHint is left at its
    * default. */
   if (ro)
      MCP_RAW(w, ",\"annotations\":{\"readOnlyHint\":true}}");
   else
      rjsonwriter_rawf(w, ",\"annotations\":{\"destructiveHint\":%s}}",
            de ? "true" : "false");
}

static void mcp_tools_list(struct mcp_conn *c, const struct mcp_request *r,
      bool modern, bool structured)
{
   size_t n, i, j;
   bool   first = true;
   /* the column tables already described, by the first tool to answer
    * with each */
   const struct cmd_column *seen[32];
   size_t nseen = 0;
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
      bool describe = true;
      if (acts[i].flags & CMD_INFO_NO_TOOL)
         continue;
      if (!first)
         MCP_RAW(w, ",");
      first = false;
      if (acts[i].rows && acts[i].rows->columns && structured)
      {
         for (j = 0; j < nseen; j++)
            if (seen[j] == acts[i].rows->columns)
               describe = false;
         if (describe && nseen < ARRAY_SIZE(seen))
            seen[nseen++] = acts[i].rows->columns;
      }
      mcp_tool(w, acts[i].str, acts[i].desc, acts[i].params, acts[i].rows,
            acts[i].flags, structured, describe);
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
         mcp_tool(w, keys[i].str, keys[i].desc, NULL, NULL, keys[i].flags,
               structured, true);
      }
   }
   MCP_RAW(w, "]");
   if (modern)
      MCP_RAW(w, MCP_NO_CACHE);
   mcp_result_end(w, modern);
   mcp_respond_writer(c, w);
}

/* The action row for @name, or NULL when the command is a hotkey -
 * which takes no parameters - or not a command at all. */
static const struct cmd_action_map *mcp_tool_action(const char *name)
{
   size_t n, i;
   const struct cmd_action_map *acts = command_action_list(&n);
   for (i = 0; i < n; i++)
      if (string_is_equal(acts[i].str, name))
         return &acts[i];
   return NULL;
}

/* What goes on the line for @p: what the client sent, else what the
 * table stands in for it, else nothing. */
static const char *mcp_param_value(const struct cmd_param *p,
      const struct mcp_request *r)
{
   const char *v = mcp_arg(r, p->name);
   return v ? v : p->def;
}

/* Normalize exactly, without rounding through a floating-point value. */
static char *mcp_integer(const char *value)
{
   const char *s = value;
   size_t n = 0, fractional = 0, first = 0, i, count, size;
   long exponent = 0, places;
   bool negative = false, dot = false, exp_negative = false;
   char *digits, *out;

   if (*s == '-')
   {
      negative = true;
      s++;
   }
   if (!(digits = (char*)malloc(strlen(s) + 1)))
      return NULL;
   while (*s && *s != 'e' && *s != 'E')
   {
      if (*s == '.')
         dot = true;
      else
      {
         digits[n++] = *s;
         if (dot)
            fractional++;
      }
      s++;
   }
   if (*s)
   {
      s++;
      if (*s == '-' || *s == '+')
         exp_negative = *s++ == '-';
      while (*s)
      {
         if (exponent < MCP_MAX_REQUEST * 2)
            exponent = exponent * 10 + (*s - '0');
         s++;
      }
   }
   while (first < n && digits[first] == '0')
      first++;
   if (first == n)
   {
      digits[0] = '0';
      digits[1] = '\0';
      return digits;
   }
   if (exp_negative)
      exponent = -exponent;
   places = exponent - (long)fractional;
   while (places < 0 && n > first && digits[n - 1] == '0')
   {
      n--;
      places++;
   }
   if (places < 0 || places > MCP_MAX_REQUEST)
   {
      free(digits);
      return NULL;
   }
   count = n - first;
   size  = count + (size_t)places + (negative ? 1 : 0) + 1;
   if (size > MCP_MAX_REQUEST || !(out = (char*)malloc(size)))
   {
      free(digits);
      return NULL;
   }
   i = 0;
   if (negative)
      out[i++] = '-';
   memcpy(out + i, digits + first, count);
   i += count;
   memset(out + i, '0', (size_t)places);
   out[i + (size_t)places] = '\0';
   free(digits);
   return out;
}

static const char *mcp_integer_args(const struct cmd_action_map *act,
      struct mcp_request *r)
{
   const struct cmd_param *p;
   unsigned i;
   if (!act || !act->params)
      return NULL;
   if (mcp_arg(r, "argument") && !mcp_arg(r, act->params[0].name))
      return NULL;
   for (p = act->params; p->name; p++)
      if (p->type == CMD_PARAM_INT)
         for (i = 0; i < r->num_args; i++)
            if (string_is_equal(r->args[i].name, p->name))
            {
               char *value;
               if (!r->args[i].is_number
                     || !(value = mcp_integer(r->args[i].value)))
                  return p->name;
               free(r->args[i].value);
               r->args[i].value = value;
            }
   return NULL;
}

/* A name the client sent that the tool does not take, or NULL. The
 * schemas do not forbid other names, to keep the tool list short, so
 * the server does: a misspelt one would otherwise be dropped from the
 * line and the command run as if it had been left out. */
static const char *mcp_unknown_arg(const struct cmd_action_map *act,
      const struct mcp_request *r)
{
   const struct cmd_param *p;
   unsigned i;
   for (i = 0; i < r->num_args; i++)
   {
      if (string_is_equal(r->args[i].name, "argument"))
         continue;
      for (p = act ? act->params : NULL; p && p->name; p++)
         if (string_is_equal(r->args[i].name, p->name))
            break;
      if (!p || !p->name)
         return r->args[i].name;
   }
   return NULL;
}

/* A parameter sent as true, false, null, an object or an array, which
 * no tool takes, or NULL. */
static const char *mcp_other_arg(const struct mcp_request *r)
{
   unsigned i;
   for (i = 0; i < r->num_args; i++)
      if (r->args[i].is_other)
         return r->args[i].name;
   return NULL;
}

/* A parameter whose value is not one of the set its schema offers, or
 * NULL: "yes" for an on|off would otherwise reach the line as text the
 * handler reads as something else. */
static const struct cmd_param *mcp_enum_arg(const struct cmd_action_map *act,
      const struct mcp_request *r)
{
   const struct cmd_param *p;
   for (p = act ? act->params : NULL; p && p->name; p++)
   {
      const char *v = p->values ? mcp_arg(r, p->name) : NULL;
      const char *w;
      size_t      n;
      if (!v)
         continue;
      /* empty stands in for left out where it may be left out; a value
       * that must be one of the set is not one of them empty */
      if (!*v)
      {
         if (!p->required)
            continue;
         return p;
      }
      n = strlen(v);
      for (w = p->values; *w; )
      {
         const char *bar = strchr(w, '|');
         size_t      len = bar ? (size_t)(bar - w) : strlen(w);
         if (len == n && !strncmp(w, v, n))
            break;
         w += len + (bar ? 1 : 0);
      }
      if (!*w)
         return p;
   }
   return NULL;
}

/* Puts what the client sent by name back into the one line the command
 * handlers read, in the order and with the separators the table gives.
 * Returns the line, which the caller frees, or NULL with @err set.
 *
 * A client still sending the single "argument" this server used to take
 * - one whose cached tool list predates the parameters - is given it
 * verbatim, so an upgrade does not break it mid-session. */
static char *mcp_compose(const struct cmd_action_map *act,
      const struct mcp_request *r, const char **err)
{
   const struct cmd_param *p;
   const struct cmd_param *last = NULL;
   const char *only;
   size_t      len = 1;
   char       *out;
   bool        first = true;

   if (!act || !act->params)
      return NULL;

   if ((only = mcp_arg(r, "argument")) && !mcp_arg(r, act->params[0].name))
   {
      size_t l = strlen(only);
      if ((out = (char*)malloc(l + 1)))
         memcpy(out, only, l + 1);
      return out;
   }

   for (p = act->params; p->name; p++)
   {
      const char *v = mcp_param_value(p, r);
      if (!v)
      {
         if (p->required)
         {
            *err = p->name;
            return NULL;
         }
         continue;
      }
      /* an empty optional one keeps the places of those after it, and
       * with none after it is left out: "core|content|" is a path */
      if (*v || p->required)
         last = p;
      len += strlen(v) + (p->lead ? strlen(p->lead) : 1);
   }

   if (!(out = (char*)malloc(len)))
      return NULL;
   *out = '\0';
   for (p = act->params; last && p <= last; p++)
   {
      const char *v = mcp_param_value(p, r);
      if (!v)
         continue;
      /* a space joins them unless the command's own form says
       * otherwise, and nothing leads the first */
      if (!first)
         strlcat(out, p->lead ? p->lead : " ", len);
      strlcat(out, v, len);
      first = false;
   }
   return out;
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

/* One line as one row. The columns before the slack one are read from
 * the line's start and those after it from the end, so the field
 * between them keeps whatever tabs the line has over its columns. */
static void mcp_row(rjsonwriter_t *w, const struct cmd_rows *rows,
      const char *line, const char *stop, size_t ncol, size_t slack)
{
   size_t i;

   MCP_RAW(w, "{");
   for (i = 0; i < ncol; i++)
   {
      const char *e = NULL;
      if (i == slack)
      {
         /* back over one tab for each column still to come */
         size_t n = ncol - 1 - i;
         e = stop;
         while (n-- && e > line)
            while (e > line && *--e != '\t')
               ;
      }
      else
         e = (const char*)memchr(line, '\t', (size_t)(stop - line));
      if (!e)
         e = stop;
      if (i)
         MCP_RAW(w, ",");
      rjsonwriter_add_string(w, rows->columns[i].name);
      MCP_RAW(w, ":");
      rjsonwriter_add_string_len(w, line, (int)(e - line));
      line = e < stop ? e + 1 : stop;
   }
   MCP_RAW(w, "}");
}

/* Which of @tr the line from @line to @stop is - its word, then a
 * number - or -1 for none, which makes it a row. A trailer counts only
 * where it belongs, at the reply's @opening line or its @closing one,
 * and only with digits after the word: a playlist named "MORE games" is
 * a row, and so is a log line that happens to start with NEXT. The
 * number found is left in @num and @num_len for the caller. */
static int mcp_trailer(const struct cmd_trailer *tr, size_t n,
      const char *line, const char *stop, bool opening, bool closing,
      const char **num, size_t *num_len)
{
   size_t i;
   size_t len = (size_t)(stop - line);

   for (i = 0; i < n; i++)
   {
      size_t j;
      size_t k;
      size_t wl = strlen(tr[i].word);
      if (tr[i].text || (tr[i].first ? !opening : !closing))
         continue;
      if (len < wl || strncmp(line, tr[i].word, wl))
         continue;
      /* the word by itself, from a reply with nowhere to resume */
      if (len == wl)
      {
         if (!tr[i].flag)
            continue;
         return (int)i;
      }
      if (line[wl] != ' ')
         continue;
      /* the number ends the line: "PRESS 1 right+a 4" ran 4 */
      for (k = len; k > wl + 1 && line[k - 1] != ' '; k--)
         ;
      for (j = k; j < len; j++)
         if (line[j] < '0' || line[j] > '9')
            break;
      if (j < len || k == len)
         continue;
      *num     = line + k;
      *num_len = len - k;
      return (int)i;
   }
   return -1;
}

/* A trailer found, as fields: its flag, and the number it carried. */
static void mcp_trailer_fields(rjsonwriter_t *w,
      const struct cmd_trailer *tr, const char *num, size_t num_len,
      bool *wrote)
{
   if (tr->flag)
   {
      if (*wrote)
         MCP_RAW(w, ",");
      *wrote = true;
      rjsonwriter_add_string(w, tr->flag);
      MCP_RAW(w, ":true");
   }
   if (tr->name && num_len)
   {
      if (*wrote)
         MCP_RAW(w, ",");
      *wrote = true;
      rjsonwriter_add_string(w, tr->name);
      MCP_RAW(w, ":");
      rjsonwriter_raw(w, num, (int)num_len);
   }
}

/* ,"previous":{...} from "PREVIOUS STOPPED 3 STOP 1 STEP 2" - each
 * word and its number, in lower case - or "PREVIOUS ERROR <why>". */
static void mcp_structured_previous(rjsonwriter_t *w, const char *p,
      const char *end)
{
   bool first = true;
   MCP_RAW(w, ",\"previous\":{");
   if ((size_t)(end - p) > 6 && !strncmp(p, "ERROR ", 6))
   {
      MCP_RAW(w, "\"error\":");
      rjsonwriter_add_string_len(w, p + 6, (int)(end - p - 6));
      p = end;
   }
   while (p < end)
   {
      char        word[16];
      size_t      i;
      const char *num = (const char*)memchr(p, ' ', (size_t)(end - p));
      const char *q;
      if (!num || (size_t)(num - p) >= sizeof(word))
         break;
      for (i = 0; p + i < num; i++)
         word[i] = p[i] >= 'A' && p[i] <= 'Z' ? (char)(p[i] + 32) : p[i];
      word[i] = '\0';
      for (q = ++num; q < end && *q >= '0' && *q <= '9'; q++)
         ;
      if (q == num)
         break;
      if (!first)
         MCP_RAW(w, ",");
      first = false;
      rjsonwriter_add_string(w, word);
      MCP_RAW(w, ":");
      rjsonwriter_raw(w, num, (int)(q - num));
      p = q < end ? q + 1 : end;
   }
   MCP_RAW(w, "}");
}

/* A record's fields from the line the command opens with its own
 * name: split by rows->seps in turn, the last field taking the rest,
 * and the trailers on the lines after it. */
static void mcp_structured_record(rjsonwriter_t *w,
      const struct cmd_rows *rows, const char *name, const char *text,
      size_t len)
{
   const struct cmd_column *col;
   const char *end      = text + len;
   const char *p        = text;
   const char *eol      = (const char*)memchr(text, '\n', len);
   size_t      name_len = strlen(name);
   size_t      i        = 0;
   bool        first    = true;
   bool        wrote;

   if (eol)
      end = eol;
   if ((size_t)(end - p) > name_len && !strncmp(p, name, name_len)
         && p[name_len] == ' ')
      p += name_len + 1;
   MCP_RAW(w, ",\"structuredContent\":{");
   for (col = rows->columns; col && col->name && p < end; col++, i++)
   {
      const char *stop = end;
      if (col[1].name)
      {
         char sep = rows->seps[i] ? rows->seps[i]
            : rows->seps[strlen(rows->seps) - 1];
         const char *q = (const char*)memchr(p, sep, (size_t)(end - p));
         if (q)
            stop = q;
      }
      if (!first)
         MCP_RAW(w, ",");
      first = false;
      rjsonwriter_add_string(w, col->name);
      MCP_RAW(w, ":");
      rjsonwriter_add_string_len(w, p, (int)(stop - p));
      p = stop < end ? stop + 1 : end;
   }
   wrote = !first;
   for (p = end; p < text + len; )
   {
      const char *line = p + 1;
      const char *num;
      size_t      num_len;
      int         t;
      size_t      ntr = 0;
      eol     = (const char*)memchr(line, '\n', (size_t)(text + len - line));
      p       = eol ? eol : text + len;
      num     = NULL;
      num_len = 0;
      while (rows->trailers && rows->trailers[ntr].word)
         ntr++;
      if ((t = mcp_trailer(rows->trailers, ntr, line, p, false, true,
            &num, &num_len)) >= 0)
         mcp_trailer_fields(w, &rows->trailers[t], num, num_len, &wrote);
   }
   MCP_RAW(w, "}");
}

static void mcp_structured(rjsonwriter_t *w, const struct cmd_rows *rows,
      const char *name, const char *text, size_t len)
{
   const char *p        = text;
   const char *end      = text + len;
   /* A trailer is the reply's first line or one of its last three -
    * STEPS ends on which stop held and then on which step, MENU on the
    * selection, the count and where more go on from: the number each
    * one found carried. */
   const char *num[16];
   size_t      num_len[16];
   bool        found[16];
   size_t      nlines   = 0;
   size_t      at       = 0;
   size_t      name_len = strlen(name);
   size_t      ncol     = 0;
   size_t      ntr      = 0;
   size_t      slack;
   size_t      req;
   size_t      present;
   size_t      i;
   bool        first    = true;
   bool        wrote    = false;
   const char *prev     = NULL;
   const char *prev_stop = NULL;
   /* a text field's line, per trailer, where one came */
   const char *text_at[16];
   const char *text_stop[16];

   while (rows->columns && rows->columns[ncol].name)
      ncol++;
   while (rows->trailers && rows->trailers[ntr].word)
      ntr++;
   for (i = 0; i < 16; i++)
      text_at[i] = text_stop[i] = NULL;
   for (i = 0; i < 16; i++)
   {
      found[i]   = false;
      num[i]     = NULL;
      num_len[i] = 0;
   }
   for (p = text; p < end; )
   {
      const char *eol = (const char*)memchr(p, '\n', (size_t)(end - p));
      if ((eol ? eol : end) > p)
         nlines++;
      p = eol ? eol + 1 : end;
   }
   p = text;
   slack = ncol && rows->slack < ncol ? rows->slack : (ncol ? ncol - 1 : 0);
   req   = rows->required && rows->required < ncol ? rows->required : ncol;

   MCP_RAW(w, ",\"structuredContent\":{");
   if (ncol)
      MCP_RAW(w, "\"rows\":[");
   while (p < end)
   {
      const char *eol  = (const char*)memchr(p, '\n', (size_t)(end - p));
      const char *line = p;
      const char *stop = eol ? eol : end;
      const char *f;
      size_t      tabs = 0;

      p = eol ? eol + 1 : end;
      if (stop == line)
         continue;
      at++;
      for (i = 0; i < ntr && i < 16; i++)
      {
         size_t wl = strlen(rows->trailers[i].word);
         if (     rows->trailers[i].text && (size_t)(stop - line) > wl
               && !strncmp(line, rows->trailers[i].word, wl)
               && line[wl] == ' ')
            break;
      }
      if (i < ntr && i < 16)
      {
         text_at[i]   = line + strlen(rows->trailers[i].word) + 1;
         text_stop[i] = stop;
         continue;
      }
      if (     rows->previous && (size_t)(stop - line) > 9
            && !strncmp(line, "PREVIOUS ", 9))
      {
         prev      = line + 9;
         prev_stop = stop;
         continue;
      }
      /* one of the command's trailers rather than a row */
      {
         const char *n = NULL;
         size_t      l = 0;
         int         t = mcp_trailer(rows->trailers, ntr, line, stop,
               at == 1, at + 3 > nlines, &n, &l);
         if (t >= 0 && t < 16)
         {
            found[t]   = true;
            num[t]     = n;
            num_len[t] = l;
            continue;
         }
      }
      /* the command opens a line that is not a row with its own name */
      if (     (size_t)(stop - line) > name_len
            && !strncmp(line, name, name_len) && line[name_len] == ' ')
         continue;
      for (f = line; f < stop; f++)
         if (*f == '\t')
            tabs++;
      if (!ncol)
         continue;
      /* fewer fields than the reply always has: not a row of it */
      if (tabs + 1 < req)
         continue;
      /* a reply may end short of its columns, and then the last one it
       * does carry takes whatever the line has left */
      present = tabs + 1 < ncol ? tabs + 1 : ncol;
      if (!first)
         MCP_RAW(w, ",");
      first = false;
      mcp_row(w, rows, line, stop, present,
            slack < present ? slack : present - 1);
   }
   if (ncol)
   {
      MCP_RAW(w, "]");
      wrote = true;
   }
   /* in the order the table names them, whichever line they came on */
   for (i = 0; i < ntr && i < 16; i++)
      if (found[i])
         mcp_trailer_fields(w, &rows->trailers[i], num[i], num_len[i],
               &wrote);
   for (i = 0; i < ntr && i < 16; i++)
      if (text_at[i])
      {
         if (wrote)
            MCP_RAW(w, ",");
         wrote = true;
         rjsonwriter_add_string(w, rows->trailers[i].name);
         MCP_RAW(w, ":");
         rjsonwriter_add_string_len(w, text_at[i],
               (int)(text_stop[i] - text_at[i]));
      }
   /* after the rows, which every reply with one has */
   if (prev)
      mcp_structured_previous(w, prev, prev_stop);
   MCP_RAW(w, "}");
}

/* {"content":[{"type":"text","text":<text>}],"isError":<b>}, and the
 * same reply as structuredContent where the command answers rows and
 * the client's revision takes it. The text is what every interface
 * sends, byte for byte: a client that reads it alone sees no change. */
/* How long the UTF-8 sequence at @p is, or 0 where it is not one. */
static size_t mcp_utf8_len(const unsigned char *p, size_t left)
{
   size_t        n;
   size_t        i;
   unsigned char lo = 0x80;
   unsigned char hi = 0xBF;
   if (p[0] < 0x80)
      return 1;
   if      (p[0] >= 0xC2 && p[0] <= 0xDF) n = 2;
   else if (p[0] >= 0xE0 && p[0] <= 0xEF) n = 3;
   else if (p[0] >= 0xF0 && p[0] <= 0xF4) n = 4;
   else
      return 0;
   if (left < n)
      return 0;
   /* no overlong forms, no surrogates, nothing past U+10FFFF */
   if      (p[0] == 0xE0) lo = 0xA0;
   else if (p[0] == 0xED) hi = 0x9F;
   else if (p[0] == 0xF0) lo = 0x90;
   else if (p[0] == 0xF4) hi = 0x8F;
   if (p[1] < lo || p[1] > hi)
      return 0;
   for (i = 2; i < n; i++)
      if (p[i] < 0x80 || p[i] > 0xBF)
         return 0;
   return n;
}

/* @s with whatever is not UTF-8 - a path in another encoding, a line
 * cut short mid-character - as U+FFFD, allocated, its length in
 * @out_len; NULL where @s needs no repair. JSON text must be UTF-8, and
 * a client that cannot decode one answer may drop the whole stream. */
static char *mcp_utf8_repair(const char *s, size_t len, size_t *out_len)
{
   const unsigned char *p = (const unsigned char*)s;
   size_t i;
   size_t n;
   size_t o = 0;
   char  *out;
   for (i = 0; i < len; i += n)
      if (!(n = mcp_utf8_len(p + i, len - i)))
         break;
   if (i >= len || !(out = (char*)malloc(len * 3 + 1)))
      return NULL;
   for (i = 0; i < len; i += n)
   {
      if ((n = mcp_utf8_len(p + i, len - i)))
      {
         memcpy(out + o, s + i, n);
         o += n;
      }
      else
      {
         memcpy(out + o, "\xEF\xBF\xBD", 3);
         o += 3;
         n  = 1;
      }
   }
   out[o]   = '\0';
   *out_len = o;
   return out;
}

static void mcp_tool_result(struct mcp_conn *c, const char *id, bool modern,
      const struct cmd_action_map *act, const char *text, size_t text_len,
      bool is_error)
{
   size_t         fixed_len = 0;
   char          *fixed     = mcp_utf8_repair(text, text_len, &fixed_len);
   rjsonwriter_t *w         = mcp_envelope(id);
   if (fixed)
   {
      text     = fixed;
      text_len = fixed_len;
   }
   if (!w)
   {
      free(fixed);
      mcp_conn_close(c);
      return;
   }
   MCP_RAW(w, "\"result\":{\"content\":[{\"type\":\"text\",\"text\":");
   rjsonwriter_add_string_len(w, text, (int)text_len);
   MCP_RAW(w, "}]");
   /* a failure's text is the reason, not rows */
   if (act && act->rows && c->structured && !is_error)
   {
      if (act->rows->seps)
         mcp_structured_record(w, act->rows, act->str, text, text_len);
      else
         mcp_structured(w, act->rows, act->str, text, text_len);
   }
   rjsonwriter_rawf(w, ",\"isError\":%s", is_error ? "true" : "false");
   mcp_result_end(w, modern);
   mcp_respond_writer(c, w);
   free(fixed);
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
   if (o->conn == mcp->current)
      mcp->answered = true;
   mcp_tool_result(c, c->id, c->modern, c->act, data, len, cmd->error);
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
   if (o->conn == mcp->current)
      mcp->answered = true;
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
   {
      size_t fixed_len = 0;
      char  *fixed     = mcp_utf8_repair(text, len, &fixed_len);
      if (fixed)
         rjsonwriter_add_string_len(w, fixed, (int)fixed_len);
      else
         rjsonwriter_add_string_len(w, text, (int)len);
      free(fixed);
   }
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
         else if (string_is_equal(key[1], "level") && t == RJSON_STRING)
            mcp_take(j, r->level, sizeof(r->level));
      }
      else if (depth == 3 && string_is_equal(key[0], "params"))
      {
         if (     string_is_equal(key[1], "arguments")
               && *key[2]
               && r->num_args < MCP_MAX_ARGS)
         {
            /* A number arrives as the digits it was written with, which
             * is what the line form wants anyway. Any other type is kept
             * as present, so it is refused rather than dropped. */
            size_t      l    = 0;
            bool        text = t == RJSON_STRING || t == RJSON_NUMBER;
            const char *v    = text ? rjson_get_string(j, &l) : "";
            struct mcp_arg *a = &r->args[r->num_args];
            if ((a->value = (char*)malloc(l + 1)))
            {
               memcpy(a->value, v, l);
               a->value[l] = '\0';
               a->is_number = t == RJSON_NUMBER;
               a->is_other  = !text;
               strlcpy(a->name, key[2], sizeof(a->name));
               r->num_args++;
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

/* The client's revision gives outputSchema and structuredContent. That
 * revision also made MCP-Protocol-Version mandatory, so an HTTP request
 * without the header is from the one revision before it, 2025-03-26,
 * which is sent neither. The revisions are ISO dates and compare as
 * dates. */
static bool mcp_structured_header(const char *v)
{
   return v && mcp_header_len(v) == 10
      && strncmp(v, MCP_STRUCTURED_VERSION, 10) >= 0;
}

/* The same of a revision named in a request body or at a handshake,
 * where there is no header length to check. */
static bool mcp_structured_version(const char *v)
{
   return v && strncmp(v, MCP_STRUCTURED_VERSION, 10) >= 0;
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
   bool  modern, structured;
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
   /* an id too long to answer with: nothing the client could match */
   if (!*r.id)
   {
      mcp_error(c, NULL, MCP_ERR_INVALID_REQUEST, "The id is too long.",
            false);
      goto end;
   }

   hv     = mcp_header(head, "MCP-Protocol-Version");
   hm     = mcp_header(head, "Mcp-Method");
   hn     = mcp_header(head, "Mcp-Name");
   modern = *r.version != '\0';
   /* The stateless era is newer than the revision either way, and a
    * classic HTTP client names its revision in a header on every
    * request. Over stdio there are no headers and no second request to
    * carry one: the revision agreed at initialize holds for the session,
    * which is the connection. */
   structured = modern || mcp_structured_header(hv)
      || (c->stdio && mcp_stdio.agreed);

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
      rjsonwriter_rawf(w, "],\"capabilities\":{\"tools\":{\"listChanged\":false}%s},"
            "\"instructions\":", c->stdio ? ",\"logging\":{}" : "");
      rjsonwriter_add_string(w, MCP_INSTRUCTIONS);
      MCP_RAW(w, MCP_NO_CACHE);
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
      /* what this session answers with from here on */
      if (c->stdio)
         mcp_stdio.agreed = mcp_structured_version(v);
      MCP_RAW(w, "\"result\":{\"protocolVersion\":");
      rjsonwriter_add_string(w, v);
      MCP_RAW(w, ",\"capabilities\":{\"tools\":{\"listChanged\":false}");
      if (c->stdio)
         MCP_RAW(w, ",\"logging\":{}");
      MCP_RAW(w, "},\"serverInfo\":{\"name\":\"RetroArch\",\"version\":\""
            PACKAGE_VERSION "\"},\"instructions\":");
      rjsonwriter_add_string(w, MCP_INSTRUCTIONS);
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
   else if (string_is_equal(r.method, "logging/setLevel") && c->stdio)
   {
      /* what RetroArch logs from here on, at @level and above, goes to
       * the client as notifications/message */
      unsigned i;
      rjsonwriter_t *w;
      for (i = 0; i < sizeof(mcp_log_levels) / sizeof(mcp_log_levels[0]); i++)
         if (string_is_equal(r.level, mcp_log_levels[i]))
            break;
      if (i == sizeof(mcp_log_levels) / sizeof(mcp_log_levels[0]))
      {
         mcp_error(c, r.id, MCP_ERR_PARAMS, "Unknown log level.", false);
         goto end;
      }
      if (mcp_stdio.log_level < 0)
      {
         size_t first;
         verbosity_ring_arm();
         /* from now: the lines already kept were not asked for */
         verbosity_ring_read((size_t)-1, NULL, 0, &first,
               &mcp_stdio.log_next);
      }
      mcp_stdio.log_level = (int)i;
      if (!(w = mcp_envelope(r.id)))
      {
         mcp_conn_close(c);
         goto end;
      }
      MCP_RAW(w, "\"result\":{\"level\":");
      rjsonwriter_add_string(w, mcp_log_levels[i]);
      mcp_result_end(w, modern);
      mcp_respond_writer(c, w);
   }
   else if (string_is_equal(r.method, "tools/list"))
      mcp_tools_list(c, &r, modern, structured);
   else if (string_is_equal(r.method, "tools/call"))
   {
      bool        ok;
      char       *line    = NULL;
      const char *missing = NULL;
      const char *invalid;
      const struct cmd_action_map *act;
      if (!mcp_tool_known(r.name))
      {
         mcp_error(c, r.id, MCP_ERR_PARAMS, "Unknown tool.", false);
         goto end;
      }
      act = mcp_tool_action(r.name);
      /* nothing runs for an assistant while achievements are hardcore */
      {
         const char *why = command_tools_refused();
         if (why)
         {
            char msg[192];
            int  n = snprintf(msg, sizeof(msg), "%.64s ERROR %s",
                  r.name, why);
            mcp_tool_result(c, r.id, modern, NULL, msg,
                  n > 0 && (size_t)n < sizeof(msg) ? (size_t)n
                  : strlen(msg), true);
            goto end;
         }
      }
      if ((invalid = mcp_unknown_arg(act, &r)))
      {
         char msg[128];
         snprintf(msg, sizeof(msg), "Unknown parameter \"%.96s\".",
               invalid);
         mcp_error(c, r.id, MCP_ERR_PARAMS, msg, false);
         goto end;
      }
      if ((invalid = mcp_other_arg(&r)))
      {
         char msg[128];
         snprintf(msg, sizeof(msg), "Parameter \"%.64s\" takes a string "
               "or a number.", invalid);
         mcp_error(c, r.id, MCP_ERR_PARAMS, msg, false);
         goto end;
      }
      {
         const struct cmd_param *bad = mcp_enum_arg(act, &r);
         if (bad)
         {
            char msg[256];
            snprintf(msg, sizeof(msg), "Parameter \"%.64s\" is one of "
                  "%.160s.", bad->name, bad->values);
            mcp_error(c, r.id, MCP_ERR_PARAMS, msg, false);
            goto end;
         }
      }
      if ((invalid = mcp_integer_args(act, &r)))
      {
         char msg[128];
         snprintf(msg, sizeof(msg), "Invalid integer parameter \"%.96s\".",
               invalid);
         mcp_error(c, r.id, MCP_ERR_PARAMS, msg, false);
         goto end;
      }
      /* What the client sent by name becomes the line the handler
       * reads; a parameter it needed and left out is answered before
       * the command runs at all. */
      line = mcp_compose(act, &r, &missing);
      if (missing)
      {
         char msg[128];
         snprintf(msg, sizeof(msg), "Missing required parameter \"%.96s\".",
               missing);
         mcp_error(c, r.id, MCP_ERR_PARAMS, msg, false);
         goto end;
      }
      /* the command runs now; what it replies is the result */
      mcp->reply_len = 0;
      mcp->answered  = false;
      mcp->current   = ci;
      strlcpy(c->id, r.id, sizeof(c->id));
      c->modern      = modern;
      c->structured  = structured;
      c->act         = act;
      ok = command_run(cmd, r.name, line);
      free(line);
      mcp->current   = -1;
      /* answered by reply_to, later or already */
      if (c->state == MCP_CONN_WAITING || mcp->answered)
         goto end;
      if (mcp->reply_len)
         mcp_tool_result(c, r.id, modern, act, mcp->reply, mcp->reply_len,
               cmd->error || !ok);
      else
         mcp_tool_result(c, r.id, modern, NULL,
               ok ? "Done." : "The command failed.", ok ? 5 : 19, !ok);
   }
   else
      mcp_error(c, r.id, MCP_ERR_METHOD, "Method not found.", false);

end:
   mcp_request_free(&r);
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
   /* the headers and the body share the one buffer */
   if (n < 0 || (size_t)(end + 4 - c->buf) + (size_t)n > MCP_MAX_REQUEST)
      return -2;
   if (c->len - (size_t)(end + 4 - c->buf) < (size_t)n)
      return -1;
   /* the body ends where Content-Length says */
   c->len = (size_t)(end + 4 - c->buf) + (size_t)n;
   c->buf[c->len] = '\0';
   return n;
}

/* ------------------------------------------------------------------ */
/* The stdio transport                                                 */

/* Writes what stdout takes now of the queued answers. */
static void mcp_stdio_flush(void)
{
   size_t left = mcp_stdio.out_len - mcp_stdio.out_sent;
   long   put  = 0;
   if (!left)
      return;
#if defined(_WIN32)
   {
      DWORD n = 0;
      if (WriteFile(mcp_stdio.out_h, mcp_stdio.out + mcp_stdio.out_sent,
               (DWORD)(left > 65536 ? 65536 : left), &n, NULL))
         put = (long)n;
   }
#else
   put = (long)write(mcp_stdio.out_fd, mcp_stdio.out + mcp_stdio.out_sent,
         left);
   if (put < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
   {
      /* the client is gone: nothing more will be read */
      mcp_stdio.out_sent = mcp_stdio.out_len = 0;
      mcp_stdio.eof      = true;
      return;
   }
#endif
   if (put > 0)
      mcp_stdio.out_sent += (size_t)put;
   if (mcp_stdio.out_sent == mcp_stdio.out_len)
      mcp_stdio.out_sent = mcp_stdio.out_len = 0;
}

/* Reads what stdin holds now, after what is already read. */
static void mcp_stdio_fill(void)
{
   size_t room = MCP_MAX_REQUEST - mcp_stdio.in_len;
   long   got  = 0;
   if (!room || mcp_stdio.eof)
      return;
#if defined(_WIN32)
   {
      DWORD avail = 0, n = 0;
      if (!PeekNamedPipe(mcp_stdio.in_h, NULL, 0, NULL, &avail, NULL))
      {
         mcp_stdio.eof = true;
         return;
      }
      if (!avail)
         return;
      if (avail < room)
         room = avail;
      if (!ReadFile(mcp_stdio.in_h, mcp_stdio.in + mcp_stdio.in_len,
               (DWORD)room, &n, NULL))
      {
         mcp_stdio.eof = true;
         return;
      }
      got = (long)n;
   }
#else
   got = (long)read(STDIN_FILENO, mcp_stdio.in + mcp_stdio.in_len, room);
   if (got == 0)
      mcp_stdio.eof = true;
   if (got <= 0)
      return;
#endif
   mcp_stdio.in_len += (size_t)got;
}

/* The log lines since the last, at the level asked for and above, as
 * notifications/message. */
static void mcp_stdio_log(void)
{
   size_t first, next, n;
   char  *s, *end;
   if (mcp_stdio.log_level < 0)
      return;
   if (!mcp_stdio.log && !(mcp_stdio.log = (char*)malloc(MCP_MAX_REQUEST)))
      return;
   n = verbosity_ring_read(mcp_stdio.log_next, mcp_stdio.log,
         MCP_MAX_REQUEST, &first, &next);
   mcp_stdio.log_next = next;
   for (s = mcp_stdio.log, end = s + n; s < end; )
   {
      char          *eol   = (char*)memchr(s, '\n', (size_t)(end - s));
      int            level = 1;
      int            wl    = 0;
      char          *body;
      rjsonwriter_t *w;
      if (!eol)
         eol = end;
      *eol = '\0';
      if (strstr(s, FILE_PATH_LOG_ERROR))
         level = 4;
      else if (strstr(s, FILE_PATH_LOG_WARN))
         level = 3;
      else if (strstr(s, FILE_PATH_LOG_DBG))
         level = 0;
      if (level >= mcp_stdio.log_level && (w = rjsonwriter_open_memory()))
      {
         MCP_RAW(w, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/message\","
               "\"params\":{\"level\":");
         rjsonwriter_add_string(w, mcp_log_levels[level]);
         MCP_RAW(w, ",\"logger\":\"retroarch\",\"data\":");
         {
            size_t fixed_len = 0;
            char  *fixed     = mcp_utf8_repair(s, (size_t)(eol - s),
                  &fixed_len);
            if (fixed)
               rjsonwriter_add_string_len(w, fixed, (int)fixed_len);
            else
               rjsonwriter_add_string_len(w, s, (int)(eol - s));
            free(fixed);
         }
         MCP_RAW(w, "}}");
         if ((body = rjsonwriter_get_memory_buffer(w, &wl)) && wl > 0
               /* half the queue at most, so an answer always has room */
               && mcp_stdio.out_len + (size_t)wl + 1 <= MCP_STDIO_OUT_MAX / 2)
         {
            char *nb = (char*)realloc(mcp_stdio.out,
                  mcp_stdio.out_len + (size_t)wl + 1);
            if (nb)
            {
               memcpy(nb + mcp_stdio.out_len, body, (size_t)wl);
               nb[mcp_stdio.out_len + (size_t)wl] = '\n';
               mcp_stdio.out      = nb;
               mcp_stdio.out_len += (size_t)wl + 1;
            }
         }
         rjsonwriter_free(w);
      }
      s = eol + 1;
   }
}

static void mcp_stdio_poll(mcp_server_t *mcp, command_t *cmd,
      retro_time_t now)
{
   struct mcp_conn *c = &mcp->conns[0];

   mcp_stdio_log();
   mcp_stdio_flush();

   if (c->state == MCP_CONN_WAITING)
   {
      if (now - c->since > MCP_TIMEOUT_US)
         mcp_tool_result(c, c->id, c->modern, NULL,
               "The command did not answer in time.", 35, true);
      return;
   }

   mcp_stdio_fill();
   while (c->state == MCP_CONN_READING)
   {
      char  *nl = (char*)memchr(mcp_stdio.in, '\n', mcp_stdio.in_len);
      size_t line;
      if (!nl)
      {
         if (mcp_stdio.in_len >= MCP_MAX_REQUEST)
         {
            mcp_stdio.in_len = 0;
            if (!mcp_stdio.discarding)
               mcp_error(c, NULL, MCP_ERR_PARSE,
                     "The message is too long.", false);
            mcp_stdio.discarding = true;
         }
         break;
      }
      line = (size_t)(nl - mcp_stdio.in);
      *nl  = '\0';
      if (line && mcp_stdio.in[line - 1] == '\r')
         mcp_stdio.in[--line] = '\0';
      /* the rest of a message too long to read is not one of its own */
      if (line && !mcp_stdio.discarding)
      {
         c->serial = ++mcp->serials;
         c->since  = now;
         mcp_handle(mcp, cmd, 0, (char*)"", mcp_stdio.in, line);
      }
      mcp_stdio.discarding = false;
      mcp_stdio.in_len    -= (size_t)(nl + 1 - mcp_stdio.in);
      memmove(mcp_stdio.in, nl + 1, mcp_stdio.in_len);
      /* the other requests wait out the content load */
      if (command_interfaces_held())
         break;
   }
   mcp_stdio_flush();

   /* the client closed stdin: it is done with RetroArch */
   if (     mcp_stdio.eof && !mcp_stdio.in_len && !mcp_stdio.quit_sent
         && c->state == MCP_CONN_READING)
   {
      mcp_stdio.quit_sent = true;
      command_event(CMD_EVENT_QUIT, NULL);
   }
}

bool command_mcp_stdio_claim(void)
{
   if (mcp_stdio.claimed)
      return true;
   if (!(mcp_stdio.in = (char*)malloc(MCP_MAX_REQUEST + 1)))
      return false;
   fflush(stdout);
#if defined(_WIN32)
   {
      DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
      int   fd   = _dup(_fileno(stdout));
      if (fd < 0)
         return false;
      mcp_stdio.out_h = (HANDLE)_get_osfhandle(fd);
      mcp_stdio.in_h  = GetStdHandle(STD_INPUT_HANDLE);
      /* a write the pipe cannot take now returns short, not waits */
      SetNamedPipeHandleState(mcp_stdio.out_h, &mode, NULL, NULL);
      _dup2(_fileno(stderr), _fileno(stdout));
      SetStdHandle(STD_OUTPUT_HANDLE, GetStdHandle(STD_ERROR_HANDLE));
   }
#else
   {
      int fd = dup(STDOUT_FILENO);
      if (fd < 0)
         return false;
      mcp_stdio.out_fd = fd;
      dup2(STDERR_FILENO, STDOUT_FILENO);
      fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
      fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
   }
#endif
   mcp_stdio.log_level = -1;
   mcp_stdio.claimed   = true;
   return true;
}

bool command_mcp_stdio_claimed(void)
{
   return mcp_stdio.claimed;
}

static void mcp_poll(command_t *cmd)
{
   mcp_server_t *mcp = (mcp_server_t*)cmd->userptr;
   retro_time_t  now = cpu_features_get_time_usec();
   int i;

   if (mcp->stdio)
   {
      mcp_stdio_poll(mcp, cmd, now);
      return;
   }

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
            mcp_tool_result(c, c->id, c->modern, NULL,
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
            mcp_tool_result(c, c->id, c->modern, NULL, gone,
                  sizeof(gone) - 1, true);
         if (c->stdio)
         {
            mcp_conn_close(c);
            continue;
         }
         /* a content load tears this down in the frame of the request
          * that started it, before the next poll would send the answer */
         if (c->state == MCP_CONN_WRITING && c->sent < c->len)
            socket_send_all_nonblocking(c->fd, c->buf + c->sent,
                  c->len - c->sent, true);
         if (c->state != MCP_CONN_FREE)
            mcp_conn_close(c);
      }
      /* the answers queued stay for the next server; as much as stdout
       * takes now goes before this one is gone */
      if (mcp->stdio)
         mcp_stdio_flush();
      if (mcp->listen_fd >= 0)
         socket_close(mcp->listen_fd);
      mcp_wipe(mcp->token, sizeof(mcp->token));
      free(mcp->reply);
      free(mcp);
   }
   free(cmd);
}

/* The server on stdin and stdout, once claimed: no token, as only the
 * process that started RetroArch holds the other ends. */
command_t *command_mcp_stdio_new(void)
{
   command_t    *cmd;
   mcp_server_t *mcp;
   int           i;

   if (!mcp_stdio.claimed)
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
   mcp->stdio     = true;
   for (i = 0; i < MCP_MAX_CONNS; i++)
      mcp->conns[i].fd = -1;
   mcp->conns[0].stdio  = true;
   mcp->conns[0].state  = MCP_CONN_READING;
   mcp->conns[0].serial = ++mcp->serials;

   cmd->userptr        = mcp;
   cmd->poll           = mcp_poll;
   cmd->replier        = mcp_reply;
   cmd->reply_dest     = mcp_reply_dest;
   cmd->reply_to       = mcp_reply_to;
   cmd->reply_image_to = mcp_reply_image_to;
   cmd->destroy        = mcp_destroy;
   cmd->structured     = true;
   verbosity_ring_arm();
   return cmd;
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
   verbosity_ring_arm();
   RARCH_LOG("[MCP] Listening on http://%s:%hu/mcp.\n",
         bind_address, (unsigned short)port);
   return cmd;
}
