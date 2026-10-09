/* A certificate the client refuses must reach ssl_socket_log_verify_fail
 * as a required failure for the host, whichever backend made the
 * connection: RetroArch's log line and on-screen notice hang off that
 * hook. Run against a server whose certificate chains to no trusted CA
 * (see verify_hook_test.sh). Exit status is the outcome. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>

static int  fail_calls;
static int  fail_required;
static char fail_domain[64];

void ssl_socket_log_verify_fail(int mode_required, const char *domain,
      const char *verify_info)
{
   fail_calls++;
   fail_required = mode_required;
   strncpy(fail_domain, domain ? domain : "", sizeof(fail_domain) - 1);
   printf("hook: required %d, %s: %s\n", mode_required,
         domain ? domain : "(null)", verify_info ? verify_info : "");
}

void ssl_socket_log_verify_disabled(const char *domain)
{
   (void)domain;
}

int main(int argc, char **argv)
{
   struct addrinfo *addr = NULL;
   void            *ssl;
   int              fd, rc;

   if (argc < 2)
   {
      fprintf(stderr, "usage: %s port\n", argv[0]);
      return 2;
   }
   network_init();
   ssl_socket_set_verify_mode(0);
   fd = socket_init((void**)&addr, (uint16_t)atoi(argv[1]), "localhost",
         SOCKET_TYPE_STREAM, AF_INET);
   if (fd < 0 || !addr || !(ssl = ssl_socket_init(fd, "localhost")))
   {
      printf("[FAIL] tls_verify_hook: no socket\n");
      return 1;
   }
   rc = ssl_socket_connect(ssl, addr, true, false);
   ssl_socket_close(ssl);
   ssl_socket_free(ssl);
   freeaddrinfo_retro(addr);

   if (rc >= 0)
   {
      printf("[FAIL] tls_verify_hook: an untrusted certificate was accepted\n");
      return 1;
   }
   if (fail_calls < 1 || !fail_required || strcmp(fail_domain, "localhost"))
   {
      printf("[FAIL] tls_verify_hook: refusal not reported "
            "(calls %d, required %d, host '%s')\n",
            fail_calls, fail_required, fail_domain);
      return 1;
   }
   printf("[pass] tls_verify_hook: a refused certificate reaches the verify hook\n");
   return 0;
}
