/* Handshake cost of the built-in TLS client: N full handshakes (the
 * session cache cleared by using a fresh hostname alias each time is
 * not possible, so full ones run with verify on and resumption is
 * measured on the same host afterwards).
 *   tls_bench host port ca.pem rounds */
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
   if (!socket_connect_with_timeout(fd, addr, 3000))
      goto done;
   if (!(ssl = ssl_socket_init(fd, host)))
      goto done;
   if (ssl_socket_connect(ssl, addr, true, true) >= 0)
   {
      ok = 1;
      *resumed = ssl_socket_retro_was_resumed(ssl);
   }
   ssl_socket_close(ssl);
   ssl_socket_free(ssl);
done:
   freeaddrinfo_retro(addr);
   return ok;
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
