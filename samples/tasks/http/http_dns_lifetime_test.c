/* Exercise eviction during the production connect path, without network I/O. */
#include <assert.h>
#include "../../../libretro-common/net/net_http.c"

static retro_time_t now;
static unsigned freed_addresses, connect_calls;
static bool connect_success, init_success;
static struct addrinfo address;

retro_time_t cpu_features_get_time_usec(void) { return now; }
void freeaddrinfo_retro(struct addrinfo *addr)
{
   assert(addr == &address);
   freed_addresses++;
}
int socket_close(int fd) { (void)fd; return 0; }
int socket_next(void **addr) { *addr = NULL; return -1; }

/* No nested-connect case any more: it modelled a second transfer, on
 * another thread, running the expiry sweep while this connect held the
 * address, which a per-entry user count pinned.  Transfers are driven
 * from one thread now, so that interleaving cannot happen and the pin
 * is gone; what is left to check is that a connect, successful or not,
 * leaves the entry to expire normally afterwards. */
static bool attempt(void *addr)
{
   assert(addr == &address);
   connect_calls++;
   return connect_success;
}
bool socket_connect_with_timeout(int fd, void *addr, int timeout)
{
   (void)fd; (void)timeout;
   return attempt(addr);
}
#ifdef HAVE_SSL
void *ssl_socket_init(int fd, const char *domain)
{
   (void)fd; (void)domain;
   return init_success ? &address : NULL;
}
int ssl_socket_connect(void *state, void *addr, bool timeout, bool nonblock)
{
   (void)state; (void)timeout; (void)nonblock;
   return attempt(addr) ? 0 : -1;
}
int ssl_socket_last_error(void *state) { (void)state; return -1; }
void ssl_socket_close(void *state) { (void)state; }
void ssl_socket_free(void *state) { (void)state; }
#endif

static void run_case(bool tls, bool success, bool initialize)
{
   struct http_t state;
   memset(&state, 0, sizeof(state));
   now = 1;
   freed_addresses = connect_calls = 0;
   connect_success = success;
   init_success = initialize;
   state.request.domain = "example.invalid";
   state.request.port = 80;
   state.ssl = tls;
   assert(net_http_dns_cache_add(state.request.domain, 80, &address));
   state.conn = net_http_conn_pool_add(state.request.domain, 80, 7, tls);
   assert(state.conn);
   assert(net_http_connect(&state) == success);
   if (!tls || initialize)
      assert(connect_calls == 1);
   if (state.conn)
      net_http_conn_pool_remove(state.conn);
   now += dns_cache_timeout + 1;
   net_http_dns_cache_remove_expired();
   assert(!dns_cache && freed_addresses == 1);
}

#ifdef HAVE_SSL
static void test_missing_connection(void)
{
   struct http_t state;
   memset(&state, 0, sizeof(state));
   now = 1;
   freed_addresses = 0;
   state.request.domain = "example.invalid";
   state.request.port = 80;
   state.ssl = true;
   assert(net_http_dns_cache_add(state.request.domain, 80, &address));
   assert(!net_http_connect(&state));
   now += dns_cache_timeout + 1;
   net_http_dns_cache_remove_expired();
   assert(!dns_cache && freed_addresses == 1);
}
#endif

int main(void)
{
   run_case(false, true, true);
   run_case(false, false, true);
#ifdef HAVE_SSL
   run_case(true, true, true);
   run_case(true, false, true);
   run_case(true, false, false);
   test_missing_connection();
#else
   /* Unsupported TLS must also release the cache entry. */
   run_case(true, false, false);
#endif
   puts("HTTP DNS connect lifetime tests passed");
   return 0;
}
