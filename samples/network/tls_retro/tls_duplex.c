/* SSL_RETRO_DUPLEX against openssl s_server's interactive console: one
 * thread reads the lines the server sends while another sends lines of
 * its own, on one connection, with no lock. The server's stdin script
 * puts KeyUpdates between its lines ('K' with update_requested, 'k'
 * without); a requested one is answered by the sending thread, so what
 * the server prints of ours after it has come under our new keys.
 * Built under ThreadSanitizer by the matrix.
 *   tls_duplex host port ca.pem nlines
 * Reads lines until "end"; sends w000..w(nlines-1), one every 50 ms,
 * then "done". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <features/features_cpu.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <streams/file_stream.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_timers.h>
void ssl_socket_log_verify_fail(int m, const char *d, const char *i) { fprintf(stderr, "verify: %s %s\n", d, i); (void)m; }
void ssl_socket_log_verify_disabled(const char *d) { (void)d; }

static void *g_ssl;
static int   g_fd;
static int   g_lines;
static retro_atomic_int_t g_send_failed;
static retro_atomic_int_t g_stop;

/* All of @len out, waiting for the socket when it is full */
static int send_all(const char *data, size_t len)
{
   retro_time_t deadline = cpu_features_get_time_usec() + 5000000;
   size_t       off      = 0;
   for (;;)
   {
      bool read = false, write = true;
      if (off < len)
      {
         ssize_t n = ssl_socket_send_all_nonblocking(g_ssl, data + off, len - off, true);
         if (n < 0)
            return -1;
         off += (size_t)n;
      }
      if (off == len)
      {
         int r = ssl_socket_flush_nonblocking(g_ssl);
         if (r < 0)
            return -1;
         if (r)
            return 0;
      }
      if (cpu_features_get_time_usec() >= deadline)
         return -1;
      socket_wait(g_fd, &read, &write, 100);
   }
}

static void writer(void *arg)
{
   char line[16];
   int  i;
   (void)arg;
   for (i = 0; i < g_lines && !retro_atomic_load_acquire_int(&g_stop); i++)
   {
      snprintf(line, sizeof(line), "w%03d\n", i);
      if (send_all(line, strlen(line)) != 0)
      {
         retro_atomic_store_release_int(&g_send_failed, 1);
         return;
      }
      retro_sleep(50);
   }
   if (send_all("done\n", 5) != 0)
      retro_atomic_store_release_int(&g_send_failed, 1);
}

static int read_line(char *out, size_t cap)
{
   size_t n = 0;
   retro_time_t deadline = cpu_features_get_time_usec() + 10000000;
   while (n + 1 < cap)
   {
      bool error = false;
      ssize_t got = ssl_socket_receive_all_nonblocking(g_ssl, &error, out + n, 1);
      if (got < 0 || error || cpu_features_get_time_usec() >= deadline)
         return -1;
      if (!got)
      {
         bool read = true, write = false;
         if (!ssl_socket_retro_pending(g_ssl, &write))
            socket_wait(g_fd, &read, &write, 100);
         continue;
      }
      if (out[n] == '\n')
         break;
      n++;
   }
   out[n] = '\0';
   return 0;
}

int main(int argc, char **argv)
{
   struct ssl_socket_retro_config cfg;
   struct addrinfo *addr = NULL;
   sthread_t *t;
   char line[128], *pem = NULL;
   int64_t pem_len = 0;
   int lines = 0, rc = 0;
   if (argc < 5)
      return 2;
   g_lines = atoi(argv[4]);
   retro_atomic_int_init(&g_send_failed, 0);
   retro_atomic_int_init(&g_stop, 0);
   network_init();
   if (filestream_read_file(argv[3], (void**)&pem, &pem_len))
      ssl_socket_retro_set_trust_pem(pem, (size_t)pem_len);
   ssl_socket_set_verify_mode(0);
   g_fd = socket_init((void**)&addr, atoi(argv[2]), argv[1], SOCKET_TYPE_STREAM, AF_INET);
   if (g_fd < 0 || !addr || !(g_ssl = ssl_socket_init(g_fd, argv[1])))
      return 1;
   memset(&cfg, 0, sizeof(cfg));
   cfg.flags = SSL_RETRO_DUPLEX;
   if (ssl_socket_retro_configure(g_ssl, &cfg) != 0)
   {
      fprintf(stderr, "configure failed\n");
      return 1;
   }
   if (ssl_socket_connect(g_ssl, addr, true, true) < 0)
   {
      fprintf(stderr, "connect failed (%d)\n", ssl_socket_last_error(g_ssl));
      return 1;
   }
   if (!(t = sthread_create(writer, NULL)))
      return 1;
   for (;;)
   {
      if (read_line(line, sizeof(line)) != 0)
      {
         fprintf(stderr, "read failed after %d lines (%d)\n", lines,
               ssl_socket_last_error(g_ssl));
         rc = 1;
         break;
      }
      if (!strcmp(line, "end"))
         break;
      printf("got: %s\n", line);
      lines++;
   }
   if (rc)
      retro_atomic_store_release_int(&g_stop, 1);
   sthread_join(t);
   if (retro_atomic_load_acquire_int(&g_send_failed))
   {
      fprintf(stderr, "send failed (%d)\n", ssl_socket_last_error(g_ssl));
      rc = 1;
   }
   if (!rc)
      printf("ok: %d lines read while %d were sent, %s\n", lines, g_lines,
            ssl_socket_retro_version(g_ssl) == 0x0304 ? "TLS 1.3" : "TLS 1.2");
   ssl_socket_close(g_ssl);
   ssl_socket_free(g_ssl);
   freeaddrinfo_retro(addr);
   free(pem);
   return rc;
}
