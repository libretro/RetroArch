/* Several threads make their first TLS connection at once, verify
 * required: the trust store is built by whichever gets there first and
 * read by the others while it is being built. Built under
 * ThreadSanitizer by the matrix.  tls_threads host port ca.pem */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <streams/file_stream.h>
#include <rthreads/rthreads.h>
void ssl_socket_log_verify_fail(int m, const char *d, const char *i) { fprintf(stderr, "verify: %s %s\n", d, i); (void)m; }
void ssl_socket_log_verify_disabled(const char *d) { (void)d; }
#define NTHREADS 6
static const char *g_host; static int g_port; static int failures = 0;
static void worker(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < 3; i++)
   {
      struct addrinfo *addr = NULL;
      int fd = socket_init((void**)&addr, g_port, g_host, SOCKET_TYPE_STREAM, AF_INET);
      void *ssl;
      uint8_t buf[16];
      if (fd < 0 || !addr || !(ssl = ssl_socket_init(fd, g_host))) { __sync_fetch_and_add(&failures, 1); continue; }
      if (ssl_socket_connect(ssl, addr, true, false) < 0)
         __sync_fetch_and_add(&failures, 1);
      else
      {
         ssl_socket_send_all_blocking(ssl, "GET / HTTP/1.0\r\n\r\n", 18, true);
         ssl_socket_receive_all_blocking(ssl, buf, 8);
      }
      ssl_socket_close(ssl); ssl_socket_free(ssl); freeaddrinfo_retro(addr);
   }
}
int main(int argc, char **argv)
{
   sthread_t *t[NTHREADS]; int i; char *pem = NULL; int64_t pem_len = 0;
   if (argc < 4) return 2;
   network_init(); g_host = argv[1]; g_port = atoi(argv[2]);
   if (filestream_read_file(argv[3], (void**)&pem, &pem_len))
      ssl_socket_retro_set_trust_pem(pem, (size_t)pem_len);
   ssl_socket_set_verify_mode(0);
   for (i = 0; i < NTHREADS; i++) t[i] = sthread_create(worker, NULL);
   for (i = 0; i < NTHREADS; i++) if (t[i]) sthread_join(t[i]);
   free(pem);
   if (failures) { fprintf(stderr, "FAIL: %d connections failed\n", failures); return 1; }
   printf("ok: %d threads x 3 connections, first use concurrent\n", NTHREADS);
   return 0;
}
