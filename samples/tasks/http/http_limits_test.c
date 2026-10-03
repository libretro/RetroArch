/* Bounds on net_http's DNS cache and connection pool, the socket family
 * fallback and IPv6 literal handling, without network I/O: net_http.c is
 * included whole and the clock, the resolver and the socket calls are
 * stubbed. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <rthreads/rthreads.h>

static int test_socket(int family, int type, int protocol);
#define socket test_socket
#include "../../../libretro-common/net/net_http.c"
#undef socket

static retro_time_t now = 1;
static unsigned freed_addresses, closed_sockets;
static int failures;

retro_time_t cpu_features_get_time_usec(void) { return now; }
bool network_init(void) { return true; }
static char resolved_node[64];
int getaddrinfo_retro(const char *node, const char *service,
      struct addrinfo *hints, struct addrinfo **res)
{
   (void)service; (void)hints;
   snprintf(resolved_node, sizeof(resolved_node), "%s", node);
   *res = NULL;
   return -1;
}
void freeaddrinfo_retro(struct addrinfo *addr) { (void)addr; freed_addresses++; }
int socket_close(int fd) { (void)fd; closed_sockets++; return 0; }

/* A system without IPv6: socket() refuses the family, as it does on
 * kernels and console stacks built without it. */
static bool refuse_v6;
static int  sockets_made;

static int test_socket(int family, int type, int protocol)
{
   (void)type; (void)protocol;
   if (refuse_v6 && family == AF_INET6)
      return -1;
   sockets_made++;
   return 7;
}

/* Connect fails for the address in fail_addr, succeeds otherwise, and
 * records which address it was asked for. */
static struct addrinfo *fail_addr, *last_connect;
bool socket_connect_with_timeout(int fd, void *addr, int timeout)
{
   (void)fd; (void)timeout;
   last_connect = (struct addrinfo*)addr;
   return addr != fail_addr;
}

static void check(int ok, const char *what)
{
   printf("[%s] %s\n", ok ? "pass" : "FAIL", what);
   if (!ok)
      failures++;
}

static struct addrinfo address;

static struct dns_cache_entry *dns_find_port(int port)
{
   struct dns_cache_entry *e;
   for (e = dns_cache; e; e = e->next)
      if (e->port == port)
         return e;
   return NULL;
}

static unsigned dns_count(void)
{
   unsigned n = 0;
   struct dns_cache_entry *e;
   for (e = dns_cache; e; e = e->next)
      n++;
   return n;
}

/* ---- DNS cache cap ---- */

static void test_dns_cap(void)
{
   int i;
   struct dns_cache_entry *pending;

   net_http_init();
   freed_addresses = 0;

   /* A lookup still in flight, and the oldest entry of all: it must
    * survive every eviction, since its resolver owns it until it
    * publishes. */
   pending = net_http_dns_cache_add("example.invalid", 1, NULL);
#ifdef HAVE_THREADS
   pending->thread = (sthread_t*)&address;   /* never joined: see below */
#endif

   for (i = 0; i < 100; i++)
   {
      now++;
      assert(net_http_dns_cache_add("example.invalid", 1000 + i, &address));
   }

   printf("  100 hosts resolved: %u cache entries\n", dns_count());
   check(dns_count() <= NET_HTTP_DNS_CACHE_MAX,
         "DNS cache stays within its cap after 100 distinct hosts");
   check(dns_find_port(1099) && dns_find_port(1099 - NET_HTTP_DNS_CACHE_MAX + 2)
         && !dns_find_port(1000),
         "DNS cache evicts the least recently used entries");
   {
      unsigned with_addr = 0;
      struct dns_cache_entry *e;
      for (e = dns_cache; e; e = e->next)
         with_addr += (e->addr != NULL);
      check(freed_addresses == 100 - with_addr,
            "evicted entries free their addresses");
   }
#ifdef HAVE_THREADS
   check(dns_find_port(1) == pending,
         "an entry still being resolved is never evicted");
   pending->thread = NULL;
   retro_atomic_store_release_int(&pending->valid, 1);
#else
   (void)pending;   /* no resolver threads: nothing is ever in flight */
#endif
   net_http_deinit();
}

/* ---- connection pool: idle cap and idle TTL ---- */

static unsigned pool_idle(void)
{
   unsigned n = 0;
   struct conn_pool_entry *e;
   for (e = conn_pool; e; e = e->next)
      n += !e->in_use;
   return n;
}

static void test_pool_limits(void)
{
   int i;
   struct conn_pool_entry *c;

   net_http_init();
   closed_sockets = 0;
   for (i = 0; i < 20; i++)
   {
      now++;
      c = net_http_conn_pool_add("example.invalid", 2000 + i, 100 + i, false);
      assert(c);
      net_http_conn_pool_release(c);
   }
   printf("  20 connections finished: %u idle in the pool\n", pool_idle());
   check(pool_idle() == NET_HTTP_POOL_MAX_IDLE
         && closed_sockets == 20 - NET_HTTP_POOL_MAX_IDLE,
         "pool keeps at most 8 idle connections, closing the rest");
   check(!net_http_conn_pool_find("example.invalid", 2000)
         && (c = net_http_conn_pool_find("example.invalid", 2019)) != NULL,
         "pool closes the longest-idle connection first");
   net_http_conn_pool_release(c);

   /* Within the TTL a connection is reused... */
   now += NET_HTTP_POOL_IDLE_TTL / 2;
   c = net_http_conn_pool_find("example.invalid", 2019);
   check(c != NULL, "idle connection within the TTL is reused");
   net_http_conn_pool_release(c);

   /* ...past it, it is closed instead. */
   closed_sockets = 0;
   now += NET_HTTP_POOL_IDLE_TTL + 1;
   check(!net_http_conn_pool_find("example.invalid", 2019)
         && closed_sockets == NET_HTTP_POOL_MAX_IDLE && !conn_pool,
         "idle connections past the TTL are closed, not reused");
   net_http_deinit();
}

/* ---- socket family fallback ---- */

static struct sockaddr_in6 sa6;
static struct sockaddr_in  sa4a, sa4b;
static struct addrinfo     ai6, ai4a, ai4b;

static void make_list(struct addrinfo *a, struct addrinfo *b, struct addrinfo *c)
{
   memset(&ai6, 0, sizeof(ai6));
   memset(&ai4a, 0, sizeof(ai4a));
   memset(&ai4b, 0, sizeof(ai4b));
   ai6.ai_family   = AF_INET6;  ai6.ai_socktype  = SOCK_STREAM;
   ai6.ai_addr     = (struct sockaddr*)&sa6;  ai6.ai_addrlen = sizeof(sa6);
   ai4a.ai_family  = AF_INET;   ai4a.ai_socktype = SOCK_STREAM;
   ai4a.ai_addr    = (struct sockaddr*)&sa4a; ai4a.ai_addrlen = sizeof(sa4a);
   ai4b.ai_family  = AF_INET;   ai4b.ai_socktype = SOCK_STREAM;
   ai4b.ai_addr    = (struct sockaddr*)&sa4b; ai4b.ai_addrlen = sizeof(sa4b);
   a->ai_next = b;
   b->ai_next = c;
   c->ai_next = NULL;
}

static bool connect_via(struct addrinfo *head, struct addrinfo **used)
{
   struct http_t state;
   bool ok = false;

   memset(&state, 0, sizeof(state));
   state.request.domain = "dualstack.invalid";
   state.request.port   = 80;
   net_http_init();
   assert(net_http_dns_cache_add(state.request.domain, 80, head));
   last_connect = NULL;
   if (net_http_new_socket(&state) && state.conn)
      ok = net_http_connect(&state);
   *used = last_connect;
   if (state.conn)
      net_http_conn_pool_remove(state.conn);
   net_http_deinit();
   return ok;
}

static void test_family_fallback(void)
{
   struct addrinfo *used;

   refuse_v6 = true;

   /* IPv6 listed first, the system has none. */
   make_list(&ai6, &ai4a, &ai4b);
   fail_addr = NULL;
   check(connect_via(&ai6, &used) && used == &ai4a,
         "IPv6 first, no IPv6 support: connects over the IPv4 address");

   /* IPv4, IPv6, IPv4: the first IPv4 address refuses the connection.
    * The IPv6 one in between cannot even get a socket; the next IPv4
    * address must still be tried. */
   make_list(&ai4a, &ai6, &ai4b);
   fail_addr = &ai4a;
   check(connect_via(&ai4a, &used) && used == &ai4b,
         "a refused family mid-list does not hide the addresses after it");

   refuse_v6 = false;
   fail_addr = NULL;
}

/* ---- IPv6 literals ---- */

static bool split_url(const char *url, char *domain, size_t size, int *port,
      char *path, size_t psize)
{
   struct http_connection_t *c = net_http_connection_new(url, "GET", NULL);
   bool ok = false;
   if (!c)
      return false;
   net_http_connection_iterate(c);
   if (net_http_connection_done(c))
   {
      snprintf(domain, size, "%s", c->domain);
      snprintf(path, psize, "%s", c->path);
      *port = c->port;
      ok    = true;
   }
   net_http_connection_free(c);
   return ok;
}

static void test_ipv6_literals(void)
{
   char domain[64], path[64];
   int  port = 0;
   struct http_connection_t *conn;
   struct http_t *h;
   struct dns_cache_entry *e;

   check(split_url("http://[::1]:8080/x?y", domain, sizeof(domain), &port,
            path, sizeof(path))
         && !strcmp(domain, "[::1]") && port == 8080 && !strcmp(path, "x?y"),
         "http://[::1]:8080/x?y: host [::1], port 8080 (was split at the first colon)");
   check(split_url("https://[fe80::1%25eth0]/", domain, sizeof(domain), &port,
            path, sizeof(path))
         && !strcmp(domain, "[fe80::1%25eth0]") && port == 443,
         "bracketed literal without a port takes the scheme default");
   check(!split_url("http://[::1/x", domain, sizeof(domain), &port, path, sizeof(path))
         && !split_url("http://[::1]x/", domain, sizeof(domain), &port, path, sizeof(path))
         && !split_url("http://[]/", domain, sizeof(domain), &port, path, sizeof(path)),
         "unterminated, trailing-garbage and empty literals are refused");

   /* The resolver gets the address without its brackets. */
   net_http_init();
   e = net_http_dns_cache_add("[::1]", 80, NULL);
   resolved_node[0] = '\0';
   net_http_resolve(e);
   check(!strcmp(resolved_node, "::1"), "resolver is handed ::1, not [::1]");
   net_http_deinit();

   /* A redirect to a literal, through the join and the host split. */
   net_http_init();
   conn = net_http_connection_new("http://example.invalid/a", "GET", NULL);
   net_http_connection_iterate(conn);
   net_http_connection_done(conn);
   h = net_http_new(conn);
   net_http_connection_free(conn);
   check(h && !net_http_redirect(h, "http://[::1]:8443/v6")
         && !strcmp(h->request.domain, "[::1]") && h->request.port == 8443
         && !strcmp(h->request.path, "v6"),
         "redirect to http://[::1]:8443/v6: host [::1], port 8443");
   check(h && net_http_redirect(h, "http://[::1/broken") && h->err,
         "redirect to an unterminated literal fails cleanly");
   net_http_delete(h);
   net_http_deinit();
}

int main(void)
{
   test_dns_cap();
   test_pool_limits();
   test_family_fallback();
   test_ipv6_literals();
   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   puts("net_http limits: all checks passed");
   return 0;
}
