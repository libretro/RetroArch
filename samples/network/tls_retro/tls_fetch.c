/* End-to-end check of net_socket_ssl_retro.c: TCP connect, TLS
 * handshake, one HTTP GET, first line of the reply. Exit status is
 * the outcome, so it doubles as a test:
 *   tls_fetch host [port] [verify-mode] [ca.pem] [rounds]
 * With rounds > 1 the connection is made that many times in one
 * process and every round after the first must resume the session. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>

void ssl_socket_log_verify_fail(int mode_required, const char *domain, const char *info)
{
   fprintf(stderr, "verify %s for %s: %s\n", mode_required ? "FAILED (required)" : "failed (optional)", domain, info);
}
void ssl_socket_log_verify_disabled(const char *domain)
{
   fprintf(stderr, "verify disabled for %s\n", domain);
}

int main(int argc, char **argv)
{
   const char *host = argc > 1 ? argv[1] : "buildbot.libretro.com";
   uint16_t    port = argc > 2 ? (uint16_t)atoi(argv[2]) : 443;
   unsigned    mode = argc > 3 ? (unsigned)atoi(argv[3]) : 0;
   const char *capem = argc > 4 && *argv[4] ? argv[4] : NULL;
   char       *pem   = NULL;
   int         rounds = argc > 5 ? atoi(argv[5]) : 1;
   int         round;
   struct addrinfo *addr = NULL;
   char req[512];
   char buf[4096];
   void *ssl;
   int   fd;
   ssize_t n;
   bool   err = false;

   network_init();
   ssl_socket_set_verify_mode(mode);
   if (capem)
   {
      FILE *f = fopen(capem, "rb");
      long  n;
      if (!f)
         return 2;
      fseek(f, 0, SEEK_END);
      n = ftell(f);
      fseek(f, 0, SEEK_SET);
      pem = (char*)malloc((size_t)n + 1);
      if (fread(pem, 1, (size_t)n, f) != (size_t)n)
         return 2;
      pem[n] = '\0';
      fclose(f);
      ssl_socket_retro_set_trust_pem(pem, (size_t)n);
   }
   for (round = 0; round < (rounds > 0 ? rounds : 1); round++)
   {
   fd = socket_init((void**)&addr, port, host, SOCKET_TYPE_STREAM, AF_INET);
   if (fd < 0 || !addr)
   {
      fprintf(stderr, "resolve/socket failed\n");
      return 2;
   }
   ssl = ssl_socket_init(fd, host);
   if (!ssl)
      return 2;
   if (ssl_socket_connect(ssl, addr, true, false) < 0)
   {
      fprintf(stderr, "TLS connect failed (%d)\n", ssl_socket_last_error(ssl));
      ssl_socket_free(ssl);
      return 1;
   }
   snprintf(req, sizeof(req), "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: RetroArch tls_fetch\r\n\r\n", host);
   if (ssl_socket_send_all_blocking(ssl, req, strlen(req), true) < 0)
   {
      fprintf(stderr, "send failed\n");
      return 1;
   }
   n = ssl_socket_receive_all_nonblocking(ssl, &err, buf, sizeof(buf) - 1);
   while (n == 0 && !err)
      n = ssl_socket_receive_all_nonblocking(ssl, &err, buf, sizeof(buf) - 1);
   if (n <= 0)
   {
      fprintf(stderr, "receive failed (%d)\n", ssl_socket_last_error(ssl));
      return 1;
   }
   buf[n] = '\0';
   printf("%s: %.*s%s%s\n", host, (int)(strcspn(buf, "\r\n")), buf,
         ssl_socket_retro_version(ssl) == 0x0304 ? " [TLS 1.3]" : " [TLS 1.2]",
         ssl_socket_retro_was_resumed(ssl) ? " (resumed)" : "");
   /* every round after the first must resume: a 1.2 session or a 1.3 PSK */
   if (round > 0 && !ssl_socket_retro_was_resumed(ssl))
   {
      fprintf(stderr, "round %d did not resume\n", round + 1);
      return 1;
   }
   ssl_socket_close(ssl);
   ssl_socket_free(ssl);
   freeaddrinfo_retro(addr);
   if (strncmp(buf, "HTTP/1.", 7) != 0)
      return 1;
   }
   return 0;
}
