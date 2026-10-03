/* Pulls the real BearSSL backend into the test build and exposes its
 * static trust-store loader, so the test can install its own CA before
 * the first handshake instead of depending on the host's bundle at
 * /etc/ssl/certs/ca-certificates.crt. */
#include "../../../libretro-common/net/net_socket_ssl_bear.c"

void cheevos_login_test_seed_trust(char *pem);
void cheevos_login_test_seed_trust(char *pem)
{
   append_certs_pem_x509(pem);
}
