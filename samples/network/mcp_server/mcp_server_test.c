/* Regression test for the embedded MCP server (network/mcp_server.c).
 *
 * The server is built as it ships, against a small command table of
 * its own (command_run / command_action_list / command_hotkey_list are
 * stubbed here), and driven over loopback by a client in the same
 * thread: each request is sent, then the server polled until it has
 * answered and closed.  Covered: the protocol handshake, the tool list
 * and its hints, tool calls answered at once and later, the JSON-RPC
 * errors, the parameters a tool declares and the line they are put
 * back into, the rows a reply is read into beside the text it leaves
 * untouched and the outputSchema describing them, a later answer
 * carrying an image (SCREENSHOT's), the flag
 * that leaves a command out of the tool list, the HTTP gate (method,
 * path, Origin, the bearer token the server will not start without), a
 * body larger than the server takes, a request arriving in pieces, and
 * a waiting request answered when the server is torn down.
 *
 * Whether a reply is a failure is the command's own word (command_t's
 * error), not something read out of the text: a path that holds ERROR
 * is no error, and a failure whose text holds none still is one.
 *
 * Builds with gcc/clang on Linux and with mingw-w64 (run under Wine):
 * only libretro-common's sockets are used. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <formats/rjson.h>
#include <features/features_cpu.h>
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#define read  _read
#define write _write
#define close _close
#define dup2  _dup2
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "../../../command.h"
#include "../../../network/mcp_server.h"

static int failures;
static void check(const char *what, bool ok)
{
   printf("   %-62s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      failures++;
}

/* ---- the command table the server is put in front of ---- */

static command_t *later_cmd;
static void      *later_dest;

/* how many times PING ran */
static unsigned pings = 0;
static bool act_ping(command_t *cmd, const char *arg)
{ (void)arg; pings++; cmd->replier(cmd, "PONG\n", 5); return true; }
/* a path with an accent, then a character cut short, as a log line
 * truncated mid-character ends */
#define MOJI_TEXT "caf\xC3\xA9 \xE6\x97\n"
static bool act_moji(command_t *cmd, const char *arg)
{ (void)arg; cmd->replier(cmd, MOJI_TEXT, sizeof(MOJI_TEXT) - 1); return true; }
/* a status with a comma in the name, and a read, as the real commands
 * answer */
#define STAT_TEXT "GET_STATUS PLAYING nes,Mario, Luigi & Co.nes\n"
#define READ_TEXT "READ_CORE_MEMORY 1000 AA 0F 10\n"
static bool act_stat(command_t *cmd, const char *arg)
{ (void)arg; cmd->replier(cmd, STAT_TEXT, sizeof(STAT_TEXT) - 1); return true; }
static bool act_read(command_t *cmd, const char *arg)
{ (void)arg; cmd->replier(cmd, READ_TEXT, sizeof(READ_TEXT) - 1); return true; }
static bool act_wipe(command_t *cmd, const char *arg)
{ (void)cmd; (void)arg; return true; }
static bool act_fail(command_t *cmd, const char *arg)
{ (void)cmd; (void)arg; return false; }
static bool act_bad(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->error = true;
   cmd->replier(cmd, "BAD ERROR no such thing\n", 24);
   return false;
}
/* fails, and its text says nothing about it */
static bool act_quiet_fail(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->error = true;
   cmd->replier(cmd, "nothing to save\n", 16);
   return false;
}
/* answers after it returns, as SCREENSHOT does */
static bool act_later(command_t *cmd, const char *arg)
{
   (void)arg;
   later_cmd  = cmd;
   later_dest = cmd->reply_dest ? cmd->reply_dest(cmd) : NULL;
   return later_dest != NULL;
}

/* "WIPE <what>", and "JOIN <a>|<b> [n]" for the separators and the
 * types: a lead of its own, an optional parameter, an integer, and a
 * set the schema offers as an enum. */
static const struct cmd_param test_params_wipe[] = {
   { "what", "What to wipe.", CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { NULL }
};
static const struct cmd_param test_params_join[] = {
   { "a", "The first.", CMD_PARAM_STRING, true, NULL, NULL, NULL },
   { "b", "The second.", CMD_PARAM_STRING, true, "|", NULL, NULL },
   { "n", "How many.", CMD_PARAM_INT, false, NULL, NULL, "0" },
   { "mode", "Which way.", CMD_PARAM_STRING, false, NULL, "fast|slow", NULL },
   { NULL }
};

/* a reply that is rows of named columns, as the lists' and
 * GET_PLAYLIST's are */
static const struct cmd_column test_cols_rows[] = {
   { "index", "The entry's number." },
   { "label", "The entry's name." },
   { "path", "The entry's path." },
   { NULL }
};
/* a reply that can be cut short and resumed, as GET_PLAYLIST's is */
static const struct cmd_trailer test_trailers[] = {
   { "MORE", "more", "next", "The entry the next page starts at." },
   { NULL }
};
/* the name in the middle is the column that takes a tab of its own */
static const struct cmd_rows test_rows =
   { test_cols_rows, test_trailers, 1, 0 };

/* a reply whose last column is only sometimes there, as
 * GET_CORE_OPTION's values are */
static const struct cmd_column test_cols_short[] = {
   { "key", "The key." },
   { "value", "The value." },
   { "extra", "Only sometimes given." },
   { NULL }
};
static const struct cmd_rows test_rows_short =
   { test_cols_short, NULL, 2, 2 };

#define SHORT_TEXT "a\tone\nb\ttwo\tand more\nc\n"

/* one column, and a row that opens with the cursor's own word: only the
 * last line, with nothing or digits after MORE, is the cursor */
static const struct cmd_column test_cols_one[] = {
   { "name", "The name." },
   { NULL }
};
static const struct cmd_rows test_rows_one =
   { test_cols_one, test_trailers, 0, 0 };

#define ONE_TEXT "Atari.lpl\nMORE games.lpl\nMORE\n"

/* a reply that opens with one trailer and closes with another, as
 * GET_LOG's SKIPPED and NEXT do, and neither says it was cut short */
static const struct cmd_trailer test_trailers_log[] = {
   { "NEXT", NULL, "next", "The line to ask for next." },
   { "SKIPPED", NULL, "lost", "Lines already gone.", true },
   { NULL }
};
static const struct cmd_rows test_rows_log =
   { test_cols_one, test_trailers_log, 0, 0 };

/* the word with a line after it instead of a number is a row, and so
 * is a line that only ends with one */
#define LOG_TEXT "SKIPPED 3\nNEXT is a line\nfine\nNEXT 9\n"

/* a reply whose lines are prose: the trailers are all a client is
 * given, as GET_LOG's are */
static const struct cmd_rows test_rows_bare =
   { NULL, test_trailers_log, 0, 0 };

static bool act_log(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, LOG_TEXT, sizeof(LOG_TEXT) - 1);
   return true;
}

/* RUN_UNTIL's reply as it ships: how the wait ended opening it, the
 * watched bytes as rows, the byte waited on closing it */
#define UNTIL_TEXT "MET 12\nx\t7e0086\t52\nmode\t7e0770\t8\nVALUE 5\n"
#define WAIT_TEXT  "WAITING 540\nVALUE 3\n"
/* and a step's, which opens with the command's own line */
#define STEP_TEXT  "STEP 4\nx\t7e0086\t52\n"

static bool act_until(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, UNTIL_TEXT, sizeof(UNTIL_TEXT) - 1);
   return true;
}

/* answers through its destination before it returns, as RUN_UNTIL does
 * when the byte already holds what was asked */
#define NOW_TEXT "MET 0\nx\t7e0086\t52\nVALUE 165\n"
static bool act_now(command_t *cmd, const char *arg)
{
   void *dest = cmd->reply_dest ? cmd->reply_dest(cmd) : NULL;
   (void)arg;
   if (!dest)
      return false;
   cmd->reply_to(cmd, dest, NOW_TEXT, sizeof(NOW_TEXT) - 1);
   free(dest);
   return true;
}

/* RUN_FRAMES's and PRESS's own lines, the frames run ending each */
#define RUNF_TEXT  "RUN_FRAMES 3\nx\t7e0086\t52\n"
#define PRESS_TEXT "PRESS 1 right+a 4\n"
static bool act_runf(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, RUNF_TEXT, sizeof(RUNF_TEXT) - 1);
   return true;
}
static bool act_press(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, PRESS_TEXT, sizeof(PRESS_TEXT) - 1);
   return true;
}

/* a PRESS that waited after it, and a STEPS list */
#define PRESSU_TEXT "MET 5\nVALUE 0\n"
#define STEPS_TEXT  "DONE 6\nx\t86\t40\nSTEP 3\n"
#define STOPS_TEXT  "STOPPED 4\nx\t86\t40\nSTOP 2\nSTEP 3\n"
static bool act_pressu(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, PRESSU_TEXT, sizeof(PRESSU_TEXT) - 1);
   return true;
}
static bool act_stops(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, STOPS_TEXT, sizeof(STOPS_TEXT) - 1);
   return true;
}
static bool act_steps(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, STEPS_TEXT, sizeof(STEPS_TEXT) - 1);
   return true;
}

/* replies carrying how a queued STEPS ended, and a read cut short */
#define PREVS_TEXT "DISCARDED 2\nPREVIOUS STOPPED 43 STOP 1 STEP 1\n"
#define PREVF_TEXT "RUN_FRAMES 1\nPREVIOUS WAITING 540 STEP 1\nx\t86\t40\n"
#define PREVE_TEXT "DONE 1\nPREVIOUS ERROR the content was closed\nSTEP 1\n"
#define READT_TEXT "READT 101e 1E 1F\nTRUNCATED 2\n"
static bool act_prevs(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, PREVS_TEXT, sizeof(PREVS_TEXT) - 1);
   return true;
}
static bool act_prevf(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, PREVF_TEXT, sizeof(PREVF_TEXT) - 1);
   return true;
}
static bool act_preve(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, PREVE_TEXT, sizeof(PREVE_TEXT) - 1);
   return true;
}
static bool act_readt(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, READT_TEXT, sizeof(READT_TEXT) - 1);
   return true;
}

/* the menu: its title opens the reply, and a closed one says so */
#define MENUS_TEXT "MENU Main Menu\nPATH Main Menu > Settings\n" \
                   "0\tQuick Menu\t...\n1\tLoad Content\t\n" \
                   "SELECTED 1\nCOUNT 60\nMORE 50\n"
#define MENUC_TEXT "CLOSED\n"
/* what waits on the user, as text anywhere in the reply */
#define MENUD_TEXT "MENU User\nKEYBOARD Username: any\t1 text\n" \
                   "MESSAGE Press again to quit\n0\tUsername\t\n" \
                   "SELECTED 0\nCOUNT 1\n"
static bool act_menud(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, MENUD_TEXT, sizeof(MENUD_TEXT) - 1);
   return true;
}
static bool act_menus(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, MENUS_TEXT, sizeof(MENUS_TEXT) - 1);
   return true;
}
static bool act_menuc(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, MENUC_TEXT, sizeof(MENUC_TEXT) - 1);
   return true;
}

static bool act_wait(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, WAIT_TEXT, sizeof(WAIT_TEXT) - 1);
   return true;
}

static bool act_step(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, STEP_TEXT, sizeof(STEP_TEXT) - 1);
   return true;
}

static bool act_one(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, ONE_TEXT, sizeof(ONE_TEXT) - 1);
   return true;
}

static bool act_short(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, SHORT_TEXT, sizeof(SHORT_TEXT) - 1);
   return true;
}

/* a line the command opens with its own name is not a row, a tab in
 * the name shifts no field, and MORE carries the cursor */
#define ROWS_TEXT "0\tOne\t/one\nROWS none here\n" \
                  "1\tTwo\tand a tab\t/two\nMORE 2\n"

static bool act_rows(command_t *cmd, const char *arg)
{
   (void)arg;
   cmd->replier(cmd, ROWS_TEXT, sizeof(ROWS_TEXT) - 1);
   return true;
}

/* answers with the line it was given, so the test can see how the
 * named arguments were put back together */
static bool act_join(command_t *cmd, const char *arg)
{
   char reply[256];
   size_t len = (size_t)snprintf(reply, sizeof(reply), "JOIN[%s]\n",
         arg ? arg : "");
   cmd->replier(cmd, reply, len);
   return true;
}

static struct cmd_action_map test_actions[] = {
   { "PING",  act_ping,  "No argument", "Answer PONG.",       CMD_INFO_READ_ONLY, NULL },
   { "WIPE",  act_wipe,  "<what>",      "Wipe something.",    CMD_INFO_DESTRUCTIVE, test_params_wipe },
   { "FAIL",  act_fail,  "No argument", "Fail.",              0, NULL },
   { "BAD",   act_bad,   "No argument", "Reply an error.",    0, NULL },
   { "LATER", act_later, "No argument", "Answer on a later frame.", 0, NULL },
   { "GET_STATUS", act_stat, "No argument", "Report the status.", CMD_INFO_READ_ONLY, NULL, &cmd_rows_status },
   { "READ_CORE_MEMORY", act_read, "No argument", "Read bytes.", CMD_INFO_READ_ONLY, NULL, &cmd_rows_read },
   { "MOJI",  act_moji,  "No argument", "Answer text that is not all UTF-8.", 0, NULL },
   { "QUIET", act_quiet_fail, "No argument", "Fail without saying ERROR.", 0, NULL },
   { "JOIN",  act_join,  "<a>|<b> [n]", "Echo the line it was given.", 0, test_params_join },
   { "ROWS",  act_rows,  "No argument", "Answer rows of named columns.", CMD_INFO_READ_ONLY, NULL, &test_rows },
   { "LROWS", act_later, "No argument", "Answer rows on a later frame.", CMD_INFO_READ_ONLY, NULL, &test_rows },
   { "SHORT", act_short, "No argument", "Answer rows that may end short.", CMD_INFO_READ_ONLY, NULL, &test_rows_short },
   { "ONE",   act_one,   "No argument", "Answer one-column rows.", CMD_INFO_READ_ONLY, NULL, &test_rows_one },
   { "LOGR",  act_log,   "No argument", "Answer rows between two trailers.", CMD_INFO_READ_ONLY, NULL, &test_rows_log },
   { "BARE",  act_log,   "No argument", "Answer trailers and no rows.", CMD_INFO_READ_ONLY, NULL, &test_rows_bare },
   { "UNTIL", act_until, "No argument", "Answer as RUN_UNTIL meets.", 0, NULL, &cmd_rows_until },
   { "NOW",   act_now,   "No argument", "Answer before returning.", 0, NULL, &cmd_rows_until },
   { "WAIT",  act_wait,  "No argument", "Answer as RUN_UNTIL waits.", 0, NULL, &cmd_rows_until },
   { "RUN_FRAMES", act_runf,  "No argument", "Answer as RUN_FRAMES does.", 0, NULL, &cmd_rows_frames },
   { "PRESS", act_press, "No argument", "Answer as PRESS does.", 0, NULL, &cmd_rows_press },
   { "PRESSU", act_pressu, "No argument", "Answer as PRESS with until does.", 0, NULL, &cmd_rows_press },
   { "STOPS", act_stops, "No argument", "Answer as STEPS does when stopped.", 0, NULL, &cmd_rows_steps },
   { "STEPS", act_steps, "No argument", "Answer as STEPS does.", 0, NULL, &cmd_rows_steps },
   { "STEP",  act_step,  "No argument", "Answer as a step with watches.", 0, NULL, &cmd_rows_watch },
   { "PREVS", act_prevs, "No argument", "Answer as a queued STEPS after a stop.", 0, NULL, &cmd_rows_steps },
   { "PREVF", act_prevf, "No argument", "Answer as RUN_FRAMES after a queue gave up.", 0, NULL, &cmd_rows_frames },
   { "PREVE", act_preve, "No argument", "Answer as STEPS after a queue failed.", 0, NULL, &cmd_rows_steps },
   { "READT", act_readt, "No argument", "Answer as a read cut short.", CMD_INFO_READ_ONLY, NULL, &cmd_rows_read },
   { "MENUS", act_menus, "No argument", "Answer as MENU does.", 0, NULL, &cmd_rows_menu },
   { "MENUC", act_menuc, "No argument", "Answer as MENU does when closed.", 0, NULL, &cmd_rows_menu },
   { "MENUD", act_menud, "No argument", "Answer as MENU does over a dialog.", 0, NULL, &cmd_rows_menu },
};
static struct cmd_map test_hotkeys[] = {
   { "PAUSE_TOGGLE", 1, "Pause or resume.", 0 },
};

const struct cmd_action_map *command_action_list(size_t *count)
{ *count = sizeof(test_actions) / sizeof(test_actions[0]); return test_actions; }
const struct cmd_map *command_hotkey_list(size_t *count)
{ *count = sizeof(test_hotkeys) / sizeof(test_hotkeys[0]); return test_hotkeys; }

bool command_interfaces_held(void) { return false; }

/* The log ring, as GET_LOG's notifications read it */
static const char *ring_lines[8];
static size_t      ring_head;
size_t verbosity_ring_read(size_t since, char *s, size_t len,
      size_t *first, size_t *next)
{
   size_t _len = 0;
   if (since > ring_head)
      since = ring_head;
   *first = since;
   for (; since < ring_head; since++)
   {
      size_t n = strlen(ring_lines[since]);
      if (_len + n + 1 > len)
         break;
      memcpy(s + _len, ring_lines[since], n);
      _len += n;
   }
   *next = since;
   return _len;
}

static int quits;
bool command_event(enum event_command action, void *data)
{
   (void)data;
   if (action == CMD_EVENT_QUIT)
      quits++;
   return true;
}

/* set where the test plays achievements in hardcore mode */
static const char *test_refused = NULL;
const char *command_tools_refused(void) { return test_refused; }

bool command_run(command_t *handle, const char *name, const char *arg)
{
   size_t i;
   /* as command.c does: the last command's failure is not this one's */
   handle->error = false;
   for (i = 0; i < sizeof(test_actions) / sizeof(test_actions[0]); i++)
      if (!strcmp(test_actions[i].str, name))
         return test_actions[i].action(handle, arg);
   for (i = 0; i < sizeof(test_hotkeys) / sizeof(test_hotkeys[0]); i++)
      if (!strcmp(test_hotkeys[i].str, name))
      {
         char   reply[64];
         size_t len;
         handle->state[test_hotkeys[i].id] = true;
         len = (size_t)snprintf(reply, sizeof(reply), "%s pressed\n", name);
         if (handle->structured)
            handle->replier(handle, reply, len);
         return true;
      }
   return false;
}

/* ---- the client ---- */

#define PORT_OPEN  55471
#define TOKEN      "s3cret-token"
#define AUTH       "Authorization: Bearer " TOKEN "\r\n"

static int client_connect(uint16_t port)
{
   void *addr = NULL;
   int   fd   = socket_init(&addr, port, "127.0.0.1",
         SOCKET_TYPE_STREAM, AF_INET);
   if (fd < 0)
      return -1;
   if (socket_connect(fd, addr) < 0)
   {
      socket_close(fd);
      fd = -1;
   }
   freeaddrinfo_retro((struct addrinfo*)addr);
   return fd;
}

/* Polls @cmd until the peer on @fd closes, collecting what it sent.
 * @deferred_reply, when set, is sent through the waiting request's
 * destination after a few polls, as a task's callback would. */
static int collect(command_t *cmd, int fd, char *out, size_t cap,
      const char *deferred_reply)
{
   size_t   got   = 0;
   int      polls = 0;
   retro_time_t until = cpu_features_get_time_usec() + 3000000;
   socket_nonblock(fd);
   out[0] = '\0';
   while (cpu_features_get_time_usec() < until)
   {
      bool    err = false;
      ssize_t n;
      cmd->poll(cmd);
      if (++polls == 3 && deferred_reply && later_dest)
      {
         /* "ERR:" marks the answer a failure, as a deferred reply
          * carrying one does */
         if (!strncmp(deferred_reply, "ERR:", 4))
         {
            later_cmd->error = true;
            deferred_reply  += 4;
         }
         /* "IMAGE:<path>" answers with a picture and the path, as a
          * screenshot does */
         if (!strncmp(deferred_reply, "IMAGE:", 6))
            later_cmd->reply_image_to(later_cmd, later_dest,
                  deferred_reply + 6, strlen(deferred_reply + 6),
                  "image/png", "iVBORw0KGgo=", 12);
         else
            later_cmd->reply_to(later_cmd, later_dest, deferred_reply,
                  strlen(deferred_reply));
         free(later_dest);
         later_dest = NULL;
      }
      n = socket_receive_all_nonblocking(fd, &err, out + got, cap - 1 - got);
      if (n > 0)
      {
         got     += (size_t)n;
         out[got] = '\0';
      }
      if (err || got >= cap - 1)
         break;
   }
   socket_close(fd);
   return got ? atoi(out + 9) : -1;   /* "HTTP/1.1 NNN" */
}

static int post(command_t *cmd, uint16_t port, const char *extra_headers,
      const char *body, char *out, size_t cap, const char *deferred_reply)
{
   char req[4096];
   int  fd = client_connect(port);
   /* the token, unless the request says what to authorise with */
   const char *auth = (extra_headers && strstr(extra_headers, "Authorization"))
      ? "" : AUTH;
   if (fd < 0)
      return -1;
   if (extra_headers && !strcmp(extra_headers, "NOAUTH"))
   {
      auth          = "";
      extra_headers = NULL;
   }
   snprintf(req, sizeof(req),
         "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\n"
         "Content-Type: application/json\r\n"
         "Accept: application/json, text/event-stream\r\n"
         "MCP-Protocol-Version: 2025-06-18\r\n%s%s"
         "Content-Length: %u\r\n\r\n%s",
         auth, extra_headers ? extra_headers : "",
         (unsigned)strlen(body), body);
   socket_send_all_blocking(fd, req, strlen(req), true);
   return collect(cmd, fd, out, cap, deferred_reply);
}

/* A request from the one revision that sends no MCP-Protocol-Version. */
static int post_old(command_t *cmd, uint16_t port, const char *body,
      char *out, size_t cap)
{
   char req[4096];
   int  fd = client_connect(port);
   if (fd < 0)
      return -1;
   snprintf(req, sizeof(req),
         "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\n"
         "Content-Type: application/json\r\n" AUTH
         "Content-Length: %u\r\n\r\n%s", (unsigned)strlen(body), body);
   socket_send_all_blocking(fd, req, strlen(req), true);
   return collect(cmd, fd, out, cap, NULL);
}

static const char *body_of(const char *resp)
{
   const char *b = strstr(resp, "\r\n\r\n");
   return b ? b + 4 : "";
}

static bool json_valid(const char *s)
{
   rjson_t *j = rjson_open_buffer(s, strlen(s));
   enum rjson_type t;
   bool ok = true;
   if (!j)
      return false;
   while ((t = rjson_next(j)) != RJSON_DONE)
      if (t == RJSON_ERROR)
      {
         ok = false;
         break;
      }
   rjson_free(j);
   return ok;
}

static char *call(const char *name, const char *args)
{
   static char b[512];
   snprintf(b, sizeof(b), "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":"
         "\"tools/call\",\"params\":{\"name\":\"%s\",\"arguments\":%s}}",
         name, args ? args : "{}");
   return b;
}

/* ---- the stdio transport ---- */

static int stdio_in  = -1;   /* the test writes requests here */
static int stdio_out = -1;   /* and reads answers here */

/* The stdio server polled while a request does not fit the pipe yet */
static command_t *stdio_cmd;

static void stdio_send(const char *s)
{
   size_t len  = strlen(s);
   size_t sent = 0;
   int    i;
   for (i = 0; sent < len && i < 1000; i++)
   {
      int n = (int)write(stdio_in, s + sent, (unsigned)(len - sent));
      if (n > 0)
         sent += (size_t)n;
      else if (stdio_cmd)
         stdio_cmd->poll(stdio_cmd);
   }
   if (sent < len)
      check("a request was written", false);
}

/* What the server answers within a few polls, as one string */
static char *stdio_read(command_t *cmd, char *out, size_t cap)
{
   size_t got = 0;
   int    i;
   out[0] = '\0';
   for (i = 0; i < 50; i++)
   {
      int n;
      if (cmd)
         cmd->poll(cmd);
#if defined(_WIN32)
      {
         DWORD avail = 0;
         HANDLE h    = (HANDLE)_get_osfhandle(stdio_out);
         if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL) || !avail)
         {
            Sleep(1);
            continue;
         }
      }
#endif
      n = (int)read(stdio_out, out + got, (unsigned)(cap - 1 - got));
      if (n > 0)
      {
         got     += (size_t)n;
         out[got] = '\0';
      }
#if !defined(_WIN32)
      else
         usleep(1000);
#endif
   }
   return out;
}

static int count_lines(const char *s)
{
   int n = 0;
   for (; *s; s++)
      if (*s == '\n')
         n++;
   return n;
}

static void test_stdio(void)
{
   static char out[65536];
   int in_p[2], out_p[2];
   command_t *cmd;

   printf("stdio transport\n");
   check("nothing until stdout is claimed",
         command_mcp_stdio_new() == NULL);
#if defined(_WIN32)
   if (_pipe(in_p, 65536, _O_BINARY) || _pipe(out_p, 65536, _O_BINARY))
#else
   if (pipe(in_p) || pipe(out_p))
#endif
   {
      check("pipes for the stdio transport", false);
      return;
   }
   /* the server's stdin and stdout are the pipes' other ends */
   fflush(stdout);
   dup2(in_p[0],  0);
   dup2(out_p[1], 1);
   close(in_p[0]);
   close(out_p[1]);
#if defined(_WIN32)
   SetStdHandle(STD_INPUT_HANDLE,  (HANDLE)_get_osfhandle(0));
   SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)_get_osfhandle(1));
#else
   fcntl(out_p[0], F_SETFL, fcntl(out_p[0], F_GETFL) | O_NONBLOCK);
   /* a user over the kernel's pipe budget gets a pipe of a page or two:
    * stdio_send polls the server for room rather than wait on it */
   fcntl(in_p[1], F_SETFL, fcntl(in_p[1], F_GETFL) | O_NONBLOCK);
#endif
   stdio_in  = in_p[1];
   stdio_out = out_p[0];

   check("stdout is claimed", command_mcp_stdio_claim());
   fprintf(stdout, "a stray line for stdout\n");
   fflush(stdout);
   cmd = stdio_cmd = command_mcp_stdio_new();
   check("the stdio server starts", cmd != NULL);
   if (!cmd)
      return;

   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
         "\"params\":{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{},"
         "\"clientInfo\":{\"name\":\"t\",\"version\":\"1\"}}}\n");
   stdio_read(cmd, out, sizeof(out));
   check("initialize is answered on one line", count_lines(out) == 1
         && strstr(out, "\"protocolVersion\":\"2025-06-18\""));
   check("what else wrote to stdout is not in it",
         !strstr(out, "stray"));
   check("with logging among its capabilities",
         strstr(out, "\"logging\":{}") != NULL);
   check("and instructions that say how the tools play a game",
         strstr(out, "\"instructions\":\"Controls a running RetroArch")
         && strstr(out, "STEPS") && strstr(out, "checkpoint"));

   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":60,\"method\":\"tools/list\"}\n");
   stdio_read(cmd, out, sizeof(out));
   check("its tool list has no caching fields before the newest revision",
         strstr(out, "\"tools\":[") && !strstr(out, "ttlMs")
         && !strstr(out, "cacheScope"));
   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":71,\"method\":\"tools/list\","
         "\"params\":{\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"}}}\n");
   stdio_read(cmd, out, sizeof(out));
   check("in the newest revision it says how long to cache it, and for whom",
         strstr(out, "\"tools\":[") && strstr(out, "\"ttlMs\":0")
         && strstr(out, "\"cacheScope\":\"private\""));
   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":73,\"method\":\"server/discover\","
         "\"params\":{\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"}}}\n");
   stdio_read(cmd, out, sizeof(out));
   check("and so does server/discover",
         strstr(out, "\"supportedVersions\":[") && strstr(out, "\"ttlMs\":0")
         && strstr(out, "\"cacheScope\":\"private\""));

   stdio_send("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n");
   check("a notification is not answered",
         !*stdio_read(cmd, out, sizeof(out)));

   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\","
         "\"params\":{\"name\":\"PING\",\"arguments\":{}}}\n"
         "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",");
   stdio_send("\"params\":{\"name\":\"PING\",\"arguments\":{}}}\r\n");
   stdio_read(cmd, out, sizeof(out));
   check("two requests, one in pieces, are answered in order",
         count_lines(out) == 2 && strstr(out, "\"id\":2")
         && strstr(out, "\"id\":3")
         && strstr(out, "\"id\":2") < strstr(out, "\"id\":3"));

   stdio_send("not json\n");
   stdio_read(cmd, out, sizeof(out));
   check("broken JSON is a parse error", strstr(out, "-32700") != NULL);

   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"logging/setLevel\","
         "\"params\":{\"level\":\"warning\"}}\n");
   stdio_read(cmd, out, sizeof(out));
   check("setLevel is answered", strstr(out, "\"id\":4")
         && strstr(out, "\"level\":\"warning\""));
   ring_lines[ring_head++] = "[WARN] [Test] low on something\n";
   ring_lines[ring_head++] = "[INFO] [Test] all is well\n";
   ring_lines[ring_head++] = "[ERROR] [Test] it broke\n";
   stdio_read(cmd, out, sizeof(out));
   check("the log at the level asked and above arrives as notifications",
         count_lines(out) == 2
         && strstr(out, "\"level\":\"warning\"")
         && strstr(out, "\"level\":\"error\"")
         && !strstr(out, "all is well")
         && strstr(out, "notifications/message"));

   {
      /* a message past the limit, and on the same line after it what
       * would be a request of its own: only the refusal is answered */
      static char chunk[32769];
      memset(chunk, 'x', 32768);
      chunk[32768] = '\0';
      stdio_send(chunk);
      stdio_read(cmd, out, sizeof(out));
      stdio_send(chunk);
      stdio_read(cmd, out, sizeof(out));
      check("a message too long is refused once",
            count_lines(out) == 1 && strstr(out, "too long"));
      stdio_send("{\"jsonrpc\":\"2.0\",\"id\":45,\"method\":\"tools/call\","
            "\"params\":{\"name\":\"PING\",\"arguments\":{}}}\n");
      stdio_read(cmd, out, sizeof(out));
      check("and the rest of its line is not run", !*out);
      stdio_send("{\"jsonrpc\":\"2.0\",\"id\":46,\"method\":\"tools/call\","
            "\"params\":{\"name\":\"PING\",\"arguments\":{}}}\n");
      stdio_read(cmd, out, sizeof(out));
      check("the next line is read as ever",
            strstr(out, "\"id\":46") && strstr(out, "PONG"));
   }

   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":44,\"method\":\"tools/call\","
         "\"params\":{\"name\":\"NOW\",\"arguments\":{}}}\n");
   stdio_read(cmd, out, sizeof(out));
   check("answered before it returns, one answer arrives, structured",
         count_lines(out) == 1 && strstr(out, "\"id\":44")
         && strstr(out, "\"met\":0,\"value\":165"));

   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\","
         "\"params\":{\"name\":\"LATER\",\"arguments\":{}}}\n");
   stdio_read(cmd, out, sizeof(out));
   check("a command answering later is waited for", later_dest != NULL
         && !*out);
   cmd->destroy(cmd);
   free(later_dest);
   later_dest = NULL;
   cmd = stdio_cmd = command_mcp_stdio_new();
   stdio_read(cmd, out, sizeof(out));
   check("torn down meanwhile, it is answered with an error",
         strstr(out, "\"id\":5") && strstr(out, "\"isError\":true"));
   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\","
         "\"params\":{\"name\":\"PING\",\"arguments\":{}}}\n");
   stdio_read(cmd, out, sizeof(out));
   check("and the next server goes on", strstr(out, "\"id\":6") != NULL);
   /* the client goes on with the tool list it was given, whose rows
    * tools declare an outputSchema: the revision agreed still holds */
   stdio_send("{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/call\","
         "\"params\":{\"name\":\"UNTIL\",\"arguments\":{}}}\n");
   stdio_read(cmd, out, sizeof(out));
   check("and answers with structuredContent as the first did",
         strstr(out, "\"id\":8") && strstr(out, "\"structuredContent\""));

   close(stdio_in);
   stdio_read(cmd, out, sizeof(out));
   check("stdin closed, RetroArch is asked to quit, once", quits == 1);
   cmd->destroy(cmd);
   stdio_cmd = NULL;
}

int main(void)
{
   static char out[262144];
   command_t *cmd, *none;
   int st;

   network_init();
   printf("start\n");
   none = command_mcp_new(PORT_OPEN, "127.0.0.1", "");
   check("without a token the server does not start", none == NULL);
   if (none)
      none->destroy(none);
   if (!(cmd = command_mcp_new(PORT_OPEN, "127.0.0.1", TOKEN)))
   {
      printf("could not start the server\n");
      return 1;
   }

   printf("handshake\n");
   st = post(cmd, PORT_OPEN, NULL, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":"
         "\"initialize\",\"params\":{\"protocolVersion\":\"2025-06-18\","
         "\"capabilities\":{},\"clientInfo\":{\"name\":\"t\",\"version\":\"1\"}}}",
         out, sizeof(out), NULL);
   check("initialize answers 200 with valid JSON",
         st == 200 && json_valid(body_of(out)));
   check("a version it speaks is the one agreed",
         strstr(out, "\"protocolVersion\":\"2025-06-18\"") != NULL);
   st = post(cmd, PORT_OPEN, NULL, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":"
         "\"initialize\",\"params\":{\"protocolVersion\":\"1999-01-01\","
         "\"capabilities\":{},\"clientInfo\":{\"name\":\"t\",\"version\":\"1\"}}}",
         out, sizeof(out), NULL);
   check("an unknown version gets one it speaks instead",
         st == 200 && strstr(out, "\"protocolVersion\":\"") != NULL
         && !strstr(out, "1999-01-01"));
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"ping\"}",
         out, sizeof(out), NULL);
   check("ping answers", st == 200 && strstr(out, "\"result\"") != NULL);

   printf("tools/list\n");
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/list\"}",
         out, sizeof(out), NULL);
   check("answers 200 with valid JSON", st == 200 && json_valid(body_of(out)));
   check("lists the commands and the hotkeys",
         strstr(out, "\"name\":\"PING\"") && strstr(out, "\"name\":\"WIPE\"")
         && strstr(out, "\"name\":\"PAUSE_TOGGLE\""));
   {
      const char *p = strstr(out, "\"name\":\"PING\"");
      const char *w = strstr(out, "\"name\":\"WIPE\"");
      check("a read-only command is hinted read-only",
            p && strstr(p, "\"readOnlyHint\":true")
            && strstr(p, "\"readOnlyHint\":true") < w);
      check("a destructive command is hinted destructive",
            w && strstr(w, "\"destructiveHint\":true"));
      check("a command taking an argument requires it by name",
            w && strstr(w, "\"required\":[\"what\"]"));
   }
   {
      const char *j = strstr(out, "\"name\":\"JOIN\"");
      check("every parameter is a property of its own",
            j && strstr(j, "\"a\":{\"type\":\"string\"")
            && strstr(j, "\"b\":{\"type\":\"string\"")
            && strstr(j, "\"n\":{\"type\":\"integer\""));
      check("only the needed ones are required",
            j && strstr(j, "\"required\":[\"a\",\"b\"]"));
      check("a parameter with a set of values offers them as an enum",
            j && strstr(j, "\"enum\":[\"fast\",\"slow\"]"));
      check("a command taking nothing has no properties",
            strstr(out, "\"name\":\"PING\"")
            && strstr(strstr(out, "\"name\":\"PING\""),
                  "\"properties\":{}"));
   }
   {
      const char *rw = strstr(out, "\"name\":\"ROWS\"");
      const char *s  = strstr(out, "\"outputSchema\"");
      const char *q  = s;
      int         n  = 0;
      while (q)
      {
         n++;
         q = strstr(q + 1, "\"outputSchema\"");
      }
      const char *rs = rw ? strstr(rw, "\"outputSchema\"") : NULL;
      check("a command answering rows declares an outputSchema",
            rw && rs && strstr(rs, "\"index\":{\"type\":\"string\"")
            && strstr(rs, "\"required\":[\"index\",\"label\",\"path\"]")
            && strstr(rs, "\"next\":{\"type\":\"integer\""));
      /* GET_STATUS, READ_CORE_MEMORY, ROWS, LROWS, SHORT, ONE, LOGR,
       * BARE, UNTIL, NOW, WAIT, RUN_FRAMES, PRESS, PRESSU, STOPS, STEPS,
       * STEP, PREVS, PREVF, PREVE, READT, MENUS, MENUC and MENUD */
      check("only the commands that answer rows declare one", n == 24);
   }
   {
      /* ROWS describes its columns; LROWS, answering the same ones,
       * names them and leaves the describing to ROWS */
      const char *rw = strstr(out, "\"name\":\"ROWS\"");
      const char *lr = strstr(out, "\"name\":\"LROWS\"");
      const char *nx = lr ? strstr(lr + 1, "\"name\":") : NULL;
      check("columns are described by the first tool to answer with them",
            rw && lr && strstr(rw, "\"index\":{\"type\":\"string\",\"description\"")
            < lr);
      check("and named alone by the next",
            lr && strstr(lr, "\"index\":{\"type\":\"string\"}")
            && (!nx || strstr(lr, "\"index\":{\"type\":\"string\"}") < nx));
      check("no schema says what the server would refuse anyway",
            !strstr(out, "additionalProperties"));
      check("and no tool repeats a hint's default",
            !strstr(out, "openWorldHint"));
   }

   printf("tools/call\n");
   st = post(cmd, PORT_OPEN, NULL, call("PING", NULL), out, sizeof(out), NULL);
   check("a reply is the result",
         st == 200 && strstr(out, "PONG") && strstr(out, "\"isError\":false"));
   /* achievements in hardcore mode: no tool runs, and the client is
    * told why as the tool's own failure; the list still answers */
   test_refused = "achievements hardcore mode is on";
   pings = 0;
   st = post(cmd, PORT_OPEN, NULL, call("PING", NULL), out, sizeof(out), NULL);
   check("in hardcore mode a tool call is refused, as the tool's failure",
         st == 200 && json_valid(body_of(out)) && pings == 0
         && strstr(out, "PING ERROR achievements hardcore mode is on")
         && strstr(out, "\"isError\":true") && !strstr(out, "\"error\""));
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/list\"}",
         out, sizeof(out), NULL);
   check("and the tool list still answers", st == 200
         && strstr(out, "\"name\":\"PING\""));
   test_refused = NULL;
   st = post(cmd, PORT_OPEN, NULL, call("FAIL", NULL), out, sizeof(out), NULL);
   check("a command that fails is an error",
         st == 200 && strstr(out, "\"isError\":true"));
   st = post(cmd, PORT_OPEN, NULL, call("BAD", NULL), out, sizeof(out), NULL);
   check("an error reply is an error",
         st == 200 && strstr(out, "BAD ERROR") && strstr(out, "\"isError\":true"));
   st = post(cmd, PORT_OPEN, NULL, call("QUIET", NULL), out, sizeof(out), NULL);
   check("a failure whose text says nothing is still an error",
         st == 200 && strstr(out, "nothing to save")
         && strstr(out, "\"isError\":true"));
   st = post(cmd, PORT_OPEN, NULL, call("PAUSE_TOGGLE", NULL), out, sizeof(out), NULL);
   check("a hotkey is pressed and reports the press",
         st == 200 && cmd->state[1] && strstr(out, "PAUSE_TOGGLE pressed")
         && strstr(out, "\"isError\":false"));
   st = post(cmd, PORT_OPEN, NULL, call("LATER", NULL), out, sizeof(out),
         "/home/me/screenshots/My ERROR Game-251004.png");
   check("a later answer arrives, and a path naming ERROR is no error",
         st == 200 && strstr(out, "My ERROR Game")
         && strstr(out, "\"isError\":false"));
   st = post(cmd, PORT_OPEN, NULL, call("LATER", NULL), out, sizeof(out),
         "ERR:LATER ERROR could not");
   check("a later error answer is an error",
         st == 200 && strstr(out, "\"isError\":true"));

   check("the server takes a reply with an image", cmd->reply_image_to != NULL);
   st = post(cmd, PORT_OPEN, NULL, call("LATER", NULL), out, sizeof(out),
         "IMAGE:/home/me/screenshots/Game-251004.png");
   check("a later answer with an image is image content, then the path",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "{\"type\":\"image\",\"data\":\"iVBORw0KGgo=\"")
         && strstr(out, "\"mimeType\":\"image/png\"")
         && strstr(out, "Game-251004.png")
         && strstr(out, "\"isError\":false"));

   printf("structured content\n");
   st = post(cmd, PORT_OPEN, NULL, call("ROWS", NULL), out, sizeof(out), NULL);
   check("the text a line client reads is unchanged",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"text\":\"0\\tOne\\t/one\\nROWS none here\\n"
               "1\\tTwo\\tand a tab\\t/two\\nMORE 2\\n\""));
   check("the same reply is given as rows of named columns",
         strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"index\":\"0\",\"label\":\"One\",\"path\":\"/one\"},"
               "{\"index\":\"1\",\"label\":\"Two\\tand a tab\","
               "\"path\":\"/two\"}]"));
   check("the MORE cursor is kept", strstr(out, "\"more\":true,\"next\":2")
         != NULL);
   st = post(cmd, PORT_OPEN, NULL, call("LROWS", NULL), out, sizeof(out),
         "3\tThree\t/three\n");
   check("an answer that comes later carries them too",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":[{\"index\":\"3\","
               "\"label\":\"Three\",\"path\":\"/three\"}]}")
         && !strstr(out, "\"more\""));
   st = post(cmd, PORT_OPEN, NULL, call("LROWS", NULL), out, sizeof(out),
         "ERR:LROWS ERROR no such thing");
   check("a failure's text is the reason, and no rows",
         st == 200 && strstr(out, "\"isError\":true")
         && !strstr(out, "structuredContent"));
   /* 2025-03-26 has neither, and is the one revision this server speaks
    * that sends no MCP-Protocol-Version */
   check("a revision before them is sent the text alone",
         post_old(cmd, PORT_OPEN, call("ROWS", NULL), out, sizeof(out)) == 200
         && strstr(out, "MORE 2") && !strstr(out, "structuredContent"));
   check("and its tool list declares no outputSchema",
         post_old(cmd, PORT_OPEN,
            "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/list\"}",
            out, sizeof(out)) == 200 && json_valid(body_of(out))
         && !strstr(out, "\"outputSchema\""));

   printf("parameters\n");
   st = post(cmd, PORT_OPEN, NULL,
         call("WIPE", "{\"what\":\"\"}"), out, sizeof(out), NULL);
   check("an explicit empty required string is present",
         st == 200 && strstr(out, "\"isError\":false"));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"\",\"b\":\"two\"}"),
         out, sizeof(out), NULL);
   check("an empty first operand keeps the next separator",
         st == 200 && strstr(out, "JOIN[|two 0]"));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"one\",\"b\":\"\",\"mode\":\"\"}"),
         out, sizeof(out), NULL);
   check("empty later operands retain their positions, an empty last one goes",
         st == 200 && strstr(out, "JOIN[one| 0]"));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"one\",\"b\":\"two\"}"),
         out, sizeof(out), NULL);
   check("named arguments are joined as the table says",
         st == 200 && strstr(out, "JOIN[one|two 0]"));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"one\",\"b\":\"two\",\"n\":7}"),
         out, sizeof(out), NULL);
   check("an integer arrives as its digits",
         st == 200 && strstr(out, "JOIN[one|two 7]"));
   {
      static const struct {
         const char *number;
         const char *line;
      } cases[] = {
         { "7.0", "JOIN[one|two 7]" },
         { "7e0", "JOIN[one|two 7]" },
         { "7e1", "JOIN[one|two 70]" },
         { "700e-2", "JOIN[one|two 7]" },
         { "0.07e2", "JOIN[one|two 7]" },
         { "-7.0", "JOIN[one|two -7]" },
         { "-0.0001e4", "JOIN[one|two -1]" },
         { "-0.0", "JOIN[one|two 0]" },
         { "0e999999999999999999", "JOIN[one|two 0]" },
         { "0e-999999999999999999", "JOIN[one|two 0]" },
         { "9007199254740993.0", "JOIN[one|two 9007199254740993]" },
         { "1.2300e+2", "JOIN[one|two 123]" }
      };
      static const char *invalid[] = {
         "2.5", "7e-1", "-0.0001e3", "\"7\"",
         "1e999999999999999999", "1e-999999999999999999"
      };
      unsigned i;
      char args[256], label[128];
      for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
      {
         snprintf(args, sizeof(args),
               "{\"a\":\"one\",\"b\":\"two\",\"n\":%s}", cases[i].number);
         st = post(cmd, PORT_OPEN, NULL, call("JOIN", args),
               out, sizeof(out), NULL);
         snprintf(label, sizeof(label), "integer %s is normalized exactly",
               cases[i].number);
         check(label, st == 200 && strstr(out, cases[i].line));
      }
      for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
      {
         snprintf(args, sizeof(args),
               "{\"a\":\"one\",\"b\":\"two\",\"n\":%s}", invalid[i]);
         st = post(cmd, PORT_OPEN, NULL, call("JOIN", args),
               out, sizeof(out), NULL);
         snprintf(label, sizeof(label), "invalid integer %s is rejected",
               invalid[i]);
         check(label, st == 200 && strstr(out, "-32602")
               && strstr(out, "Invalid integer") && !strstr(out, "JOIN["));
      }
   }
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"b\":\"two\",\"a\":\"one\",\"n\":7}"),
         out, sizeof(out), NULL);
   check("the table's order is used, not the client's",
         st == 200 && strstr(out, "JOIN[one|two 7]"));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"one\",\"b\":\"two\",\"mode\":\"fast\"}"),
         out, sizeof(out), NULL);
   check("one left out stands in with its default, if it has one",
         st == 200 && strstr(out, "JOIN[one|two 0 fast]"));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"one\"}"), out, sizeof(out), NULL);
   check("a required one left out is invalid params (-32602)",
         strstr(out, "-32602") && strstr(out, "\\\"b\\\""));
   /* the schemas do not forbid other names, so the server does */
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"one\",\"b\":\"two\",\"mdoe\":\"fast\"}"),
         out, sizeof(out), NULL);
   check("a name the tool does not take is invalid params, not dropped",
         strstr(out, "-32602") && strstr(out, "Unknown parameter")
         && strstr(out, "mdoe") && !strstr(out, "JOIN["));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"one\",\"b\":\"two\",\"mode\":\"quick\"}"),
         out, sizeof(out), NULL);
   check("and so is a value outside the set it offers",
         strstr(out, "-32602") && strstr(out, "fast|slow")
         && !strstr(out, "JOIN["));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"a\":\"one\",\"b\":\"two\",\"mode\":true}"),
         out, sizeof(out), NULL);
   check("a value that is neither a string nor a number is refused",
         strstr(out, "-32602") && strstr(out, "string or a number")
         && !strstr(out, "JOIN["));
   st = post(cmd, PORT_OPEN, NULL, call("PAUSE_TOGGLE", "{\"x\":1}"),
         out, sizeof(out), NULL);
   check("and a parameter given to a hotkey, which takes none",
         strstr(out, "-32602") && strstr(out, "Unknown parameter"));
   st = post(cmd, PORT_OPEN, NULL,
         call("JOIN", "{\"argument\":\"one|two 7\"}"), out, sizeof(out), NULL);
   check("the single argument older clients send still works",
         st == 200 && strstr(out, "JOIN[one|two 7]"));

   st = post(cmd, PORT_OPEN, NULL, call("SHORT", NULL), out, sizeof(out), NULL);
   check("a row ending short of its columns carries what it has",
         st == 200 && strstr(out, "{\"key\":\"a\",\"value\":\"one\"}"));
   check("a full row still carries all of them",
         strstr(out, "{\"key\":\"b\",\"value\":\"two\",\"extra\":\"and more\"}"));
   check("a row short of what every row has is no row",
         st == 200 && !strstr(out, "\"key\":\"c\""));
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/list\"}",
         out, sizeof(out), NULL);
   {
      const char *sh = strstr(out, "\"name\":\"SHORT\"");
      check("the schema requires only the columns every row has",
            sh && strstr(sh, "\"required\":[\"key\",\"value\"]"));
   }

   st = post(cmd, PORT_OPEN, NULL, call("ONE", NULL), out, sizeof(out), NULL);
   check("a row opening with the cursor's word is still a row",
         st == 200 && strstr(out, "{\"name\":\"MORE games.lpl\"}"));
   check("and the cursor on the last line is still the cursor",
         st == 200 && strstr(out, "\"more\":true"));

   st = post(cmd, PORT_OPEN, NULL, call("LOGR", NULL), out, sizeof(out), NULL);
   check("a trailer opening the reply is no row, and carries its number",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"lost\":3") && !strstr(out, "{\"name\":\"SKIPPED"));
   check("a trailer closing it carries its own",
         st == 200 && strstr(out, "\"next\":9"));
   check("the word with anything but a number after it is a row",
         st == 200 && strstr(out, "{\"name\":\"NEXT is a line\"}")
         && strstr(out, "{\"name\":\"fine\"}"));
   check("a reply that cannot be cut short says nothing about more",
         st == 200 && !strstr(out, "\"more\""));
   st = post(cmd, PORT_OPEN, NULL, call("BARE", NULL), out, sizeof(out), NULL);
   check("a reply with no columns is answered by its trailers alone",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"next\":9,\"lost\":3}"));
   check("and its text is the whole of the reply, as ever",
         st == 200 && strstr(out, "NEXT is a line"));
   /* RUN_UNTIL and the steps, with the real tables command.h ships */
   st = post(cmd, PORT_OPEN, NULL, call("UNTIL", NULL), out, sizeof(out), NULL);
   check("RUN_UNTIL's watches are rows, MET and VALUE its trailers",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"label\":\"x\",\"address\":\"7e0086\",\"value\":\"52\"},"
               "{\"label\":\"mode\",\"address\":\"7e0770\",\"value\":\"8\"}],"
               "\"met\":12,\"value\":5}"));
   st = post(cmd, PORT_OPEN, NULL, call("NOW", NULL), out, sizeof(out), NULL);
   check("answered before the command returns, it is answered once, whole",
         st == 200 && json_valid(body_of(out)) && !strstr(out, "Done.")
         && strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"label\":\"x\",\"address\":\"7e0086\",\"value\":\"52\"}],"
               "\"met\":0,\"value\":165}"));
   st = post(cmd, PORT_OPEN, NULL, call("WAIT", NULL), out, sizeof(out), NULL);
   check("WAITING with nothing watched answers how far it got and the byte",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":[],"
               "\"waiting\":540,\"value\":3}"));
   st = post(cmd, PORT_OPEN, NULL, call("STEP", NULL), out, sizeof(out), NULL);
   check("a step's own line is no row, its watches are",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"label\":\"x\",\"address\":\"7e0086\",\"value\":\"52\"}]}"));
   st = post(cmd, PORT_OPEN, NULL, call("RUN_FRAMES", NULL), out, sizeof(out), NULL);
   check("RUN_FRAMES's own line says the frames it ran",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"label\":\"x\",\"address\":\"7e0086\",\"value\":\"52\"}],"
               "\"frames\":3}"));
   st = post(cmd, PORT_OPEN, NULL, call("PRESS", NULL), out, sizeof(out), NULL);
   check("and PRESS's, the number ending it, with nothing watched",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":[],\"frames\":4}"));
   st = post(cmd, PORT_OPEN, NULL, call("PRESSU", NULL), out, sizeof(out), NULL);
   check("a PRESS that waited says how it ended and what it read",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":[],\"met\":5,\"value\":0}"));
   st = post(cmd, PORT_OPEN, NULL, call("STEPS", NULL), out, sizeof(out), NULL);
   check("STEPS says how the list ended and on which step",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"label\":\"x\",\"address\":\"86\",\"value\":\"40\"}],"
               "\"done\":6,\"step\":3}"));
   st = post(cmd, PORT_OPEN, NULL, call("STOPS", NULL), out, sizeof(out), NULL);
   check("a STEPS that stopped says which stop, the line before the last",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"stopped\":4,\"stop\":2,\"step\":3}"));
   st = post(cmd, PORT_OPEN, NULL, call("GET_STATUS", NULL), out, sizeof(out), NULL);
   check("a one-line reply is a record, its last field taking the rest",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"state\":\"PLAYING\","
               "\"system\":\"nes\",\"content\":\"Mario, Luigi & Co.nes\"}"));
   st = post(cmd, PORT_OPEN, NULL, call("READ_CORE_MEMORY", NULL), out, sizeof(out), NULL);
   check("and a read is its address and its bytes",
         st == 200 && strstr(out, "\"structuredContent\":{\"address\":\"1000\","
               "\"bytes\":\"AA 0F 10\"}"));
   st = post(cmd, PORT_OPEN, NULL, call("READT", NULL), out, sizeof(out), NULL);
   check("a read cut short says so, and how many it read",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"address\":\"101e\","
               "\"bytes\":\"1E 1F\",\"truncated\":true,\"read\":2}"));
   st = post(cmd, PORT_OPEN, NULL, call("MENUS", NULL), out, sizeof(out), NULL);
   check("the menu's title, path, and three closing lines",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"index\":\"0\",\"label\":\"Quick Menu\",\"value\":\"...\"},"
               "{\"index\":\"1\",\"label\":\"Load Content\",\"value\":\"\"}],"
               "\"selected\":1,\"count\":60,\"more\":true,\"next\":50,"
               "\"title\":\"Main Menu\",\"path\":\"Main Menu > Settings\"}"));
   st = post(cmd, PORT_OPEN, NULL, call("MENUC", NULL), out, sizeof(out), NULL);
   check("and a closed menu says so",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":[],\"closed\":true}"));
   st = post(cmd, PORT_OPEN, NULL, call("MENUD", NULL), out, sizeof(out), NULL);
   check("a text line is a field on any line, a tab in it and all",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"index\":\"0\",\"label\":\"Username\",\"value\":\"\"}],"
               "\"selected\":0,\"count\":1,\"title\":\"User\","
               "\"keyboard\":\"Username: any\\t1 text\","
               "\"message\":\"Press again to quit\"}"));
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/list\"}",
         out, sizeof(out), NULL);
   {
      const char *m = strstr(out, "\"name\":\"MENUC\"");
      check("a flag that is not a cut says what it means",
            m && strstr(m, "\"closed\":{\"type\":\"boolean\","
                  "\"description\":\"The menu is closed.\"}"));
      check("and a text line is declared a string",
            m && strstr(m, "\"message\":{\"type\":\"string\","));
   }
   st = post(cmd, PORT_OPEN, NULL, call("PREVS", NULL), out, sizeof(out), NULL);
   check("a queued STEPS after a stop: its steps dropped, and the stop",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":[],\"discarded\":2,"
               "\"previous\":{\"stopped\":43,\"stop\":1,\"step\":1}}"));
   st = post(cmd, PORT_OPEN, NULL, call("PREVF", NULL), out, sizeof(out), NULL);
   check("any step after a queue gave up says so as previous",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":["
               "{\"label\":\"x\",\"address\":\"86\",\"value\":\"40\"}],"
               "\"frames\":1,\"previous\":{\"waiting\":540,\"step\":1}}"));
   st = post(cmd, PORT_OPEN, NULL, call("PREVE", NULL), out, sizeof(out), NULL);
   check("and why, where the queue failed",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"structuredContent\":{\"rows\":[],\"done\":1,"
               "\"step\":1,\"previous\":{\"error\":"
               "\"the content was closed\"}}"));
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/list\"}",
         out, sizeof(out), NULL);
   {
      const char *br = strstr(out, "\"name\":\"BARE\"");
      const char *lg = strstr(out, "\"name\":\"LOGR\"");
      /* the tool after BARE answers rows, so BARE's own ends there */
      const char *nx = br ? strstr(br, "\"name\":\"UNTIL\"") : NULL;
      const char *rw = br ? strstr(br, "\"rows\"") : NULL;
      /* LOGR's own entry: the tools after it may well cut short */
      const char *ln = lg ? strstr(lg + 1, "\"name\":\"") : NULL;
      const char *mo = lg ? strstr(lg, "\"more\"") : NULL;
      check("its schema declares the trailers and no rows",
            br && strstr(br, "\"next\":{\"type\":\"integer\"")
            && (!rw || (nx && rw > nx)));
      check("its schema declares both trailers and no more",
            lg && strstr(lg, "\"next\":{\"type\":\"integer\"")
            && strstr(lg, "\"lost\":{\"type\":\"integer\"")
            && (!mo || (ln && mo > ln)));
   }

   printf("commands that are not tools\n");
   /* The flag is what leaves a command out, so the same two the list
    * above carried are flagged and then let be again. They are the
    * first of each table, which is where a skip would take the comma
    * placement with it. */
   test_actions[0].flags |= CMD_INFO_NO_TOOL;            /* PING */
   test_hotkeys[0].flags |= CMD_INFO_NO_TOOL;            /* PAUSE_TOGGLE */
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/list\"}",
         out, sizeof(out), NULL);
   check("a flagged command is left out of the tool list",
         st == 200 && !strstr(out, "\"name\":\"PING\"")
         && !strstr(out, "\"name\":\"PAUSE_TOGGLE\""));
   check("the tools around it are still listed",
         strstr(out, "\"name\":\"WIPE\"") && strstr(out, "\"name\":\"LATER\""));
   check("the list is still valid JSON", json_valid(body_of(out)));
   st = post(cmd, PORT_OPEN, NULL, call("PING", NULL), out, sizeof(out), NULL);
   check("calling one is invalid params (-32602)",
         strstr(out, "-32602") != NULL);

   test_actions[0].flags &= ~CMD_INFO_NO_TOOL;
   test_hotkeys[0].flags &= ~CMD_INFO_NO_TOOL;
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/list\"}",
         out, sizeof(out), NULL);
   check("and are listed again once it is gone",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "\"name\":\"PING\"")
         && strstr(out, "\"name\":\"PAUSE_TOGGLE\""));
   check("and callable again",
         post(cmd, PORT_OPEN, NULL, call("PING", NULL), out, sizeof(out), NULL)
            == 200 && strstr(out, "PONG"));

   printf("JSON-RPC errors\n");
   st = post(cmd, PORT_OPEN, NULL, call("NO_SUCH_TOOL", NULL), out, sizeof(out), NULL);
   check("an unknown tool is invalid params (-32602)",
         strstr(out, "-32602") != NULL);
   st = post(cmd, PORT_OPEN, NULL,
         "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"no/such\"}",
         out, sizeof(out), NULL);
   check("an unknown method is method not found (-32601)",
         strstr(out, "-32601") != NULL);
   st = post(cmd, PORT_OPEN, NULL, "{\"jsonrpc\":\"2.0\",\"id\":", out, sizeof(out), NULL);
   check("broken JSON is a parse error (-32700)",
         strstr(out, "-32700") != NULL);

   printf("the HTTP gate\n");
   {
      static const char get[] = "GET /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
      int fd = client_connect(PORT_OPEN);
      socket_send_all_blocking(fd, get, sizeof(get) - 1, true);
      check("GET is refused (405)", collect(cmd, fd, out, sizeof(out), NULL) == 405);
   }
   {
      static const char req[] = "POST /other HTTP/1.1\r\nHost: x\r\n"
            "Content-Length: 2\r\n\r\n{}";
      int fd = client_connect(PORT_OPEN);
      socket_send_all_blocking(fd, req, sizeof(req) - 1, true);
      check("another path is not found (404)",
            collect(cmd, fd, out, sizeof(out), NULL) == 404);
   }
   check("a page on another site is refused (403)",
         post(cmd, PORT_OPEN, "Origin: http://evil.example\r\n",
            call("PING", NULL), out, sizeof(out), NULL) == 403);
   check("a page on this machine is let in",
         post(cmd, PORT_OPEN, "Origin: http://localhost:8080\r\n",
            call("PING", NULL), out, sizeof(out), NULL) == 200);

   printf("bearer token\n");
   check("no token is refused (401)",
         post(cmd, PORT_OPEN, "NOAUTH", call("PING", NULL), out, sizeof(out), NULL) == 401
         && strstr(out, "WWW-Authenticate: Bearer"));
   check("a wrong token is refused (401)",
         post(cmd, PORT_OPEN, "Authorization: Bearer s3cret-tokem\r\n",
            call("PING", NULL), out, sizeof(out), NULL) == 401);
   check("a token's prefix is refused (401)",
         post(cmd, PORT_OPEN, "Authorization: Bearer s3cret\r\n",
            call("PING", NULL), out, sizeof(out), NULL) == 401);
   check("the token is let in",
         post(cmd, PORT_OPEN, AUTH, call("PING", NULL), out, sizeof(out), NULL) == 200
         && strstr(out, "PONG"));

   printf("framing\n");
   {
      static const char big[] = "POST /mcp HTTP/1.1\r\nHost: x\r\n" AUTH
            "Content-Length: 99999999\r\n\r\n{";
      int fd = client_connect(PORT_OPEN);
      socket_send_all_blocking(fd, big, sizeof(big) - 1, true);
      collect(cmd, fd, out, sizeof(out), NULL);
      check("a body past the limit is dropped, and the server goes on",
            post(cmd, PORT_OPEN, NULL, call("PING", NULL), out, sizeof(out), NULL) == 200);
   }
   {
      /* within the limit by itself, past it with the headers */
      char big[1024];
      int  fd;
      memset(big, 0, sizeof(big));
      snprintf(big, sizeof(big), "POST /mcp HTTP/1.1\r\nHost: x\r\n" AUTH
            "X-Pad: %0400d\r\nContent-Length: 65400\r\n\r\n", 0);
      fd = client_connect(PORT_OPEN);
      socket_send_all_blocking(fd, big, strlen(big), true);
      check("headers and body past the limit together are refused",
            collect(cmd, fd, out, sizeof(out), NULL) == 413);
   }
   {
      char body[512];
      snprintf(body, sizeof(body), "{\"jsonrpc\":\"2.0\",\"id\":\"%070d\","
            "\"method\":\"tools/call\",\"params\":{\"name\":\"PING\","
            "\"arguments\":{}}}", 0);
      st = post(cmd, PORT_OPEN, NULL, body, out, sizeof(out), NULL);
      check("an id too long to answer with is refused, not run",
            st == 200 && strstr(out, "The id is too long") && !strstr(out, "PONG"));
   }
   st = post(cmd, PORT_OPEN, NULL, call("MOJI", NULL), out, sizeof(out), NULL);
   check("text that is not UTF-8 is answered as UTF-8, the rest kept",
         st == 200 && json_valid(body_of(out))
         && strstr(out, "caf\xC3\xA9 \xEF\xBF\xBD\xEF\xBF\xBD\\n")
         && !strstr(out, "\xE6\x97"));
   {
      char req[1024];
      const char *b = call("PING", NULL);
      size_t len, a;
      int fd = client_connect(PORT_OPEN);
      snprintf(req, sizeof(req),
            "POST /mcp HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
            "Accept: application/json, text/event-stream\r\n"
            "MCP-Protocol-Version: 2025-06-18\r\n" AUTH
            "Content-Length: %u\r\n\r\n%s", (unsigned)strlen(b), b);
      len = strlen(req);
      a   = len / 3;
      socket_send_all_blocking(fd, req, a, true);
      cmd->poll(cmd);
      socket_send_all_blocking(fd, req + a, a, true);
      cmd->poll(cmd);
      socket_send_all_blocking(fd, req + 2 * a, len - 2 * a, true);
      check("a request arriving in pieces is answered",
            collect(cmd, fd, out, sizeof(out), NULL) == 200 && strstr(out, "PONG"));
   }

   printf("teardown\n");
   {
      char req[1024];
      const char *b = call("LATER", NULL);
      int fd = client_connect(PORT_OPEN);
      int i;
      snprintf(req, sizeof(req),
            "POST /mcp HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\n"
            "Accept: application/json, text/event-stream\r\n"
            "MCP-Protocol-Version: 2025-06-18\r\n" AUTH
            "Content-Length: %u\r\n\r\n%s", (unsigned)strlen(b), b);
      socket_send_all_blocking(fd, req, strlen(req), true);
      for (i = 0; i < 20 && !later_dest; i++)
         cmd->poll(cmd);
      check("the request is waiting for its answer", later_dest != NULL);
      cmd->destroy(cmd);
      free(later_dest);
      later_dest = NULL;
      {
         size_t got = 0;
         retro_time_t until = cpu_features_get_time_usec() + 2000000;
         socket_nonblock(fd);
         out[0] = '\0';
         while (cpu_features_get_time_usec() < until)
         {
            bool err = false;
            ssize_t n = socket_receive_all_nonblocking(fd, &err,
                  out + got, sizeof(out) - 1 - got);
            if (n > 0) { got += (size_t)n; out[got] = '\0'; }
            if (err) break;
         }
         socket_close(fd);
      }
      check("and is answered with an error when the server goes",
            strstr(out, "\"isError\":true") != NULL);
   }

   test_stdio();

   printf(failures ? "FAIL (%d)\n" : "PASS\n", failures);
   return failures ? 1 : 0;
}
