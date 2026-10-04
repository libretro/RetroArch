/* Regression test for the embedded MCP server (network/mcp_server.c).
 *
 * The server is built as it ships, against a small command table of
 * its own (command_run / command_action_list / command_hotkey_list are
 * stubbed here), and driven over loopback by a client in the same
 * thread: each request is sent, then the server polled until it has
 * answered and closed.  Covered: the protocol handshake, the tool list
 * and its hints, tool calls answered at once and later, the JSON-RPC
 * errors, a later answer carrying an image (SCREENSHOT's), the HTTP gate (method, path, Origin, the bearer token the
 * server will not start without), a body
 * larger than the server takes, a request arriving in pieces, and a
 * waiting request answered when the server is torn down.
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

static bool act_ping(command_t *cmd, const char *arg)
{ (void)arg; cmd->replier(cmd, "PONG\n", 5); return true; }
static bool act_wipe(command_t *cmd, const char *arg)
{ (void)cmd; (void)arg; return true; }
static bool act_fail(command_t *cmd, const char *arg)
{ (void)cmd; (void)arg; return false; }
static bool act_bad(command_t *cmd, const char *arg)
{ (void)arg; cmd->replier(cmd, "BAD ERROR no such thing\n", 24); return false; }
/* answers after it returns, as SCREENSHOT does */
static bool act_later(command_t *cmd, const char *arg)
{
   (void)arg;
   later_cmd  = cmd;
   later_dest = cmd->reply_dest ? cmd->reply_dest(cmd) : NULL;
   return later_dest != NULL;
}

static const struct cmd_action_map test_actions[] = {
   { "PING",  act_ping,  "No argument", "Answer PONG.",       CMD_INFO_READ_ONLY },
   { "WIPE",  act_wipe,  "<what>",      "Wipe something.",    CMD_INFO_DESTRUCTIVE },
   { "FAIL",  act_fail,  "No argument", "Fail.",              0 },
   { "BAD",   act_bad,   "No argument", "Reply an error.",    0 },
   { "LATER", act_later, "No argument", "Answer on a later frame.", 0 },
};
static const struct cmd_map test_hotkeys[] = {
   { "PAUSE_TOGGLE", 1, "Pause or resume.", 0 },
};

const struct cmd_action_map *command_action_list(size_t *count)
{ *count = sizeof(test_actions) / sizeof(test_actions[0]); return test_actions; }
const struct cmd_map *command_hotkey_list(size_t *count)
{ *count = sizeof(test_hotkeys) / sizeof(test_hotkeys[0]); return test_hotkeys; }

bool command_run(command_t *handle, const char *name, const char *arg)
{
   size_t i;
   for (i = 0; i < sizeof(test_actions) / sizeof(test_actions[0]); i++)
      if (!strcmp(test_actions[i].str, name))
         return test_actions[i].action(handle, arg);
   for (i = 0; i < sizeof(test_hotkeys) / sizeof(test_hotkeys[0]); i++)
      if (!strcmp(test_hotkeys[i].str, name))
      {
         handle->state[test_hotkeys[i].id] = true;
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
      check("a command taking an argument requires it",
            w && strstr(w, "\"required\":[\"argument\"]"));
   }

   printf("tools/call\n");
   st = post(cmd, PORT_OPEN, NULL, call("PING", NULL), out, sizeof(out), NULL);
   check("a reply is the result",
         st == 200 && strstr(out, "PONG") && strstr(out, "\"isError\":false"));
   st = post(cmd, PORT_OPEN, NULL, call("FAIL", NULL), out, sizeof(out), NULL);
   check("a command that fails is an error",
         st == 200 && strstr(out, "\"isError\":true"));
   st = post(cmd, PORT_OPEN, NULL, call("BAD", NULL), out, sizeof(out), NULL);
   check("an error reply is an error",
         st == 200 && strstr(out, "BAD ERROR") && strstr(out, "\"isError\":true"));
   st = post(cmd, PORT_OPEN, NULL, call("PAUSE_TOGGLE", NULL), out, sizeof(out), NULL);
   check("a hotkey is pressed and answers Done",
         st == 200 && cmd->state[1] && strstr(out, "Done."));
   st = post(cmd, PORT_OPEN, NULL, call("LATER", NULL), out, sizeof(out),
         "/home/me/screenshots/My ERROR Game-251004.png");
   check("a later answer arrives, and a path naming ERROR is no error",
         st == 200 && strstr(out, "My ERROR Game")
         && strstr(out, "\"isError\":false"));
   st = post(cmd, PORT_OPEN, NULL, call("LATER", NULL), out, sizeof(out),
         "LATER ERROR could not");
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

   printf(failures ? "FAIL (%d)\n" : "PASS\n", failures);
   return failures ? 1 : 0;
}
