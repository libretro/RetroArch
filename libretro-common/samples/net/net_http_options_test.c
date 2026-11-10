/* net_http request options against a server on the loopback interface:
 * a 3xx handed back unfollowed, a body on a method other than POST/PUT
 * carrying its Content-Length, and the caller's Accept-Encoding and
 * User-Agent standing in for net_http's own. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_http.h>
#include <net/net_socket.h>
#include <rthreads/rthreads.h>

static int fails;

#define CHECK(cond, what) \
   do { if (!(cond)) { printf("FAIL: %s\n", what); fails++; } } while (0)

struct server
{
   int  fd;
   int  requests;
   char last[4096]; /* the last request, head and body */
};

static void respond(int fd, const char *status, const char *extra,
      const char *body)
{
   char head[512];
   int  n = snprintf(head, sizeof(head),
         "HTTP/1.1 %s\r\n%sContent-Length: %u\r\nConnection: close\r\n\r\n",
         status, extra, (unsigned)strlen(body));
   send(fd, head, n, 0);
   send(fd, body, (int)strlen(body), 0);
}

static void server_thread(void *data)
{
   struct server *s = (struct server*)data;
   int i;
   for (i = 0; i < s->requests; i++)
   {
      char        req[4096];
      size_t      got = 0;
      const char *body, *cl;
      int         c = (int)accept(s->fd, NULL, NULL);
      if (c < 0)
         return;
      for (;;)
      {
         int n = (int)recv(c, req + got, (int)(sizeof(req) - 1 - got), 0);
         if (n <= 0)
            break;
         got += (size_t)n;
         req[got] = '\0';
         if ((body = strstr(req, "\r\n\r\n")))
         {
            size_t want;
            body += 4;
            cl    = strstr(req, "Content-Length: ");
            want  = cl ? (size_t)atoi(cl + 16) : 0;
            if (got - (size_t)(body - req) >= want)
               break;
         }
      }
      req[got] = '\0';
      memcpy(s->last, req, got + 1);
      if (!strncmp(req, "GET /redirect ", 14))
         respond(c, "302 Found", "Location: /elsewhere\r\n", "moved");
      else if (!strncmp(req, "GET /elsewhere ", 15))
         respond(c, "200 OK", "", "here");
      else
         respond(c, "200 OK", "", "ok");
      socket_close(c);
   }
}

/* Runs one request to the end. Returns the status, the body in body. */
static int fetch(const char *url, const char *method, const char *headers,
      const char *content, bool follow, char *body, size_t body_len,
      char **resp_headers)
{
   struct http_connection_t *conn = net_http_connection_new(url, method, NULL);
   struct http_t            *http;
   uint8_t                  *data;
   size_t                    len = 0;
   int                       status;
   if (!conn)
      return -1;
   while (!net_http_connection_iterate(conn)) { }
   if (!net_http_connection_done(conn))
   {
      net_http_connection_free(conn);
      return -1;
   }
   if (headers)
      net_http_connection_set_headers(conn, headers);
   if (content)
      net_http_connection_set_content(conn, "text/plain", strlen(content),
            content);
   net_http_connection_set_redirects(conn, follow);
   http = net_http_new(conn);
   net_http_connection_free(conn);
   if (!http)
      return -1;
   while (!net_http_update(http, NULL, NULL))
      net_http_wait(http, 100);
   status = net_http_status(http);
   data   = net_http_data(http, &len, true);
   body[0] = '\0';
   if (data && len < body_len)
   {
      memcpy(body, data, len);
      body[len] = '\0';
   }
   free(data); /* the body is the caller's */
   if (resp_headers)
      *resp_headers = net_http_headers_take(http, true);
   net_http_delete(http);
   return status;
}

/* Lines of the request head starting with @name, in any case. */
static int count_lines(const char *req, const char *name)
{
   int         n   = 0;
   size_t      len = strlen(name);
   const char *end = strstr(req, "\r\n\r\n");
   const char *p;
   for (p = req; p && (!end || p < end); )
   {
      if (!strncasecmp(p, name, len))
         n++;
      p = strstr(p, "\r\n");
      if (p)
         p += 2;
   }
   return n;
}

int main(void)
{
   struct server      s;
   struct sockaddr_in addr;
   socklen_t          alen = sizeof(addr);
   sthread_t         *t;
   char               url[128], body[256];
   char              *hdrs = NULL;
   unsigned           port;
   int                status;

   if (!network_init())
   {
      printf("net_http_options_test: SKIP (no network)\n");
      return 0;
   }
   net_http_init();
   memset(&s, 0, sizeof(s));
   s.fd = (int)socket(AF_INET, SOCK_STREAM, 0);
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (s.fd < 0 || bind(s.fd, (struct sockaddr*)&addr, sizeof(addr))
         || getsockname(s.fd, (struct sockaddr*)&addr, &alen)
         || listen(s.fd, 4))
   {
      printf("net_http_options_test: FAIL (no loopback listener)\n");
      return 1;
   }
   port       = ntohs(addr.sin_port);
   s.requests = 6;
   t          = sthread_create(server_thread, &s);

   /* 1: a redirect handed back as it is */
   snprintf(url, sizeof(url), "http://127.0.0.1:%u/redirect", port);
   status = fetch(url, "GET", NULL, NULL, false, body, sizeof(body), &hdrs);
   CHECK(status == 302, "unfollowed redirect: its status");
   CHECK(!strcmp(body, "moved"), "unfollowed redirect: its body");
   CHECK(hdrs && net_http_header_value(hdrs, "Location")
         && !strcmp(net_http_header_value(hdrs, "Location"), "/elsewhere"),
         "unfollowed redirect: its Location");
   free(hdrs);

   /* 2 and 3: followed by default */
   status = fetch(url, "GET", NULL, NULL, true, body, sizeof(body), NULL);
   CHECK(status == 200 && !strcmp(body, "here"), "followed redirect");

   /* 4: a DELETE with a body */
   snprintf(url, sizeof(url), "http://127.0.0.1:%u/item", port);
   status = fetch(url, "DELETE", NULL, "abc", true, body, sizeof(body), NULL);
   CHECK(status == 200, "DELETE with a body: status");
   CHECK(strstr(s.last, "Content-Length: 3\r\n") != NULL,
         "DELETE with a body: its Content-Length");
   CHECK(!strcmp(strstr(s.last, "\r\n\r\n") + 4, "abc"),
         "DELETE with a body: the body");

   /* 5: the caller's Accept-Encoding and User-Agent */
   status = fetch(url, "GET",
         "Accept-Encoding: gzip\r\nuser-agent: Game/1.0\r\n", NULL, true,
         body, sizeof(body), NULL);
   CHECK(status == 200, "own headers: status");
   CHECK(count_lines(s.last, "Accept-Encoding:") == 1
         && strstr(s.last, "Accept-Encoding: gzip\r\n"),
         "own Accept-Encoding alone");
   CHECK(count_lines(s.last, "User-Agent:") == 1
         && strstr(s.last, "user-agent: Game/1.0\r\n"),
         "own User-Agent alone");

   /* 6: net_http's own, when the caller sends none */
   status = fetch(url, "GET", NULL, NULL, true, body, sizeof(body), NULL);
   CHECK(strstr(s.last, "Accept-Encoding: identity\r\n")
         && strstr(s.last, "User-Agent: libretro\r\n"),
         "default Accept-Encoding and User-Agent");

   sthread_join(t);
   socket_close(s.fd);
   net_http_deinit();
   printf("net_http_options_test: %s (%d failures)\n",
         fails ? "FAIL" : "PASS", fails);
   return fails ? 1 : 0;
}
