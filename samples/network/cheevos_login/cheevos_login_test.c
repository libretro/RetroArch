/* cheevos_login_test: regression test for the RetroAchievements login
 * transport path (libretro/RetroArch#19562, #19457).
 *
 * Drives libretro-common's net_http exactly the way cheevos_client.c
 * does for login -- POST /dorequest.php, urlencoded body, user agent
 * set -- against a local mock server, over plain HTTP and over TLS
 * with the real SSL backend linked in, covering the connection-pool
 * conditions a login meets in the wild:
 *
 *   1. login POST over TLS on a fresh connection
 *   2. two logins back-to-back reusing one pooled connection
 *   3. login on a pooled connection the server closed while idle
 *      (advertised keep-alive, closed anyway) -- must be replayed
 *      once on a fresh connection and succeed
 *   4. same as 3 over TLS
 *   5. a pooled connection that dies after response bytes arrived
 *      must NOT be replayed: status -1, prompt termination
 *   6. a TLS read that finds the server's TLS 1.3 session tickets
 *      buffered ahead of the reply must step past them
 *
 * The mock server validates every login request byte-for-byte and
 * answers 400 on any mismatch, so a malformed request line, header
 * block or body fails the test just as a transport error does.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>

#include <net/net_http.h>
#include <lists/string_list.h>
#include <net/net_compat.h>
#include <net/net_socket.h>

/* The /upload body: a byte pattern (i & 0xff) the mock server checks. */
#define UPLOAD_LEN (12u * 1024u * 1024u)
#ifdef HAVE_SSL
#include <net/net_socket_ssl.h>
#endif

#define LOGIN_BODY  "r=login2&u=testuser&p=testpass1234"
#define LOGIN_TOKEN "\"Token\":\"0123456789ABCDEF\""
#define USER_AGENT  "RetroArch/9.99 (cheevos_login_test)"
#define REQ_TIMEOUT_US (20 * 1000000)

#ifdef TEST_SSL_BEAR
/* Defined by bear_seed.c, which #includes the real backend so the
 * test can install its own trust anchor before the first handshake. */
void cheevos_login_test_seed_trust(char *pem);
#endif

static int failures = 0;

static void check(int ok, const char *what)
{
   if (ok)
      printf("[pass] %s\n", what);
   else
   {
      printf("[FAIL] %s\n", what);
      failures++;
   }
}

static int64_t now_us(void)
{
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Run one request to completion the way task_http does: iterate the
 * connection, then poll net_http_update. Returns the HTTP status, or
 * -1 on transport failure, or -2 on harness timeout. Body is copied
 * into *out (caller frees) when present. */
static int do_request(const char *url, const char *method,
      const char *body, char **out, size_t *out_len)
{
   struct http_connection_t *conn;
   struct http_t *http;
   int status;
   int64_t deadline;
   uint8_t *data;
   size_t len = 0;

   if (out)
      *out = NULL;
   if (out_len)
      *out_len = 0;

   if (!(conn = net_http_connection_new(url, method, body)))
      return -2;
   net_http_connection_set_user_agent(conn, USER_AGENT);
   while (!net_http_connection_iterate(conn)) { }
   if (!net_http_connection_done(conn))
   {
      net_http_connection_free(conn);
      return -2;
   }
   if (!(http = net_http_new(conn)))
   {
      net_http_connection_free(conn);
      return -2;
   }

   deadline = now_us() + REQ_TIMEOUT_US;
   while (!net_http_update(http, NULL, NULL))
   {
      if (now_us() > deadline)
      {
         net_http_delete(http);
         net_http_connection_free(conn);
         return -2;
      }
      usleep(1000);
   }

   status = net_http_status(http);
   /* The response header block is owned by the caller once taken,
    * same as task_http frees it. */
   free(net_http_headers_take(http, true));
   data   = net_http_data(http, &len, false);
   if (data && out)
   {
      *out = (char*)malloc(len + 1);
      if (*out)
      {
         memcpy(*out, data, len);
         (*out)[len] = '\0';
         if (out_len)
            *out_len = len;
      }
   }
   /* net_http_data() transfers ownership of the body buffer to the
    * caller (owns_data drops); net_http_delete() will not free it. */
   free(data);
   net_http_delete(http);
   net_http_connection_free(conn);
   return status;
}

static int login_ok(const char *url)
{
   char *body = NULL;
   int status = do_request(url, "POST", LOGIN_BODY, &body, NULL);
   int ok     = (status == 200 && body && strstr(body, LOGIN_TOKEN) != NULL);
   if (!ok)
      printf("       status=%d body=%.120s\n", status, body ? body : "(null)");
   free(body);
   return ok;
}

#ifdef HAVE_SSL
/* Mbed TLS 3.6.0 and 4.x report each TLS 1.3 session ticket from a
 * read with MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET. Later 3.6
 * releases drop tickets by default and BearSSL has no TLS 1.3, so there
 * this passes trivially. errno is cleared before every read: isagain()
 * takes any negative return for would-block when an earlier read left
 * EAGAIN there, which is how case 1 can pass without the code being
 * handled at all. */
static int tls_read_past_tickets(int port)
{
   char port_str[16];
   char request[512];
   char reply[1024];
   struct addrinfo hints;
   struct addrinfo *addr = NULL;
   void *ssl             = NULL;
   int fd                = -1;
   int ok                = 0;

   memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   snprintf(port_str, sizeof(port_str), "%d", port);
   if (getaddrinfo("127.0.0.1", port_str, &hints, &addr) != 0 || !addr)
      return 0;

   if (     (fd = socket(addr->ai_family, addr->ai_socktype,
                  addr->ai_protocol)) >= 0
         && (ssl = ssl_socket_init(fd, "localhost"))
         && ssl_socket_connect(ssl, addr, true, true) >= 0)
   {
      int len = snprintf(request, sizeof(request),
            "POST /dorequest.php HTTP/1.1\r\n"
            "Host: localhost:%d\r\n"
            "User-Agent: " USER_AGENT "\r\n"
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: %d\r\n"
            "\r\n" LOGIN_BODY,
            port, (int)strlen(LOGIN_BODY));

      if (ssl_socket_send_all_blocking(ssl, request, (size_t)len, true))
      {
         int64_t deadline = now_us() + REQ_TIMEOUT_US;
         /* Let the tickets and the reply land before the first read. */
         usleep(200000);
         while (now_us() < deadline)
         {
            bool err = false;
            ssize_t n;
            errno    = 0;
            n        = ssl_socket_receive_all_nonblocking(ssl, &err,
                  reply, sizeof(reply) - 1);
            if (n < 0 || err)
            {
               printf("       TLS read failed before the reply\n");
               break;
            }
            if (n > 0)
            {
               reply[n] = '\0';
               ok       = !strncmp(reply, "HTTP/1.1 200", 12);
               if (!ok)
                  printf("       unexpected reply: %.60s\n", reply);
               break;
            }
            usleep(1000);
         }
      }
   }

   if (ssl)
   {
      ssl_socket_close(ssl);
      ssl_socket_free(ssl);
   }
   else if (fd >= 0)
      close(fd);
   freeaddrinfo(addr);
   return ok;
}

/* The TLS backend's own non-blocking send, below net_http: the 12 MiB
 * /upload body pushed through ssl_socket_send_all_nonblocking() and
 * ssl_socket_flush_nonblocking() while the server stops reading for a
 * second.  Each call must return at once - 0 when the socket is full,
 * never blocking on it - and the server must get every byte in order.
 * Returns the longest call in microseconds, -1 on failure.  mbedtls used
 * to fail the first full socket outright (WANT_WRITE taken for an
 * error); the built-in client blocked for a record at a time. */
static int64_t tls_nonblocking_upload(int port)
{
   static unsigned char pat[65536];
   char port_str[16];
   char request[512];
   char reply[1024];
   struct addrinfo hints;
   struct addrinfo *addr = NULL;
   void *ssl             = NULL;
   int fd                = -1;
   int64_t worst         = 0;
   bool    ok            = false;
   size_t i;

   for (i = 0; i < sizeof(pat); i++)
      pat[i] = (unsigned char)i;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_STREAM;
   snprintf(port_str, sizeof(port_str), "%d", port);
   if (getaddrinfo("127.0.0.1", port_str, &hints, &addr) != 0 || !addr)
      return -1;

   if (     (fd = socket(addr->ai_family, addr->ai_socktype,
                  addr->ai_protocol)) >= 0
         && (ssl = ssl_socket_init(fd, "localhost"))
         && ssl_socket_connect(ssl, addr, true, true) >= 0)
   {
      size_t  sent     = 0;
      int64_t deadline = now_us() + 60 * 1000000LL;
      int     len      = snprintf(request, sizeof(request),
            "PUT /upload HTTP/1.1\r\nHost: localhost:%d\r\n"
            "Content-Length: %u\r\n\r\n", port, UPLOAD_LEN);
      bool    failed   = !ssl_socket_send_all_blocking(ssl, request,
            (size_t)len, true);
      int     flushed  = 0;

      while (!failed && now_us() < deadline && flushed != 1)
      {
         int64_t t0 = now_us(), dt;
         if (sent < UPLOAD_LEN)
         {
            size_t  off  = sent % sizeof(pat);
            size_t  want = sizeof(pat) - off;
            ssize_t n;
            if (want > UPLOAD_LEN - sent)
               want = UPLOAD_LEN - sent;
            /* After a 0 the same pointer and length come back, as the
             * contract asks: sent has not moved. */
            n = ssl_socket_send_all_nonblocking(ssl, pat + off, want, true);
            if (n < 0 || (size_t)n > want)
               failed = true;
            else
               sent += (size_t)n;
         }
         else if ((flushed = ssl_socket_flush_nonblocking(ssl)) < 0)
            failed = true;
         dt = now_us() - t0;
         if (dt > worst)
            worst = dt;
         if (!failed && flushed != 1)
         {
            bool rd = false, wr = true;
            socket_wait(fd, &rd, &wr, 100);
         }
      }
      if (failed || flushed != 1)
         printf("       TLS non-blocking send failed after %lu bytes\n",
               (unsigned long)sent);
      else
      {
         /* The reply: a 200 whose body says every byte arrived. */
         size_t got = 0;
         while (now_us() < deadline && got < sizeof(reply) - 1)
         {
            bool err = false;
            ssize_t n = ssl_socket_receive_all_nonblocking(ssl, &err,
                  reply + got, sizeof(reply) - 1 - got);
            if (n < 0 || err)
               break;
            got        += (size_t)n;
            reply[got]  = '\0';
            if (strstr(reply, "{\"ok\":"))
               break;
            usleep(1000);
         }
         if (!(ok = (strstr(reply, "{\"ok\":true}") != NULL)))
            printf("       TLS upload reply: %.80s\n", reply);
      }
   }
   if (ssl)
   {
      ssl_socket_close(ssl);
      ssl_socket_free(ssl);
   }
   else if (fd >= 0)
      close(fd);
   freeaddrinfo(addr);
   return ok ? worst : -1;
}
#endif

/* GET /stats and parse {"connections":N}. */
static int stats_connections(const char *base)
{
   char url[512];
   char *body = NULL;
   char *p;
   int n = -1;
   snprintf(url, sizeof(url), "%s/stats", base);
   if (do_request(url, "GET", NULL, &body, NULL) == 200 && body
         && (p = strstr(body, "\"connections\":")))
      n = atoi(p + strlen("\"connections\":"));
   free(body);
   return n;
}

/* A 12 MiB PUT to /upload, which stops reading for a second before it
 * takes the body.  The request used to go out in one blocking send, so
 * the net_http_update() that started it did not return until the
 * server resumed: one call of a second or more, at 100% CPU. Sent a
 * slice per call, no call may come anywhere near that.  Returns the
 * longest single net_http_update() in microseconds, -1 on failure. */
static size_t upload_pos;

static int64_t upload_source(void *ud, void *buf, size_t len)
{
   size_t i;
   (void)ud;
   if (len > UPLOAD_LEN - upload_pos)
      len = UPLOAD_LEN - upload_pos;
   for (i = 0; i < len; i++)
      ((unsigned char*)buf)[i] = (unsigned char)(upload_pos + i);
   upload_pos += len;
   return (int64_t)len;
}

static int64_t upload_worst_call(const char *url)
{
   struct http_connection_t *conn;
   struct http_t *http;
   int64_t worst = 0, deadline;
   uint8_t *data;
   size_t len = 0;
   int ok;

   if (!(conn = net_http_connection_new(url, "PUT", NULL)))
      return -1;
   net_http_connection_set_user_agent(conn, USER_AGENT);
   while (!net_http_connection_iterate(conn)) { }
   if (!net_http_connection_done(conn))
   {
      net_http_connection_free(conn);
      return -1;
   }
   upload_pos = 0;
   net_http_connection_set_content_source(conn, "application/octet-stream",
         UPLOAD_LEN, upload_source, NULL, NULL);
   http = net_http_new(conn);
   net_http_connection_free(conn);
   if (!http)
      return -1;

   deadline = now_us() + 60 * 1000000LL;
   for (;;)
   {
      int64_t t0 = now_us(), dt;
      bool done  = net_http_update(http, NULL, NULL);
      dt         = now_us() - t0;
      if (dt > worst)
         worst = dt;
      if (done || now_us() > deadline)
         break;
      net_http_wait(http, 50);
   }

   data = net_http_data(http, &len, false);
   ok   = (net_http_status(http) == 200 && data
         && len == 11 && !memcmp(data, "{\"ok\":true}", 11));
   if (!ok)
      printf("       upload: status=%d len=%lu\n", net_http_status(http),
            (unsigned long)len);
   free(data);
   free(net_http_headers_take(http, true));
   net_http_delete(http);
   return ok ? worst : -1;
}

int main(int argc, char **argv)
{
   char plain_base[256];
   char tls_base[256];
   char url[512];
   char *body;
   int status;
   int conns_before;
   int conns_after;
   FILE *srv;
   char line[256];
   char cmd[1024];
   int plain_port = 0;
   int tls_port   = 0;

   if (argc < 4)
   {
      fprintf(stderr,
            "usage: %s mock_ra_server.py server.crt server.key\n", argv[0]);
      return 2;
   }

   signal(SIGPIPE, SIG_IGN);
   network_init();

   /* The server exits when its stdin (our pipe) closes, so it cannot
    * outlive the test whatever way the test ends. */
   snprintf(cmd, sizeof(cmd), "exec python3 %s %s %s %s",
         argv[1], argv[2], argv[3], "mock_ra_server.ready");
   if (!(srv = popen(cmd, "w")))
   {
      fprintf(stderr, "cannot start mock server\n");
      return 2;
   }
   /* The server writes its READY line (with the ports it bound) to
    * mock_ra_server.ready once both listeners are up; poll for it. */
   {
      int64_t deadline = now_us() + 10 * 1000000;
      FILE *rf         = NULL;
      line[0] = '\0';
      while (now_us() < deadline)
      {
         if ((rf = fopen("mock_ra_server.ready", "r")))
         {
            if (fgets(line, sizeof(line), rf)
                  && sscanf(line, "READY plain=%d tls=%d",
                        &plain_port, &tls_port) == 2)
            {
               fclose(rf);
               break;
            }
            fclose(rf);
            rf = NULL;
         }
         usleep(20000);
      }
      if (!plain_port || !tls_port)
      {
         fprintf(stderr, "mock server did not come up: '%s'\n", line);
         pclose(srv);
         return 2;
      }
   }

   snprintf(plain_base, sizeof(plain_base), "http://localhost:%d", plain_port);
   snprintf(tls_base,   sizeof(tls_base),   "https://localhost:%d", tls_port);

#ifdef TEST_SSL_RETRO
   {
      /* Trust our test CA instead of the built-in bundle; the client
       * keeps the pointer, so the buffer lives for the whole run. */
      extern char *test_read_file(const char *path);
      static char *retro_pem;
      if (!(retro_pem = test_read_file(getenv("TEST_CA_PEM"))))
      {
         fprintf(stderr, "cannot read TEST_CA_PEM\n");
         pclose(srv);
         return 2;
      }
      ssl_socket_retro_set_trust_pem(retro_pem, strlen(retro_pem));
   }
#endif
#ifdef TEST_SSL_BEAR
   {
      /* Trust our test CA instead of the system bundle. */
      extern char *test_read_file(const char *path);
      char *pem = test_read_file(getenv("TEST_CA_PEM"));
      if (!pem)
      {
         fprintf(stderr, "cannot read TEST_CA_PEM\n");
         pclose(srv);
         return 2;
      }
      /* Feed the loader what a real /etc/ssl bundle looks like:
       * comment lines, blank lines, another CA first (in CRLF line
       * endings), ours second. The handshake in scenario 1 only
       * succeeds if our CA survived this parse -- guarding the
       * "wrapped PEM rejected, trust store empty" failure mode. */
      extern char *test_build_bundle(const char *our_ca_pem);
      char *bundle = test_build_bundle(pem);
      free(pem);
      if (!bundle)
      {
         fprintf(stderr, "cannot build CA bundle fixture\n");
         pclose(srv);
         return 2;
      }
      cheevos_login_test_seed_trust(bundle);
      free(bundle);
   }
#endif

   /* 1. Login over TLS on a fresh connection: the first thing cheevos
    *    does after content load. */
#ifdef HAVE_SSL
   snprintf(url, sizeof(url), "%s/dorequest.php", tls_base);
   check(login_ok(url), "TLS login POST on a fresh connection");
#endif

   /* 2. Keep-alive reuse: two logins, one TCP connection. */
   snprintf(url, sizeof(url), "%s/dorequest.php", plain_base);
   conns_before = stats_connections(plain_base);
   check(login_ok(url), "plain login POST #1 (connection pooled after)");
   check(login_ok(url), "plain login POST #2 on the pooled connection");
   conns_after  = stats_connections(plain_base);
   check(conns_before >= 0 && conns_after == conns_before,
         "both logins and stats rode one pooled connection");

   /* 3. Server silently closed the pooled connection while it sat
    *    idle: the next login must be replayed once and succeed.
    *    /lying answers 200 keep-alive, then closes. */
   snprintf(url, sizeof(url), "%s/lying/dorequest.php", plain_base);
   check(login_ok(url), "login on /lying (leaves a dead pooled connection)");
   snprintf(url, sizeof(url), "%s/dorequest.php", plain_base);
   check(login_ok(url), "login after silent server close is replayed and succeeds");

   /* 4. The same idle-close recovery over TLS. */
#ifdef HAVE_SSL
   snprintf(url, sizeof(url), "%s/lying/dorequest.php", tls_base);
   check(login_ok(url), "TLS login on /lying (dead pooled TLS connection)");
   snprintf(url, sizeof(url), "%s/dorequest.php", tls_base);
   check(login_ok(url), "TLS login after silent server close is replayed and succeeds");
#endif

   /* 5. A connection that dies after response bytes arrived must not
    *    be replayed: half a status line, then close -> status -1,
    *    and it must terminate, not spin. */
   snprintf(url, sizeof(url), "%s/dorequest.php", plain_base);
   (void)login_ok(url); /* park a live connection in the pool */
   snprintf(url, sizeof(url), "%s/partial", plain_base);
   status = do_request(url, "GET", NULL, &body, NULL);
   check(status == -1, "partial response then close fails with status -1, no replay");
   if (status == -2)
      printf("       (request did not terminate within the timeout)\n");
   free(body);
   /* And the pool must have shed the dead connection: another login
    * still works. */
   snprintf(url, sizeof(url), "%s/dorequest.php", plain_base);
   check(login_ok(url), "login still works after the failed transfer");

   /* 6. A large upload is sent a slice per update, plain and over TLS:
    *    the server stops reading for a second, and no single update
    *    may sit through that. */
   {
      int64_t worst;
      snprintf(url, sizeof(url), "%s/upload", plain_base);
      worst = upload_worst_call(url);
      printf("       plain 12 MiB upload: longest update %ld ms\n",
            (long)(worst / 1000));
      check(worst >= 0 && worst < 250000,
            "plain upload behind a stalled reader: no update blocks");
#ifdef HAVE_SSL
      snprintf(url, sizeof(url), "%s/upload", tls_base);
      worst = upload_worst_call(url);
      printf("       TLS 12 MiB upload: longest update %ld ms\n",
            (long)(worst / 1000));
      check(worst >= 0 && worst < 250000,
            "TLS upload behind a stalled reader: no update blocks");
      worst = tls_nonblocking_upload(tls_port);
      printf("       TLS backend non-blocking send: longest call %ld ms\n",
            (long)(worst / 1000));
      check(worst >= 0 && worst < 250000,
            "TLS backend non-blocking send: never blocks, delivers every byte");
#endif
   }

   /* 7. TLS 1.3 session tickets buffered ahead of the reply. */
#ifdef HAVE_SSL
   check(tls_read_past_tickets(tls_port),
         "TLS read steps past buffered session tickets");
#endif

   pclose(srv);

   if (failures)
   {
      printf("%d FAILURE(S)\n", failures);
      return 1;
   }
   printf("all cheevos login transport checks passed\n");
   return 0;
}

char *test_read_file(const char *path)
{
   FILE *f;
   long sz;
   char *buf = NULL;
   if (!path || !(f = fopen(path, "rb")))
      return NULL;
   fseek(f, 0, SEEK_END);
   sz = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (sz > 0 && (buf = (char*)malloc((size_t)sz + 1)))
   {
      if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
      {
         free(buf);
         buf = NULL;
      }
      else
         buf[sz] = '\0';
   }
   fclose(f);
   return buf;
}

#ifdef TEST_SSL_BEAR
/* A cert whose base64 body is intentionally invalid: the loader must
 * skip it and go on to install the valid CA after it, not abort the
 * bundle. (A parse failure that aborts leaves the trust store empty
 * and every TLS login failing.) */
static const char other_ca_pem[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIBhTCCASugAwIBAgIUX9YyM1H1S5nJc0R8fJqoS3T0m4YwCgYIKoZIzj0EAwIw\n"
"GjEYMBYGA1UEAwwPb3RoZXItdGVzdC1jYS0xMB4XDTI0MDEwMTAwMDAwMFoXDTM0\n"
"MDEwMTAwMDAwMFowGjEYMBYGA1UEAwwPb3RoZXItdGVzdC1jYS0xMFkwEwYHKoZI\n"
"zj0CAQYIKoZIzj0DAQcDQgAEexampleexampleexampleexampleexampleexampl\n"
"eexampleexampleexampleexampleQaMTMBEwDwYDVR0TAQH/BAUwAwEB/zAKBggq\n"
"hkjOPQQDAgNIADBFAiEA1234567890abcdef1234567890abcdef1234567890abAi\n"
"BQfedcba0987654321fedcba0987654321fedcba098765432100\n"
"-----END CERTIFICATE-----\n";

char *test_build_bundle(const char *our_ca_pem)
{
   size_t i;
   size_t need = strlen(our_ca_pem) + sizeof(other_ca_pem) + 1024;
   char *crlf  = (char*)malloc(sizeof(other_ca_pem) * 2);
   char *out   = (char*)malloc(need + sizeof(other_ca_pem));
   char *p;
   if (!crlf || !out)
   {
      free(crlf);
      free(out);
      return NULL;
   }
   /* CRLF-ify the unrelated CA. */
   p = crlf;
   for (i = 0; other_ca_pem[i]; i++)
   {
      if (other_ca_pem[i] == '\n')
         *p++ = '\r';
      *p++ = other_ca_pem[i];
   }
   *p = '\0';
   snprintf(out, need + sizeof(other_ca_pem),
         "##\n"
         "## Bundle of CA Root Certificates (test fixture)\n"
         "##\n"
         "\n"
         "Garbage Test CA A\n"
         "=================\n"
         "%s"
         "\n"
         "Cheevos Login Test CA\n"
         "=====================\n"
         "%s",
         crlf, our_ca_pem);
   free(crlf);
   return out;
}
#endif
