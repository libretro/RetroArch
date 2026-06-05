/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andres Suarez
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

/* This file folds in the former deps/discord-rpc C++ library as pure C89.
 *
 * Design notes:
 *  - No background IO thread. The RetroArch runloop calls discord_run_callbacks()
 *    every frame, which drives reads/writes/reconnects on the main thread. All
 *    atomics/mutexes/condvars from the upstream library are therefore removed.
 *  - No auto-register. Discord_Register() is kept as an exported no-op stub so
 *    the existing init code path still links; the upstream per-OS desktop-file
 *    / registry logic is dropped.
 *  - Unix socket / Windows named-pipe backends are gated by #ifdef _WIN32.
 *  - JSON read/write use libretro-common's formats/rjson.h directly, without
 *    the C++ RAII wrappers (JsonWriter/JsonDocument/JsonReader).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <net/net_http.h>
#include <features/features_cpu.h>
#include <formats/rjson.h>
#include <time/rtime.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOIME
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#endif

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#ifdef HAVE_NETWORKING
#include "netplay/netplay.h"
#endif

#include "../paths.h"
#include "../file_path_special.h"
#include "../tasks/tasks_internal.h"
#include "../tasks/task_file_transfer.h"

#ifdef HAVE_MENU
#include "../menu/menu_cbs.h"
#include "../menu/menu_driver.h"
#endif

#include "discord.h"

#define CDN_URL "https://cdn.discordapp.com/avatars"

/* ======================================================================== */
/*  Discord-RPC internals (formerly deps/discord-rpc)                        */
/* ======================================================================== */

#define DISCORD_RPC_VERSION         1
#define DISCORD_MAX_RPC_FRAMESIZE   65536
#define DISCORD_SEND_QUEUE_SIZE     8
#define DISCORD_JOIN_QUEUE_SIZE     8
#define DISCORD_BACKOFF_MIN_MS      500
#define DISCORD_BACKOFF_MAX_MS      (60 * 1000)

/* Frame opcodes */
enum discord_opcode
{
   DISCORD_OP_HANDSHAKE = 0,
   DISCORD_OP_FRAME     = 1,
   DISCORD_OP_CLOSE     = 2,
   DISCORD_OP_PING      = 3,
   DISCORD_OP_PONG      = 4
};

/* Internal connection state */
enum discord_conn_state
{
   DISCORD_STATE_DISCONNECTED = 0,
   DISCORD_STATE_SENT_HANDSHAKE,
   DISCORD_STATE_AWAITING_RESPONSE,
   DISCORD_STATE_CONNECTED
};

enum discord_error_code
{
   DISCORD_ERR_SUCCESS      = 0,
   DISCORD_ERR_PIPE_CLOSED  = 1,
   DISCORD_ERR_READ_CORRUPT = 2
};

/* Frame header and body, laid out identically to the upstream wire format. */
#pragma pack(push, 1)
struct discord_frame_header
{
   uint32_t opcode;
   uint32_t length;
};
#pragma pack(pop)

struct discord_frame
{
   struct discord_frame_header hdr;
   char message[DISCORD_MAX_RPC_FRAMESIZE - sizeof(struct discord_frame_header)];
};

/* Queued outbound messages (presence update, subscribe, respond). */
struct discord_queued_msg
{
   size_t length;
   char buffer[16384];
};

/* Join-request queue entry */
struct discord_join_user
{
   char user_id[32];
   char username[344];
   char discriminator[8];
   char avatar[128];
};

/* Connected user (Discord returns us our own identity on READY). */
struct discord_connected_user
{
   char user_id[32];
   char username[344];
   char discriminator[8];
   char avatar[128];
};

/* Singleton state for the RPC connection layer. */
struct discord_rpc
{
#ifdef _WIN32
   HANDLE pipe;
#else
   int sock;
#endif
   enum discord_conn_state state;
   bool is_open;

   char app_id[64];
   int  last_error_code;
   char last_error_message[256];

   struct discord_frame send_frame;
};

/* ------------------------------------------------------------------------ */
/*  File-scope globals (replaces upstream's scattered static storage)        */
/* ------------------------------------------------------------------------ */

static struct discord_rpc discord_rpc_inst;
static bool discord_rpc_inst_valid = false;

static int discord_pid   = 0;
static int discord_nonce = 1;

static int  discord_last_error_code            = 0;
static int  discord_last_disconnect_error_code = 0;
static char discord_join_game_secret[256];
static char discord_spectate_game_secret[256];
static char discord_last_error_message[256];
static char discord_last_disconnect_message[256];

static DiscordEventHandlers discord_queued_handlers;
static DiscordEventHandlers discord_handlers;

/* Event edge-triggers (single-threaded, so plain bools suffice). */
static bool discord_was_just_connected    = false;
static bool discord_was_just_disconnected = false;
static bool discord_got_error_message     = false;
static bool discord_was_join_game         = false;
static bool discord_was_spectate_game     = false;

/* Pending presence update. Only the most recent one matters; we just keep one
 * slot and overwrite it. */
static struct discord_queued_msg discord_queued_presence;

/* Send queue (subscribe / unsubscribe / respond commands). Single-threaded, so
 * it is just a wraparound ring buffer with a pending counter. */
static struct discord_queued_msg discord_send_queue[DISCORD_SEND_QUEUE_SIZE];
static unsigned discord_send_next_add  = 0;
static unsigned discord_send_next_send = 0;
static unsigned discord_send_pending   = 0;

/* Join-request queue */
static struct discord_join_user discord_join_queue[DISCORD_JOIN_QUEUE_SIZE];
static unsigned discord_join_next_add  = 0;
static unsigned discord_join_next_send = 0;
static unsigned discord_join_pending   = 0;

static struct discord_connected_user discord_connected;

/* Reconnect backoff */
static int64_t discord_backoff_current = DISCORD_BACKOFF_MIN_MS;
static int64_t discord_next_connect_ms = 0;

/* ------------------------------------------------------------------------ */
/*  Small helpers                                                            */
/* ------------------------------------------------------------------------ */

static int64_t discord_now_ms(void)
{
   /* cpu_features_get_time_usec() is in microseconds, monotonic. */
   return (int64_t)(cpu_features_get_time_usec() / 1000);
}

static int64_t discord_backoff_next_delay(void)
{
   /* Upstream uses mt19937 + uniform_real_distribution(0..1) for jitter.
    * For a local IPC pipe that is overkill; a simple doubling schedule with
    * a cheap rand-based jitter factor matches the intent. */
   int     r      = rand();
   /* jitter in [0 .. current] roughly */
   int64_t jitter = (int64_t)(((double)r / (double)RAND_MAX) * (double)discord_backoff_current);
   int64_t delay  = discord_backoff_current + jitter;
   if (delay > DISCORD_BACKOFF_MAX_MS)
      delay       = DISCORD_BACKOFF_MAX_MS;
   discord_backoff_current = delay;
   return delay;
}

/* ------------------------------------------------------------------------ */
/*  Platform-specific pipe / socket I/O                                      */
/* ------------------------------------------------------------------------ */

#ifdef _WIN32

static bool discord_pipe_open(struct discord_rpc *rpc)
{
   wchar_t pipe_name[] = L"\\\\?\\pipe\\discord-ipc-0";
   const size_t digit_idx = (sizeof(pipe_name) / sizeof(wchar_t)) - 2;

   pipe_name[digit_idx] = L'0';
   for (;;)
   {
      rpc->pipe = CreateFileW(
            pipe_name, GENERIC_READ | GENERIC_WRITE, 0,
            NULL, OPEN_EXISTING, 0, NULL);
      if (rpc->pipe != INVALID_HANDLE_VALUE)
      {
         rpc->is_open = true;
         return true;
      }

      {
         DWORD last_error = GetLastError();
         if (last_error == ERROR_FILE_NOT_FOUND)
         {
            if (pipe_name[digit_idx] < L'9')
            {
               pipe_name[digit_idx]++;
               continue;
            }
         }
         else if (last_error == ERROR_PIPE_BUSY)
         {
            if (!WaitNamedPipeW(pipe_name, 10000))
               return false;
            continue;
         }
         return false;
      }
   }
}

static bool discord_pipe_close(struct discord_rpc *rpc)
{
   if (rpc->pipe != INVALID_HANDLE_VALUE)
      CloseHandle(rpc->pipe);
   rpc->pipe    = INVALID_HANDLE_VALUE;
   rpc->is_open = false;
   return true;
}

static bool discord_pipe_write(struct discord_rpc *rpc,
      const void *data, size_t length)
{
   DWORD       bytes_written = 0;
   const DWORD bytes_length  = (DWORD)length;

   if (length == 0)
      return true;
   if (!data)
      return false;
   if (rpc->pipe == INVALID_HANDLE_VALUE)
      return false;
   if (WriteFile(rpc->pipe, data, bytes_length,
            &bytes_written, NULL) != TRUE)
      return false;
   return bytes_written == bytes_length;
}

static bool discord_pipe_read(struct discord_rpc *rpc,
      void *data, size_t length)
{
   DWORD bytes_available = 0;

   if (!data)
      return false;
   if (rpc->pipe == INVALID_HANDLE_VALUE)
      return false;

   if (!PeekNamedPipe(rpc->pipe, NULL, 0, NULL,
            &bytes_available, NULL))
   {
      discord_pipe_close(rpc);
      return false;
   }

   if (bytes_available >= length)
   {
      DWORD bytes_read = 0;
      if (ReadFile(rpc->pipe, data, (DWORD)length,
               &bytes_read, NULL) == TRUE)
         return true;
      discord_pipe_close(rpc);
   }
   return false;
}

#else /* !_WIN32 --- Unix domain socket backend ---------------------------- */

#ifdef MSG_NOSIGNAL
#define DISCORD_MSG_FLAGS MSG_NOSIGNAL
#else
#define DISCORD_MSG_FLAGS 0
#endif

static const char *discord_get_temp_path(void)
{
   const char *t = getenv("XDG_RUNTIME_DIR");
   if (t)
      return t;
   t             = getenv("TMPDIR");
   if (t)
      return t;
   t             = getenv("TMP");
   if (t)
      return t;
   t             = getenv("TEMP");
   if (t)
      return t;
   return "/tmp";
}

static bool discord_pipe_open(struct discord_rpc *rpc)
{
   int                i;
   struct sockaddr_un pipe_addr;
   const char        *temp_path = discord_get_temp_path();
#ifdef SO_NOSIGPIPE
   int                optval    = 1;
#endif

   memset(&pipe_addr, 0, sizeof(pipe_addr));
   pipe_addr.sun_family = AF_UNIX;

   rpc->sock = socket(AF_UNIX, SOCK_STREAM, 0);
   if (rpc->sock == -1)
      return false;
   fcntl(rpc->sock, F_SETFL, O_NONBLOCK);

#ifdef SO_NOSIGPIPE
   setsockopt(rpc->sock, SOL_SOCKET, SO_NOSIGPIPE,
         &optval, sizeof(optval));
#endif

   for (i = 0; i < 10; i++)
   {
      snprintf(pipe_addr.sun_path, sizeof(pipe_addr.sun_path),
            "%s/discord-ipc-%d", temp_path, i);
      if (connect(rpc->sock,
               (const struct sockaddr *)&pipe_addr,
               sizeof(pipe_addr)) == 0)
      {
         rpc->is_open = true;
         return true;
      }
   }

   close(rpc->sock);
   rpc->sock    = -1;
   rpc->is_open = false;
   return false;
}

static bool discord_pipe_close(struct discord_rpc *rpc)
{
   if (rpc->sock == -1)
      return false;
   close(rpc->sock);
   rpc->sock    = -1;
   rpc->is_open = false;
   return true;
}

static bool discord_pipe_write(struct discord_rpc *rpc,
      const void *data, size_t length)
{
   ssize_t sent;
   if (rpc->sock == -1)
      return false;
   sent = send(rpc->sock, data, length, DISCORD_MSG_FLAGS);
   if (sent < 0)
      discord_pipe_close(rpc);
   return sent == (ssize_t)length;
}

static bool discord_pipe_read(struct discord_rpc *rpc,
      void *data, size_t length)
{
   int res;
   if (rpc->sock == -1)
      return false;
   res = (int)recv(rpc->sock, data, length, DISCORD_MSG_FLAGS);
   if (res < 0)
   {
      if (errno == EAGAIN)
         return false;
      discord_pipe_close(rpc);
   }
   return res == (int)length;
}

#endif /* _WIN32 */

/* ------------------------------------------------------------------------ */
/*  JSON writing (formerly serialization.cpp / JsonWriter class)             */
/* ------------------------------------------------------------------------ */

struct discord_json_writer
{
   char         *buf;
   size_t        buf_len;
   size_t        buf_cap;
   rjsonwriter_t *writer;
   bool          need_comma;
};

static int discord_json_writer_io(const void *in_buf, int in_len,
      void *user_data)
{
   struct discord_json_writer *self = (struct discord_json_writer *)user_data;
   size_t remain                    = self->buf_cap - self->buf_len;

   if ((size_t)in_len > remain)
      in_len = (int)remain;
   if (in_len <= 0)
      return 0;

   memcpy(self->buf + self->buf_len, in_buf, (size_t)in_len);
   self->buf_len += (size_t)in_len;
   /* Keep the buffer NUL-terminated for safety; the upstream parser that
    * consumes these buffers also relies on the length. */
   if (self->buf_len == self->buf_cap)
      self->buf[self->buf_len - 1] = '\0';
   else
      self->buf[self->buf_len]     = '\0';
   return in_len;
}

static void discord_json_writer_init(struct discord_json_writer *w,
      char *dest, size_t max_len)
{
   w->buf        = dest;
   w->buf_len    = 0;
   w->buf_cap    = max_len;
   w->need_comma = false;
   w->writer     = rjsonwriter_open_user(discord_json_writer_io, w);
}

static size_t discord_json_writer_size(struct discord_json_writer *w)
{
   rjsonwriter_flush(w->writer);
   return w->buf_len;
}

static void discord_json_writer_free(struct discord_json_writer *w)
{
   rjsonwriter_free(w->writer);
}

static void discord_json_writer_comma(struct discord_json_writer *w)
{
   if (!w->need_comma)
      return;
   rjsonwriter_raw(w->writer, ",", 1);
   w->need_comma = false;
}

static void discord_json_start_object(struct discord_json_writer *w)
{
   discord_json_writer_comma(w);
   rjsonwriter_raw(w->writer, "{", 1);
}

static void discord_json_end_object(struct discord_json_writer *w)
{
   rjsonwriter_raw(w->writer, "}", 1);
   w->need_comma = true;
}

static void discord_json_start_array(struct discord_json_writer *w)
{
   discord_json_writer_comma(w);
   rjsonwriter_raw(w->writer, "[", 1);
}

static void discord_json_end_array(struct discord_json_writer *w)
{
   rjsonwriter_raw(w->writer, "]", 1);
   w->need_comma = true;
}

static void discord_json_key(struct discord_json_writer *w, const char *key)
{
   discord_json_writer_comma(w);
   rjsonwriter_add_string(w->writer, key);
   rjsonwriter_raw(w->writer, ":", 1);
}

static void discord_json_string(struct discord_json_writer *w, const char *val)
{
   discord_json_writer_comma(w);
   rjsonwriter_add_string(w->writer, val);
   w->need_comma = true;
}

static void discord_json_int(struct discord_json_writer *w, int value)
{
   discord_json_writer_comma(w);
   rjsonwriter_rawf(w->writer, "%d", value);
   w->need_comma = true;
}

static void discord_json_int64(struct discord_json_writer *w, int64_t val)
{
   char  num[24];
   char *p_end = num + sizeof(num);
   char *p     = p_end;

   discord_json_writer_comma(w);

   if (val == 0)
      *(--p) = '0';
   else if (val < 0)
   {
      while (val)
      {
         *(--p) = (char)('0' - (val % 10));
         val   /= 10;
      }
      *(--p) = '-';
   }
   else
   {
      while (val)
      {
         *(--p) = (char)('0' + (val % 10));
         val   /= 10;
      }
   }
   rjsonwriter_raw(w->writer, p, (int)(p_end - p));
   w->need_comma = true;
}

static void discord_json_bool(struct discord_json_writer *w, bool value)
{
   discord_json_writer_comma(w);
   if (value)
      rjsonwriter_raw(w->writer, "true", 4);
   else
      rjsonwriter_raw(w->writer, "false", 5);
   w->need_comma = true;
}

static void discord_json_optional_string(struct discord_json_writer *w,
      const char *k, const char *value)
{
   if (value && value[0])
   {
      discord_json_key(w, k);
      discord_json_string(w, value);
   }
}

static void discord_json_write_nonce(struct discord_json_writer *w, int nonce)
{
   char buf[32];
   snprintf(buf, sizeof(buf), "%d", nonce);
   discord_json_key(w, "nonce");
   discord_json_string(w, buf);
}

static size_t discord_json_write_handshake(char *dest, size_t max_len,
      int version, const char *application_id)
{
   size_t                       ret;
   struct discord_json_writer   w;

   discord_json_writer_init(&w, dest, max_len);
   discord_json_start_object(&w);
   discord_json_key(&w, "v");
   discord_json_int(&w, version);
   discord_json_key(&w, "client_id");
   discord_json_string(&w, application_id);
   discord_json_end_object(&w);
   ret = discord_json_writer_size(&w);
   discord_json_writer_free(&w);
   return ret;
}

static size_t discord_json_write_rich_presence(char *dest, size_t max_len,
      int nonce, int pid, const DiscordRichPresence *p)
{
   size_t                     ret;
   struct discord_json_writer w;

   discord_json_writer_init(&w, dest, max_len);
   discord_json_start_object(&w);

   discord_json_write_nonce(&w, nonce);

   discord_json_key(&w, "cmd");
   discord_json_string(&w, "SET_ACTIVITY");

   discord_json_key(&w, "args");
   discord_json_start_object(&w);

   discord_json_key(&w, "pid");
   discord_json_int(&w, pid);

   if (p)
   {
      discord_json_key(&w, "activity");
      discord_json_start_object(&w);

      discord_json_optional_string(&w, "state",   p->state);
      discord_json_optional_string(&w, "details", p->details);

      if (p->startTimestamp || p->endTimestamp)
      {
         discord_json_key(&w, "timestamps");
         discord_json_start_object(&w);
         if (p->startTimestamp)
         {
            discord_json_key(&w, "start");
            discord_json_int64(&w, p->startTimestamp);
         }
         if (p->endTimestamp)
         {
            discord_json_key(&w, "end");
            discord_json_int64(&w, p->endTimestamp);
         }
         discord_json_end_object(&w);
      }

      if (   (p->largeImageKey  && p->largeImageKey[0])
          || (p->largeImageText && p->largeImageText[0])
          || (p->smallImageKey  && p->smallImageKey[0])
          || (p->smallImageText && p->smallImageText[0]))
      {
         discord_json_key(&w, "assets");
         discord_json_start_object(&w);
         discord_json_optional_string(&w, "large_image", p->largeImageKey);
         discord_json_optional_string(&w, "large_text",  p->largeImageText);
         discord_json_optional_string(&w, "small_image", p->smallImageKey);
         discord_json_optional_string(&w, "small_text",  p->smallImageText);
         discord_json_end_object(&w);
      }

      if (   (p->partyId && p->partyId[0])
          ||  p->partySize
          ||  p->partyMax)
      {
         discord_json_key(&w, "party");
         discord_json_start_object(&w);
         discord_json_optional_string(&w, "id", p->partyId);
         if (p->partySize && p->partyMax)
         {
            discord_json_key(&w, "size");
            discord_json_start_array(&w);
            discord_json_int(&w, p->partySize);
            discord_json_int(&w, p->partyMax);
            discord_json_end_array(&w);
         }
         discord_json_end_object(&w);
      }

      if (   (p->matchSecret    && p->matchSecret[0])
          || (p->joinSecret     && p->joinSecret[0])
          || (p->spectateSecret && p->spectateSecret[0]))
      {
         discord_json_key(&w, "secrets");
         discord_json_start_object(&w);
         discord_json_optional_string(&w, "match",    p->matchSecret);
         discord_json_optional_string(&w, "join",     p->joinSecret);
         discord_json_optional_string(&w, "spectate", p->spectateSecret);
         discord_json_end_object(&w);
      }

      discord_json_key(&w, "instance");
      discord_json_bool(&w, p->instance != 0);

      discord_json_end_object(&w); /* activity */
   }

   discord_json_end_object(&w); /* args   */
   discord_json_end_object(&w); /* top    */

   ret = discord_json_writer_size(&w);
   discord_json_writer_free(&w);
   return ret;
}

static size_t discord_json_write_subscribe(char *dest, size_t max_len,
      int nonce, const char *evt_name)
{
   size_t                     ret;
   struct discord_json_writer w;

   discord_json_writer_init(&w, dest, max_len);
   discord_json_start_object(&w);
   discord_json_write_nonce(&w, nonce);
   discord_json_key(&w, "cmd");
   discord_json_string(&w, "SUBSCRIBE");
   discord_json_key(&w, "evt");
   discord_json_string(&w, evt_name);
   discord_json_end_object(&w);
   ret = discord_json_writer_size(&w);
   discord_json_writer_free(&w);
   return ret;
}

static size_t discord_json_write_unsubscribe(char *dest, size_t max_len,
      int nonce, const char *evt_name)
{
   size_t                     ret;
   struct discord_json_writer w;

   discord_json_writer_init(&w, dest, max_len);
   discord_json_start_object(&w);
   discord_json_write_nonce(&w, nonce);
   discord_json_key(&w, "cmd");
   discord_json_string(&w, "UNSUBSCRIBE");
   discord_json_key(&w, "evt");
   discord_json_string(&w, evt_name);
   discord_json_end_object(&w);
   ret = discord_json_writer_size(&w);
   discord_json_writer_free(&w);
   return ret;
}

static size_t discord_json_write_join_reply(char *dest, size_t max_len,
      const char *user_id, int reply, int nonce)
{
   size_t                     ret;
   struct discord_json_writer w;

   discord_json_writer_init(&w, dest, max_len);
   discord_json_start_object(&w);

   discord_json_key(&w, "cmd");
   if (reply == DISCORD_REPLY_YES)
      discord_json_string(&w, "SEND_ACTIVITY_JOIN_INVITE");
   else
      discord_json_string(&w, "CLOSE_ACTIVITY_JOIN_REQUEST");

   discord_json_key(&w, "args");
   discord_json_start_object(&w);
   discord_json_key(&w, "user_id");
   discord_json_string(&w, user_id);
   discord_json_end_object(&w);

   discord_json_write_nonce(&w, nonce);
   discord_json_end_object(&w);

   ret = discord_json_writer_size(&w);
   discord_json_writer_free(&w);
   return ret;
}

/* ------------------------------------------------------------------------ */
/*  JSON reading (formerly JsonReader class)                                 */
/* ------------------------------------------------------------------------ */

/* Walk to the next member name in an object and return it, or NULL at end.
 * On return, *depth is set to the member's nesting depth, which matches the
 * upstream JsonReader.depth semantics. */
static const char *discord_json_next_key(rjson_t *r, unsigned *depth)
{
   for (;;)
   {
      enum rjson_type t = rjson_next(r);
      if (t == RJSON_DONE || t == RJSON_ERROR)
         return NULL;
      if (t != RJSON_STRING)
         continue;
      if (rjson_get_context_type(r) != RJSON_OBJECT)
         continue;
      /* Odd-indexed strings in an object context are keys; even are values. */
      if ((rjson_get_context_count(r) & 1) != 1)
         continue;
      if (depth)
         *depth = rjson_get_context_depth(r);
      return rjson_get_string(r, NULL);
   }
}

static const char *discord_json_next_string(rjson_t *r, const char *default_val)
{
   if (rjson_next(r) != RJSON_STRING)
      return default_val;
   return rjson_get_string(r, NULL);
}

static void discord_json_next_strdup(rjson_t *r, char **out)
{
   if (rjson_next(r) == RJSON_STRING)
   {
      const char *s = rjson_get_string(r, NULL);
      if (s)
         *out = strdup(s);
   }
}

static void discord_json_next_int(rjson_t *r, int *out)
{
   if (rjson_next(r) == RJSON_NUMBER)
      *out = rjson_get_int(r);
}

/* ------------------------------------------------------------------------ */
/*  RPC connection: open/close/read/write frames                             */
/* ------------------------------------------------------------------------ */

/* Forward declarations for the on-connect handler and friends. */
static void discord_on_connect_ready(rjson_t *ready_reader);
static void discord_on_disconnect(int err, const char *message);

static void discord_rpc_close(struct discord_rpc *rpc);

static void discord_rpc_create(const char *application_id)
{
   memset(&discord_rpc_inst, 0, sizeof(discord_rpc_inst));
#ifdef _WIN32
   discord_rpc_inst.pipe = INVALID_HANDLE_VALUE;
#else
   discord_rpc_inst.sock = -1;
#endif
   discord_rpc_inst.state = DISCORD_STATE_DISCONNECTED;
   strlcpy(discord_rpc_inst.app_id, application_id,
         sizeof(discord_rpc_inst.app_id));
   discord_rpc_inst_valid = true;
}

static void discord_rpc_destroy(void)
{
   if (!discord_rpc_inst_valid)
      return;
   discord_rpc_close(&discord_rpc_inst);
   discord_rpc_inst_valid = false;
}

static bool discord_rpc_is_open(const struct discord_rpc *rpc)
{
   return rpc->state == DISCORD_STATE_CONNECTED;
}

static void discord_rpc_close(struct discord_rpc *rpc)
{
   if (   rpc->state == DISCORD_STATE_CONNECTED
       || rpc->state == DISCORD_STATE_SENT_HANDSHAKE)
      discord_on_disconnect(rpc->last_error_code,
            rpc->last_error_message);
   discord_pipe_close(rpc);
   rpc->state = DISCORD_STATE_DISCONNECTED;
}

static bool discord_rpc_write(struct discord_rpc *rpc,
      const void *data, size_t length)
{
   rpc->send_frame.hdr.opcode = DISCORD_OP_FRAME;
   if (length > sizeof(rpc->send_frame.message))
      length = sizeof(rpc->send_frame.message);
   memcpy(rpc->send_frame.message, data, length);
   rpc->send_frame.hdr.length = (uint32_t)length;
   if (!discord_pipe_write(rpc, &rpc->send_frame,
            sizeof(struct discord_frame_header) + length))
   {
      discord_rpc_close(rpc);
      return false;
   }
   return true;
}

/* Read a single frame. On DISCORD_OP_FRAME success, the caller receives
 * an rjson_t* that the caller must free with rjson_free(). On all other
 * outcomes (ping/pong handled internally, close, corrupt, no data) returns
 * NULL. *json_buf_out receives the malloc'd buffer backing the parser so
 * the caller can free it after freeing the parser. */
static rjson_t *discord_rpc_read(struct discord_rpc *rpc, char **json_buf_out)
{
   struct discord_frame read_frame;

   if (   rpc->state != DISCORD_STATE_CONNECTED
       && rpc->state != DISCORD_STATE_SENT_HANDSHAKE)
      return NULL;

   for (;;)
   {
      if (!discord_pipe_read(rpc, &read_frame,
               sizeof(struct discord_frame_header)))
      {
         if (!rpc->is_open)
         {
            rpc->last_error_code = DISCORD_ERR_PIPE_CLOSED;
            strlcpy(rpc->last_error_message, "Pipe closed",
                  sizeof(rpc->last_error_message));
            discord_rpc_close(rpc);
         }
         return NULL;
      }

      if (read_frame.hdr.length > 0)
      {
         if (read_frame.hdr.length > sizeof(read_frame.message) - 1)
         {
            rpc->last_error_code = DISCORD_ERR_READ_CORRUPT;
            strlcpy(rpc->last_error_message, "Frame too large",
                  sizeof(rpc->last_error_message));
            discord_rpc_close(rpc);
            return NULL;
         }
         if (!discord_pipe_read(rpc, read_frame.message,
                  read_frame.hdr.length))
         {
            rpc->last_error_code = DISCORD_ERR_READ_CORRUPT;
            strlcpy(rpc->last_error_message, "Partial data in frame",
                  sizeof(rpc->last_error_message));
            discord_rpc_close(rpc);
            return NULL;
         }
         read_frame.message[read_frame.hdr.length] = '\0';
      }

      switch (read_frame.hdr.opcode)
      {
         case DISCORD_OP_CLOSE:
            {
               /* Parse the close payload for code / message, then close. */
               rjson_t *r = rjson_open_buffer(read_frame.message,
                     read_frame.hdr.length);
               rpc->last_error_code       = 0;
               rpc->last_error_message[0] = '\0';
               if (r)
               {
                  unsigned    depth;
                  const char *key;
                  while ((key = discord_json_next_key(r, &depth)))
                  {
                     if (depth == 1)
                     {
                        if (!strcmp(key, "code"))
                           discord_json_next_int(r, &rpc->last_error_code);
                        else if (!strcmp(key, "message"))
                        {
                           const char *s = discord_json_next_string(r, "");
                           strlcpy(rpc->last_error_message, s,
                                 sizeof(rpc->last_error_message));
                        }
                     }
                  }
                  rjson_free(r);
               }
               discord_rpc_close(rpc);
               return NULL;
            }

         case DISCORD_OP_FRAME:
            {
               /* The rjson parser does not copy the buffer; we need to keep
                * the bytes alive for the duration of parsing. Duplicate the
                * message so the caller can free both independently. */
               size_t  len = read_frame.hdr.length;
               char   *buf = (char *)malloc(len + 1);
               rjson_t *r;
               if (!buf)
                  return NULL;
               memcpy(buf, read_frame.message, len);
               buf[len] = '\0';
               r = rjson_open_buffer(buf, len);
               if (!r)
               {
                  free(buf);
                  return NULL;
               }
               *json_buf_out = buf;
               return r;
            }

         case DISCORD_OP_PING:
            /* Echo back as PONG */
            read_frame.hdr.opcode = DISCORD_OP_PONG;
            if (!discord_pipe_write(rpc, &read_frame,
                     sizeof(struct discord_frame_header)
                        + read_frame.hdr.length))
               discord_rpc_close(rpc);
            break;

         case DISCORD_OP_PONG:
            break;

         case DISCORD_OP_HANDSHAKE:
         default:
            rpc->last_error_code = DISCORD_ERR_READ_CORRUPT;
            strlcpy(rpc->last_error_message, "Bad ipc frame",
                  sizeof(rpc->last_error_message));
            discord_rpc_close(rpc);
            return NULL;
      }
   }
}

static void discord_rpc_open(struct discord_rpc *rpc)
{
   if (rpc->state == DISCORD_STATE_CONNECTED)
      return;

   if (rpc->state == DISCORD_STATE_DISCONNECTED)
   {
      if (!discord_pipe_open(rpc))
         return;
   }

   if (rpc->state == DISCORD_STATE_SENT_HANDSHAKE)
   {
      char    *json_buf = NULL;
      rjson_t *r        = discord_rpc_read(rpc, &json_buf);
      if (r)
      {
         bool        cmd_dispatch = false;
         bool        evt_ready    = false;
         unsigned    depth        = 0;
         const char *key;

         while ((key = discord_json_next_key(r, &depth)))
         {
            if (depth == 1)
            {
               if (!strcmp(key, "cmd"))
               {
                  const char *v = discord_json_next_string(r, "");
                  cmd_dispatch = !strcmp(v, "DISPATCH");
               }
               else if (!strcmp(key, "evt"))
               {
                  const char *v = discord_json_next_string(r, "");
                  evt_ready = !strcmp(v, "READY");
               }
            }
         }

         if (cmd_dispatch && evt_ready)
         {
            rpc->state = DISCORD_STATE_CONNECTED;
            /* Re-open the parser for the ready handler so it can walk the
             * document from the beginning to pull user info. */
            rjson_free(r);
            r = rjson_open_buffer(json_buf, strlen(json_buf));
            if (r)
               discord_on_connect_ready(r);
         }
         if (r)
            rjson_free(r);
      }
      if (json_buf)
         free(json_buf);
   }
   else
   {
      rpc->send_frame.hdr.opcode = DISCORD_OP_HANDSHAKE;
      rpc->send_frame.hdr.length = (uint32_t)discord_json_write_handshake(
            rpc->send_frame.message,
            sizeof(rpc->send_frame.message),
            DISCORD_RPC_VERSION,
            rpc->app_id);

      if (discord_pipe_write(rpc, &rpc->send_frame,
               sizeof(struct discord_frame_header) + rpc->send_frame.hdr.length))
         rpc->state = DISCORD_STATE_SENT_HANDSHAKE;
      else
         discord_rpc_close(rpc);
   }
}

/* ------------------------------------------------------------------------ */
/*  Queues: send queue and join-request queue                                */
/* ------------------------------------------------------------------------ */

static struct discord_queued_msg *discord_send_queue_next_add(void)
{
   unsigned idx;
   if (discord_send_pending >= DISCORD_SEND_QUEUE_SIZE)
      return NULL;
   idx = (discord_send_next_add++) % DISCORD_SEND_QUEUE_SIZE;
   return &discord_send_queue[idx];
}

static void discord_send_queue_commit_add(void)
{
   discord_send_pending++;
}

static bool discord_send_queue_has_pending(void)
{
   return discord_send_pending != 0;
}

static struct discord_queued_msg *discord_send_queue_next_send(void)
{
   unsigned idx = (discord_send_next_send++) % DISCORD_SEND_QUEUE_SIZE;
   return &discord_send_queue[idx];
}

static void discord_send_queue_commit_send(void)
{
   discord_send_pending--;
}

static struct discord_join_user *discord_join_queue_next_add(void)
{
   unsigned idx;
   if (discord_join_pending >= DISCORD_JOIN_QUEUE_SIZE)
      return NULL;
   idx = (discord_join_next_add++) % DISCORD_JOIN_QUEUE_SIZE;
   return &discord_join_queue[idx];
}

static void discord_join_queue_commit_add(void)
{
   discord_join_pending++;
}

static bool discord_join_queue_has_pending(void)
{
   return discord_join_pending != 0;
}

static struct discord_join_user *discord_join_queue_next_send(void)
{
   unsigned idx = (discord_join_next_send++) % DISCORD_JOIN_QUEUE_SIZE;
   return &discord_join_queue[idx];
}

static void discord_join_queue_commit_send(void)
{
   discord_join_pending--;
}

/* ------------------------------------------------------------------------ */
/*  Event subscription (joinGame / spectateGame / joinRequest)               */
/* ------------------------------------------------------------------------ */

static bool discord_register_for_event(const char *evt_name)
{
   struct discord_queued_msg *q = discord_send_queue_next_add();
   if (!q)
      return false;
   q->length = discord_json_write_subscribe(
         q->buffer, sizeof(q->buffer), discord_nonce++, evt_name);
   discord_send_queue_commit_add();
   return true;
}

static bool discord_deregister_for_event(const char *evt_name)
{
   struct discord_queued_msg *q = discord_send_queue_next_add();
   if (!q)
      return false;
   q->length = discord_json_write_unsubscribe(
         q->buffer, sizeof(q->buffer), discord_nonce++, evt_name);
   discord_send_queue_commit_add();
   return true;
}

/* ------------------------------------------------------------------------ */
/*  Connection callbacks (on READY, on disconnect)                           */
/* ------------------------------------------------------------------------ */

static void discord_on_connect_ready(rjson_t *r)
{
   char       *user_id       = NULL;
   char       *username      = NULL;
   char       *avatar        = NULL;
   char       *discriminator = NULL;
   bool        in_data       = false;
   bool        in_user       = false;
   unsigned    depth;
   const char *key;

   /* Re-enable any queued handler subscriptions now that we are connected. */
   Discord_UpdateHandlers(&discord_queued_handlers);

   while ((key = discord_json_next_key(r, &depth)))
   {
      if (depth == 1)
      {
         in_data = !strcmp(key, "data");
         in_user = false;
      }
      else if (depth == 2 && in_data)
      {
         in_user = !strcmp(key, "user");
      }
      else if (depth == 3 && in_user)
      {
         if      (!strcmp(key, "id"))            discord_json_next_strdup(r, &user_id);
         else if (!strcmp(key, "username"))      discord_json_next_strdup(r, &username);
         else if (!strcmp(key, "avatar"))        discord_json_next_strdup(r, &avatar);
         else if (!strcmp(key, "discriminator")) discord_json_next_strdup(r, &discriminator);
      }
   }

   if (user_id && username)
   {
      strlcpy(discord_connected.user_id, user_id,
            sizeof(discord_connected.user_id));
      strlcpy(discord_connected.username, username,
            sizeof(discord_connected.username));
      if (discriminator)
         strlcpy(discord_connected.discriminator, discriminator,
               sizeof(discord_connected.discriminator));
      if (avatar)
         strlcpy(discord_connected.avatar, avatar,
               sizeof(discord_connected.avatar));
      else
         discord_connected.avatar[0] = '\0';
   }

   discord_was_just_connected = true;
   discord_backoff_current = DISCORD_BACKOFF_MIN_MS;

   if (user_id)       free(user_id);
   if (username)      free(username);
   if (avatar)        free(avatar);
   if (discriminator) free(discriminator);
}

static void discord_on_disconnect(int err, const char *message)
{
   discord_last_disconnect_error_code = err;
   strlcpy(discord_last_disconnect_message, message ? message : "",
         sizeof(discord_last_disconnect_message));
   memset(&discord_handlers, 0, sizeof(discord_handlers));
   discord_was_just_disconnected = true;
   discord_next_connect_ms = discord_now_ms() + discord_backoff_next_delay();
}

/* ------------------------------------------------------------------------ */
/*  Main pump: reads a batch of frames, flushes queued writes                */
/* ------------------------------------------------------------------------ */

static void discord_update_connection(void)
{
   if (!discord_rpc_inst_valid)
      return;

   if (!discord_rpc_is_open(&discord_rpc_inst))
   {
      if (discord_now_ms() >= discord_next_connect_ms)
      {
         discord_next_connect_ms = discord_now_ms() + discord_backoff_next_delay();
         discord_rpc_open(&discord_rpc_inst);
      }
      return;
   }

   /* Reads --------------------------------------------------------------- */
   for (;;)
   {
      char       *json_buf      = NULL;
      rjson_t    *r             = discord_rpc_read(&discord_rpc_inst, &json_buf);
      char       *evt_name      = NULL;
      char       *nonce         = NULL;
      char       *secret        = NULL;
      char       *user_id       = NULL;
      char       *username      = NULL;
      char       *avatar        = NULL;
      char       *discriminator = NULL;
      char       *error_message = NULL;
      int         error_code    = 0;
      bool        in_data       = false;
      bool        in_user       = false;
      unsigned    depth;
      const char *key;

      if (!r)
         break;

      while ((key = discord_json_next_key(r, &depth)))
      {
         if (depth == 1)
         {
            in_data = false;
            in_user = false;
            if      (!strcmp(key, "evt"))   discord_json_next_strdup(r, &evt_name);
            else if (!strcmp(key, "nonce")) discord_json_next_strdup(r, &nonce);
            else if (!strcmp(key, "data"))  in_data = true;
         }
         else if (depth == 2 && in_data)
         {
            in_user = false;
            if      (!strcmp(key, "code"))    discord_json_next_int(r, &error_code);
            else if (!strcmp(key, "message")) discord_json_next_strdup(r, &error_message);
            else if (!strcmp(key, "secret"))  discord_json_next_strdup(r, &secret);
            else if (!strcmp(key, "user"))    in_user = true;
         }
         else if (depth == 3 && in_user)
         {
            if      (!strcmp(key, "id"))            discord_json_next_strdup(r, &user_id);
            else if (!strcmp(key, "username"))      discord_json_next_strdup(r, &username);
            else if (!strcmp(key, "avatar"))        discord_json_next_strdup(r, &avatar);
            else if (!strcmp(key, "discriminator")) discord_json_next_strdup(r, &discriminator);
         }
      }

      if (nonce)
      {
         /* Reply to one of our commands; only surface ERROR events. */
         if (evt_name && !strcmp(evt_name, "ERROR"))
         {
            discord_last_error_code = error_code;
            strlcpy(discord_last_error_message, error_message ? error_message : "",
                  sizeof(discord_last_error_message));
            discord_got_error_message = true;
         }
      }
      else if (evt_name)
      {
         if (!strcmp(evt_name, "ACTIVITY_JOIN"))
         {
            if (secret)
            {
               strlcpy(discord_join_game_secret, secret,
                     sizeof(discord_join_game_secret));
               discord_was_join_game = true;
            }
         }
         else if (!strcmp(evt_name, "ACTIVITY_SPECTATE"))
         {
            if (secret)
            {
               strlcpy(discord_spectate_game_secret, secret,
                     sizeof(discord_spectate_game_secret));
               discord_was_spectate_game = true;
            }
         }
         else if (!strcmp(evt_name, "ACTIVITY_JOIN_REQUEST"))
         {
            struct discord_join_user *req = discord_join_queue_next_add();
            if (user_id && username && req)
            {
               strlcpy(req->user_id,  user_id,  sizeof(req->user_id));
               strlcpy(req->username, username, sizeof(req->username));
               if (discriminator)
                  strlcpy(req->discriminator, discriminator,
                        sizeof(req->discriminator));
               else
                  req->discriminator[0] = '\0';
               if (avatar)
                  strlcpy(req->avatar, avatar, sizeof(req->avatar));
               else
                  req->avatar[0] = '\0';
               discord_join_queue_commit_add();
            }
         }
      }

      if (evt_name)      free(evt_name);
      if (nonce)         free(nonce);
      if (secret)        free(secret);
      if (user_id)       free(user_id);
      if (username)      free(username);
      if (avatar)        free(avatar);
      if (discriminator) free(discriminator);
      if (error_message) free(error_message);

      rjson_free(r);
      if (json_buf)
         free(json_buf);
   }

   /* Writes -------------------------------------------------------------- */
   if (discord_queued_presence.length)
   {
      struct discord_queued_msg local;
      local.length = discord_queued_presence.length;
      memcpy(local.buffer, discord_queued_presence.buffer, local.length);
      discord_queued_presence.length = 0;

      if (!discord_rpc_write(&discord_rpc_inst, local.buffer, local.length))
      {
         /* On failure, requeue for a later attempt. */
         discord_queued_presence.length = local.length;
         memcpy(discord_queued_presence.buffer, local.buffer, local.length);
      }
   }

   while (discord_send_queue_has_pending())
   {
      struct discord_queued_msg *q = discord_send_queue_next_send();
      discord_rpc_write(&discord_rpc_inst, q->buffer, q->length);
      discord_send_queue_commit_send();
   }
}

/* ------------------------------------------------------------------------ */
/*  Public Discord_* API (stable, called by discord_init / discord_update /  */
/*  runloop.c / retroarch.c)                                                 */
/* ------------------------------------------------------------------------ */

/* No-op. Auto-register is disabled in this fold; Discord_Initialize is called
 * with autoRegister=0 from discord_init() and discord_init() itself no longer
 * relies on any side effects of this call. It remains exported because
 * discord_init() still references it. */
void Discord_Register(const char *application_id, const char *command)
{
   (void)application_id;
   (void)command;
}

/* Kept for symmetry with the upstream API. Not currently referenced. */
void Discord_RegisterSteamGame(const char *application_id, const char *steam_id)
{
   (void)application_id;
   (void)steam_id;
}

void Discord_Initialize(const char *application_id,
      DiscordEventHandlers *handlers,
      int auto_register,
      const char *optional_steam_id)
{
   (void)auto_register;
   (void)optional_steam_id;

   /* Seed the cheap backoff jitter once. */
   srand((unsigned)time(NULL));

#ifdef _WIN32
   discord_pid = (int)GetCurrentProcessId();
#else
   discord_pid = (int)getpid();
#endif

   if (handlers)
      discord_queued_handlers = *handlers;
   else
      memset(&discord_queued_handlers, 0, sizeof(discord_queued_handlers));
   memset(&discord_handlers, 0, sizeof(discord_handlers));

   if (discord_rpc_inst_valid)
      return;

   discord_rpc_create(application_id);
   discord_next_connect_ms = discord_now_ms();
}

void Discord_Shutdown(void)
{
   if (!discord_rpc_inst_valid)
      return;
   memset(&discord_handlers, 0, sizeof(discord_handlers));
   discord_rpc_destroy();
}

void Discord_UpdateConnection(void)
{
   discord_update_connection();
}

void Discord_UpdatePresence(const DiscordRichPresence *presence)
{
   discord_queued_presence.length = discord_json_write_rich_presence(
         discord_queued_presence.buffer,
         sizeof(discord_queued_presence.buffer),
         discord_nonce++, discord_pid, presence);
}

void Discord_ClearPresence(void)
{
   Discord_UpdatePresence(NULL);
}

void Discord_Respond(const char *user_id, int reply)
{
   struct discord_queued_msg *q;

   if (!discord_rpc_inst_valid || !discord_rpc_is_open(&discord_rpc_inst))
      return;

   q = discord_send_queue_next_add();
   if (!q)
      return;
   q->length = discord_json_write_join_reply(
         q->buffer, sizeof(q->buffer), user_id, reply, discord_nonce++);
   discord_send_queue_commit_add();
}

void Discord_RunCallbacks(void)
{
   bool was_disconnected;
   bool is_connected;

   if (!discord_rpc_inst_valid)
      return;

   was_disconnected              = discord_was_just_disconnected;
   discord_was_just_disconnected = false;
   is_connected                  = discord_rpc_is_open(&discord_rpc_inst);

   /* If we are currently connected, fire the disconnect callback first so
    * the outward-facing sequence is always ready -> ... -> disconnected. */
   if (is_connected)
   {
      if (was_disconnected && discord_handlers.disconnected)
         discord_handlers.disconnected(discord_last_disconnect_error_code,
               discord_last_disconnect_message);
   }

   if (discord_was_just_connected)
   {
      discord_was_just_connected = false;
      if (discord_handlers.ready)
      {
         DiscordUser du;
         du.userId        = discord_connected.user_id;
         du.username      = discord_connected.username;
         du.discriminator = discord_connected.discriminator;
         du.avatar        = discord_connected.avatar;
         discord_handlers.ready(&du);
      }
   }

   if (discord_got_error_message)
   {
      discord_got_error_message = false;
      if (discord_handlers.errored)
         discord_handlers.errored(discord_last_error_code,
               discord_last_error_message);
   }

   if (discord_was_join_game)
   {
      discord_was_join_game = false;
      if (discord_handlers.joinGame)
         discord_handlers.joinGame(discord_join_game_secret);
   }

   if (discord_was_spectate_game)
   {
      discord_was_spectate_game = false;
      if (discord_handlers.spectateGame)
         discord_handlers.spectateGame(discord_spectate_game_secret);
   }

   while (discord_join_queue_has_pending())
   {
      struct discord_join_user *req = discord_join_queue_next_send();
      if (discord_handlers.joinRequest)
      {
         DiscordUser du;
         du.userId        = req->user_id;
         du.username      = req->username;
         du.discriminator = req->discriminator;
         du.avatar        = req->avatar;
         discord_handlers.joinRequest(&du);
      }
      discord_join_queue_commit_send();
   }

   if (!is_connected)
   {
      if (was_disconnected && discord_handlers.disconnected)
         discord_handlers.disconnected(discord_last_disconnect_error_code,
               discord_last_disconnect_message);
   }
}

void Discord_UpdateHandlers(DiscordEventHandlers *new_handlers)
{
   if (!new_handlers)
   {
      memset(&discord_handlers, 0, sizeof(discord_handlers));
      return;
   }

#define DISCORD_HANDLE_EVENT_REG(field, event)                                \
   do {                                                                       \
      if      (!discord_handlers.field &&  new_handlers->field)               \
         discord_register_for_event(event);                                   \
      else if ( discord_handlers.field && !new_handlers->field)               \
         discord_deregister_for_event(event);                                 \
   } while (0)

   DISCORD_HANDLE_EVENT_REG(joinGame,     "ACTIVITY_JOIN");
   DISCORD_HANDLE_EVENT_REG(spectateGame, "ACTIVITY_SPECTATE");
   DISCORD_HANDLE_EVENT_REG(joinRequest,  "ACTIVITY_JOIN_REQUEST");

#undef DISCORD_HANDLE_EVENT_REG

   discord_handlers = *new_handlers;
}

/* ======================================================================== */
/*  RetroArch-facing discord_* API (unchanged behavior from the original     */
/*  network/discord.c)                                                       */
/* ======================================================================== */

static discord_state_t discord_state_st = {0}; /* int64_t alignment */

discord_state_t *discord_state_get_ptr(void)
{
   return &discord_state_st;
}

bool discord_is_ready(void)
{
   discord_state_t *discord_st = &discord_state_st;
   return discord_st->ready;
}

char *discord_get_own_username(void)
{
   discord_state_t *discord_st = &discord_state_st;
   if (discord_st->ready)
      return discord_st->user_name;
   return NULL;
}

char *discord_get_own_avatar(void)
{
   discord_state_t *discord_st = &discord_state_st;
   if (discord_st->ready)
      return discord_st->user_avatar;
   return NULL;
}

bool discord_avatar_is_ready(void)
{
   return false;
}

void discord_avatar_set_ready(bool ready)
{
   discord_state_t *discord_st = &discord_state_st;
   discord_st->avatar_ready    = ready;
}

#ifdef HAVE_MENU
static bool discord_download_avatar(
      discord_state_t *discord_st,
      const char *user_id, const char *avatar_id)
{
   static char url[PATH_MAX_LENGTH];
   static char url_encoded[PATH_MAX_LENGTH];
   static char full_path[PATH_MAX_LENGTH];
   static char buf[PATH_MAX_LENGTH];
   file_transfer_t *transf = NULL;

   fill_pathname_application_special(buf,
         sizeof(buf),
         APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
   fill_pathname_join_special(full_path, buf, avatar_id, sizeof(full_path));
   strlcpy(discord_st->user_avatar,
         avatar_id, sizeof(discord_st->user_avatar));

   if (path_is_valid(full_path))
      return true;

   if (!avatar_id || !*avatar_id)
      return false;

   snprintf(url, sizeof(url), "%s/%s/%s" FILE_PATH_PNG_EXTENSION,
         CDN_URL, user_id, avatar_id);
   net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));
   snprintf(buf, sizeof(buf), "%s" FILE_PATH_PNG_EXTENSION, avatar_id);

   transf            = (file_transfer_t *)malloc(sizeof(*transf));
   transf->enum_idx  = MENU_ENUM_LABEL_CB_DISCORD_AVATAR;
   strlcpy(transf->path, buf, sizeof(transf->path));
   transf->user_data = NULL;

   task_push_http_transfer_file(url_encoded, true, NULL,
         cb_generic_download, transf);
   return false;
}
#endif

static void handle_discord_ready(const DiscordUser *connected_user)
{
   discord_state_t *discord_st = &discord_state_st;
   strlcpy(discord_st->user_name,
         connected_user->username, sizeof(discord_st->user_name));

#ifdef HAVE_MENU
   discord_download_avatar(discord_st,
         connected_user->userId, connected_user->avatar);
#endif
}

static void handle_discord_disconnected(int errcode, const char *msg) { (void)errcode; (void)msg; }
static void handle_discord_err(int errcode, const char *msg)          { (void)errcode; (void)msg; }
static void handle_discord_spectate(const char *secret)                { (void)secret; }

static void handle_discord_join_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   char                       hostname[512];
   struct netplay_room       *room;
   char                      *room_data  = NULL;
   http_transfer_data_t      *data       = (http_transfer_data_t *)task_data;
   discord_state_t           *discord_st = &discord_state_st;

   if (err || !data || !data->data || !data->len || data->status != 200)
   {
      free(user_data);
      return;
   }
   if (!(room_data = (char *)malloc(data->len + 1)))
   {
      free(user_data);
      return;
   }
   memcpy(room_data, data->data, data->len);
   room_data[data->len] = '\0';

   netplay_rooms_parse(room_data, strlen(room_data));
   free(room_data);

   if ((room = netplay_room_get(0)))
   {
      if (room->host_method == NETPLAY_HOST_METHOD_MITM)
         snprintf(hostname, sizeof(hostname), "%s|%d|%s",
               room->mitm_address, room->mitm_port, room->mitm_session);
      else
         snprintf(hostname, sizeof(hostname), "%s|%d",
               room->address, room->port);

      discord_st->connecting = true;
      if (discord_st->ready)
         discord_update(PRESENCE_NETPLAY_CLIENT);

      task_push_netplay_crc_scan(room->gamecrc, room->gamename,
            room->subsystem_name, room->corename, hostname);
   }

   netplay_rooms_free();
   free(user_data);
}

static void handle_discord_join(const char *secret)
{
   int  room_id;
   char url[512];

   if ((room_id = (int)strtol(secret, NULL, 10)))
   {
      size_t           _len;
      discord_state_t *discord_st = &discord_state_st;

      snprintf(discord_st->peer_party_id,
            sizeof(discord_st->peer_party_id),
            "%d", room_id);

      _len = strlcpy(url, FILE_PATH_LOBBY_LIBRETRO_URL, sizeof(url));
      strlcpy(url + _len,
            discord_st->peer_party_id,
            sizeof(url) - _len);

      task_push_http_transfer(url, true, NULL, handle_discord_join_cb, NULL);
   }
}

static void handle_discord_join_request(const DiscordUser *request)
{
#ifdef HAVE_MENU
   discord_state_t *discord_st = &discord_state_st;
   discord_download_avatar(discord_st, request->userId, request->avatar);
#else
   (void)request;
#endif
}

void discord_update(enum presence presence)
{
   discord_state_t *discord_st = &discord_state_st;

   if (presence == discord_st->status)
      return;

   if (    !discord_st->connecting
        && (   presence == PRESENCE_NONE
            || presence == PRESENCE_MENU))
   {
      memset(&discord_st->presence, 0, sizeof(discord_st->presence));
      discord_st->peer_party_id[0] = '\0';
   }

   switch (presence)
   {
      case PRESENCE_MENU:
         discord_st->presence.details        = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU);
         discord_st->presence.largeImageKey  = "base";
         discord_st->presence.largeImageText = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_NO_CORE);
         discord_st->presence.instance       = 0;
         break;
      case PRESENCE_GAME_PAUSED:
         discord_st->presence.smallImageKey  = "paused";
         discord_st->presence.smallImageText = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED);
         discord_st->presence.details        = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED);
         discord_st->pause_time              = time(0);
         discord_st->elapsed_time            = difftime(discord_st->pause_time,
               discord_st->start_time);
         discord_st->presence.startTimestamp = discord_st->pause_time;
         break;
      case PRESENCE_GAME:
         {
            core_info_t *core_info = NULL;
            core_info_get_current_core(&core_info);

            if (core_info)
            {
               const char                   *system_id        =
                    core_info->system_id
                  ? core_info->system_id
                  : "core";
               const char                   *label            = NULL;
               const struct playlist_entry  *entry            = NULL;
               playlist_t                   *current_playlist = playlist_get_cached();

               if (current_playlist)
               {
                  playlist_get_index_by_path(
                        current_playlist,
                        path_get(RARCH_PATH_CONTENT),
                        &entry);

                  if (entry && (entry->label && *entry->label))
                     label = entry->label;
               }

               if (!label)
                  label = path_basename(path_get(RARCH_PATH_BASENAME));
               strlcpy(discord_st->game_image_key, system_id,
                     sizeof(discord_st->game_image_key));
               discord_st->presence.largeImageKey = discord_st->game_image_key;

               if (core_info->display_name)
               {
                  strlcpy(discord_st->game_image_text, core_info->display_name,
                        sizeof(discord_st->game_image_text));
                  discord_st->presence.largeImageText = discord_st->game_image_text;
               }

               discord_st->start_time           = time(0);
               if (discord_st->pause_time != 0)
                  discord_st->start_time        = time(0) - discord_st->elapsed_time;

               discord_st->pause_time              = 0;
               discord_st->elapsed_time            = 0;

               discord_st->presence.smallImageKey  = "playing";
               discord_st->presence.smallImageText = msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING);
               discord_st->presence.startTimestamp = discord_st->start_time;

#ifdef HAVE_CHEEVOS
               if (rcheevos_get_game_badge_url(
                        discord_st->cheevos_badge_url,
                        sizeof(discord_st->cheevos_badge_url)))
                  discord_st->presence.largeImageKey =
                        discord_st->cheevos_badge_url;

               if (rcheevos_get_richpresence(
                        discord_st->cheevos_richpresence,
                        sizeof(discord_st->cheevos_richpresence)) > 0)
                  discord_st->presence.details =
                        discord_st->cheevos_richpresence;
               else
#endif
                  discord_st->presence.details = msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME);

               strlcpy(discord_st->game_state, label,
                     sizeof(discord_st->game_state));
               discord_st->presence.state    = discord_st->game_state;
               discord_st->presence.instance = 0;

               if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
               {
                  discord_st->peer_party_id[0]    = '\0';
                  discord_st->connecting          = false;
                  discord_st->presence.partyId    = NULL;
                  discord_st->presence.partyMax   = 0;
                  discord_st->presence.partySize  = 0;
                  discord_st->presence.joinSecret = (const char *)'\0';
               }
            }
         }
         break;
      case PRESENCE_NETPLAY_HOSTING:
         {
            char                 join_secret[128];
            struct netplay_room *room = &networking_state_get_ptr()->host_room;
            if (room->id == 0)
               return;

            snprintf(discord_st->self_party_id,
                  sizeof(discord_st->self_party_id), "%d", room->id);
            snprintf(join_secret,
                  sizeof(join_secret), "%d|%" PRId64,
                  room->id, cpu_features_get_time_usec());

            discord_st->presence.joinSecret = strdup(join_secret);
            discord_st->presence.partyId    = strdup(discord_st->self_party_id);
            discord_st->presence.partyMax   = 2;
            discord_st->presence.partySize  = 1;
         }
         break;
      case PRESENCE_NETPLAY_CLIENT:
         discord_st->presence.partyId = strdup(discord_st->peer_party_id);
         break;
      case PRESENCE_NETPLAY_NETPLAY_STOPPED:
         if (   !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL)
             && !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
         {
            discord_st->peer_party_id[0]    = '\0';
            discord_st->connecting          = false;
            discord_st->presence.partyId    = NULL;
            discord_st->presence.partyMax   = 0;
            discord_st->presence.partySize  = 0;
            discord_st->presence.joinSecret = (const char *)'\0';
         }
         break;
#ifdef HAVE_CHEEVOS
      case PRESENCE_RETROACHIEVEMENTS:
         if (discord_st->pause_time)
            return;

         if (rcheevos_get_game_badge_url(
                  discord_st->cheevos_badge_url,
                  sizeof(discord_st->cheevos_badge_url)))
            discord_st->presence.largeImageKey =
                  discord_st->cheevos_badge_url;

         if (rcheevos_get_richpresence(
                  discord_st->cheevos_richpresence,
                  sizeof(discord_st->cheevos_richpresence)) > 0)
            discord_st->presence.details =
                  discord_st->cheevos_richpresence;
         presence = PRESENCE_GAME;
         break;
#endif
      case PRESENCE_SHUTDOWN:
         discord_st->presence.partyId    = NULL;
         discord_st->presence.partyMax   = 0;
         discord_st->presence.partySize  = 0;
         discord_st->presence.joinSecret = (const char *)'\0';
         discord_st->connecting          = false;
         /* fall-through */
      default:
         break;
   }

   Discord_UpdatePresence(&discord_st->presence);
   /* Single-threaded model: pump the connection synchronously after each
    * presence update so the write makes it out without waiting for the next
    * runloop tick. */
   Discord_UpdateConnection();

   discord_st->status = presence;
}

void discord_init(const char *discord_app_id, char *args)
{
   DiscordEventHandlers handlers;
   discord_state_t     *discord_st = &discord_state_st;

   discord_st->start_time = time(0);

   handlers.ready         = handle_discord_ready;
   handlers.disconnected  = handle_discord_disconnected;
   handlers.errored       = handle_discord_err;
   handlers.joinGame      = handle_discord_join;
   handlers.spectateGame  = handle_discord_spectate;
   handlers.joinRequest   = handle_discord_join_request;

   Discord_Initialize(discord_app_id, &handlers, 0, NULL);
   Discord_UpdateConnection();

   discord_st->ready = true;
}
