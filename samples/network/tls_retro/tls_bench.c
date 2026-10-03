/* Handshake cost of the built-in TLS client, and bulk receive
 * throughput:
 *   tls_bench host port ca.pem [rounds]        N connections; the first is
 *                                             a full handshake, the rest
 *                                             resume
 *   tls_bench host port ca.pem get path        GET /path (openssl s_server
 *                                             -WWW serves a file) and the
 *                                             receive rate over it */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <streams/file_stream.h>
#include <features/features_cpu.h>

void ssl_socket_log_verify_fail(int mode_required, const char *domain, const char *info)
{
   fprintf(stderr, "verify %s for %s: %s\n", mode_required ? "FAILED" : "failed", domain, info);
}
void ssl_socket_log_verify_disabled(const char *domain) { (void)domain; }

static int one(const char *host, int port, int *resumed)
{
   struct addrinfo *addr = NULL;
   int fd = socket_init((void**)&addr, port, host, SOCKET_TYPE_STREAM, AF_INET);
   void *ssl;
   int ok = 0;
   if (fd < 0 || !addr)
      return 0;
   if (!(ssl = ssl_socket_init(fd, host)))
      goto done;
   if (ssl_socket_connect(ssl, addr, true, true) >= 0)
   {
      /* one request and its reply: a 1.3 server's session ticket
       * arrives with the first application data */
      static const char req[] = "GET / HTTP/1.0\r\n\r\n";
      uint8_t buf[256];
      ok = 1;
      *resumed = ssl_socket_retro_was_resumed(ssl);
      ssl_socket_send_all_blocking(ssl, req, sizeof(req) - 1, true);
      ssl_socket_receive_all_blocking(ssl, buf, 16);
   }
   ssl_socket_close(ssl);
   ssl_socket_free(ssl);
done:
   freeaddrinfo_retro(addr);
   return ok;
}

static int bulk_get(const char *host, int port, const char *path)
{
   struct addrinfo *addr = NULL;
   int fd = socket_init((void**)&addr, port, host, SOCKET_TYPE_STREAM, AF_INET);
   void *ssl;
   static uint8_t buf[1 << 16];
   int64_t total = 0;
   retro_time_t t0;
   char req[512];

   /* ssl_socket_connect() makes the TCP connection itself */
   if (fd < 0 || !addr || !(ssl = ssl_socket_init(fd, host)))
      return 1;
   if (ssl_socket_connect(ssl, addr, true, true) < 0)
   {
      fprintf(stderr, "tls connect failed\n");
      return 1;
   }
   snprintf(req, sizeof(req), "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n", path, host);
   ssl_socket_send_all_blocking(ssl, req, strlen(req), true);
   t0 = cpu_features_get_time_usec();
   while (ssl_socket_receive_all_blocking(ssl, buf, sizeof(buf)) == 1)
      total += (int64_t)sizeof(buf);
   printf("receive: %lld MB in %.2f s = %.0f MB/s\n", (long long)(total >> 20),
         (cpu_features_get_time_usec() - t0) / 1e6,
         (total / 1048576.0) / ((cpu_features_get_time_usec() - t0) / 1e6));
   ssl_socket_close(ssl);
   ssl_socket_free(ssl);
   freeaddrinfo_retro(addr);
   return 0;
}

int main(int argc, char **argv)
{
   const char *host = argv[1];
   int port = atoi(argv[2]);
   int rounds = argc > 4 ? atoi(argv[4]) : 20;
   int i, res = 0, nfull = 0, nres = 0;
   int64_t tfull = 0, tres = 0;
   char *pem = NULL; int64_t pem_len = 0;

   if (argc < 4)
      return 2;
   network_init();
   if (argc > 5 && strcmp(argv[4], "get") == 0)
   {
      ssl_socket_set_verify_mode(2);
      return bulk_get(host, port, argv[5]);
   }
   if (filestream_read_file(argv[3], (void**)&pem, &pem_len))
      ssl_socket_retro_set_trust_pem(pem, (size_t)pem_len);
   ssl_socket_set_verify_mode(0);
   for (i = 0; i < rounds; i++)
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      if (!one(host, port, &res))
      {
         fprintf(stderr, "round %d failed\n", i);
         return 1;
      }
      if (res) { tres += cpu_features_get_time_usec() - t0; nres++; }
      else     { tfull += cpu_features_get_time_usec() - t0; nfull++; }
   }
   printf("full handshakes: %d, %.2f ms each; resumed: %d, %.2f ms each\n",
         nfull, nfull ? tfull / 1000.0 / nfull : 0.0, nres, nres ? tres / 1000.0 / nres : 0.0);
   return 0;
}
