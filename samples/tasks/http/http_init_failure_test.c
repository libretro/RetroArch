/* Production HTTP setup with allocator, resolver and socket failures. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <rthreads/rthreads.h>

static unsigned fail_calloc, fail_strdup, allocations;
static unsigned closed_sockets, resolver_calls, thread_calls;
static struct addrinfo address;

static void *test_calloc(size_t n, size_t size)
{
   void *ptr;
   if (fail_calloc && --fail_calloc == 0)
      return NULL;
   ptr = calloc(n, size);
   assert(ptr);
   allocations++;
   return ptr;
}
static char *test_strdup(const char *text)
{
   char *ptr;
   if (fail_strdup && --fail_strdup == 0)
      return NULL;
   assert(text);
   ptr = (char*)malloc(strlen(text) + 1);
   assert(ptr);
   strcpy(ptr, text);
   allocations++;
   return ptr;
}
static void test_free(void *ptr)
{
   if (ptr)
   {
      assert(allocations);
      allocations--;
      free(ptr);
   }
}
static int test_socket(int family, int type, int protocol)
{
   (void)family; (void)type; (void)protocol;
   return 7;
}

#ifdef HAVE_THREADS
static sthread_t *test_thread_create(void (*fn)(void*), void *data)
{
   (void)fn; (void)data;
   thread_calls++;
   return NULL;
}
#endif
#define calloc test_calloc
#define strdup test_strdup
#define free test_free
#define socket test_socket
#define sthread_create test_thread_create
#include "../../../libretro-common/net/net_http.c"
#undef calloc
#undef strdup
#undef free
#undef socket
#undef sthread_create

retro_time_t cpu_features_get_time_usec(void) { return 1; }
bool network_init(void) { return true; }
int getaddrinfo_retro(const char *node, const char *service,
      struct addrinfo *hints, struct addrinfo **res)
{
   (void)service; (void)hints;
   assert(node);
   resolver_calls++;
   *res = &address;
   return 0;
}
void freeaddrinfo_retro(struct addrinfo *addr) { assert(addr == &address); }
int socket_close(int fd) { assert(fd == 7); closed_sockets++; return 0; }

static void finish_case(void)
{
   net_http_deinit();
   assert(!allocations && !dns_cache && !conn_pool);
   fail_calloc = fail_strdup = 0;
}

int main(void)
{
   struct http_t state;
   struct dns_cache_entry *entry;
   unsigned i;
   memset(&state, 0, sizeof(state));
   state.request.domain = "example.invalid";
   state.request.port = 80;

   /* Neither failed allocation may publish a partial cache entry. */
   for (i = 0; i < 2; i++)
   {
      net_http_init();
      if (i) fail_strdup = 1; else fail_calloc = 1;
      assert(!net_http_dns_cache_add(state.request.domain, 80, NULL));
      assert(!dns_cache && !allocations);
      finish_case();
   }
   for (i = 0; i < 2; i++)
   {
      net_http_init();
      if (i) fail_strdup = 1; else fail_calloc = 1;
      assert(!net_http_new_socket(&state));
      assert(!dns_cache && !state.conn && !thread_calls);
      finish_case();
   }
#ifdef HAVE_THREADS
   net_http_init();
   entry = net_http_dns_cache_add("existing.invalid", 80, &address);
   assert(entry);
   assert(!net_http_new_socket(&state));
   assert(thread_calls == 1 && dns_cache == entry && !entry->next);
   assert(!net_http_new_socket(&state));
   assert(thread_calls == 2 && dns_cache == entry && !entry->next);
   finish_case();
#endif

   /* (The resolver no longer copies the domain before resolving - it
    * reads it in place, see net_http_resolve() - so there is no
    * allocation left in it to fail.) */

   /* A pool allocation failure must close the newly created socket. */
   for (i = 0; i < 2; i++)
   {
      net_http_init();
      assert(net_http_dns_cache_add(state.request.domain, 80, &address));
      if (i) fail_strdup = 1; else fail_calloc = 1;
      closed_sockets = 0;
      assert(!net_http_new_socket(&state));
      assert(!state.conn && !conn_pool && closed_sockets == 1);
      finish_case();
   }

   net_http_init();
   entry = net_http_dns_cache_add(state.request.domain, 80, NULL);
   assert(entry);
   net_http_resolve(entry);
   assert(net_http_dns_entry_valid(entry) && entry->addr && resolver_calls == 1);
   closed_sockets = 0;
   assert(net_http_new_socket(&state));
   assert(state.conn && !closed_sockets);
   finish_case();
   assert(closed_sockets == 1);
   puts("HTTP initialization failure tests passed");
   return 0;
}
