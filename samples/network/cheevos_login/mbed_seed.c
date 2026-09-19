/* Pulls the real Mbed TLS backend into the test build and wraps its
 * ssl_socket_init so every socket also trusts the test CA named by
 * TEST_CA_PEM.  The backend only knows its baked-in bundle and asks for
 * MBEDTLS_SSL_VERIFY_OPTIONAL, which used to be enough for the mock
 * server's certificate - but Mbed TLS 3.6.0 ignores the optional mode
 * under TLS 1.3 and fails the handshake on an untrusted chain, and a
 * handshake that verifies is the better test on every version anyway. */
#include <stdlib.h>

#define ssl_socket_init cheevos_login_test_real_ssl_socket_init
#include "../../../libretro-common/net/net_socket_ssl_mbed.c"
#undef ssl_socket_init

void *ssl_socket_init(int fd, const char *domain);
void *ssl_socket_init(int fd, const char *domain)
{
   struct ssl_state *state = (struct ssl_state*)
      cheevos_login_test_real_ssl_socket_init(fd, domain);
#if defined(MBEDTLS_X509_CRT_PARSE_C) && defined(MBEDTLS_FS_IO)
   const char *ca_path     = getenv("TEST_CA_PEM");

   if (state && ca_path
         && mbedtls_x509_crt_parse_file(&state->ca, ca_path) != 0)
   {
      ssl_socket_free(state);
      return NULL;
   }
#endif
   return state;
}
