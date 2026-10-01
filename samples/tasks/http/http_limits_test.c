/* Bounds on net_http's DNS cache and connection pool, and the socket
 * family fallback, without network I/O: net_http.c is included whole
 * and the clock, the resolver and the socket calls are stubbed. */
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
int getaddrinfo_retro(const char *node, const char *service,
      struct addrinfo *hints, struct addrinfo **res)
{
   (void)node; (void)service; (void)hints;
   *res = NULL;
   return -1;
}
void freeaddrinfo_retro(struct addrinfo *addr) { (void)addr; freed_addresses++; }
int socket_close(int fd) { (void)fd; closed_sockets++; return 0; }

static int test_socket(int family, int type, int protocol)
{
   (void)family; (void)type; (void)protocol;
   return 7;
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

int main(void)
{
   test_dns_cap();
   test_pool_limits();
   if (failures)
   {
      printf("%d check(s) failed\n", failures);
      return 1;
   }
   puts("net_http limits: all checks passed");
   return 0;
}
