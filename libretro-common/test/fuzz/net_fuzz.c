/* libFuzzer harnesses for the network clients' parsers. Each target
 * hands the fuzzer's input to the client as "what the server sent":
 * a listener in this process accepts every connection the client
 * makes and answers it with the input, so the client's real socket
 * and parsing paths run against arbitrary bytes. Build with
 *   clang -fsanitize=fuzzer,address,undefined -DTARGET_<TLS|SMB|NFS|X509>
 * (see the Makefile) and run with a time or run limit. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/net_compat.h>
#include <net/net_socket.h>

#if !defined(TARGET_X509)
static const uint8_t *fuzz_data;
static size_t         fuzz_len;
static int            listen_fd = -1;
static int            listen_port;
static pthread_mutex_t fuzz_lock = PTHREAD_MUTEX_INITIALIZER;

static void *serve(void *arg)
{
   (void)arg;
   for (;;)
   {
      int c = accept(listen_fd, NULL, NULL);
      int one = 1;
      uint8_t *copy = NULL;
      size_t   copy_len;
      if (c < 0)
         break;
      /* the fuzzer owns its input only for the duration of the
       * iteration; take a copy under the lock so a connection that
       * outlives it never reads freed memory */
      pthread_mutex_lock(&fuzz_lock);
      copy_len = fuzz_len;
      if (copy_len && (copy = (uint8_t*)malloc(copy_len)))
         memcpy(copy, fuzz_data, copy_len);
      else
         copy_len = 0;
      pthread_mutex_unlock(&fuzz_lock);
      setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      /* drain what the client sends first so its write never blocks,
       * then hand it the input and close */
      {
         char sink[4096];
         struct timeval tv = { 0, 20000 };
         setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
         (void)recv(c, sink, sizeof(sink), 0);
      }
      if (copy_len)
         (void)send(c, copy, copy_len, MSG_NOSIGNAL);
      free(copy);
      shutdown(c, SHUT_WR);
      {
         char sink[4096];
         struct timeval tv = { 0, 50000 };
         setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
         while (recv(c, sink, sizeof(sink), 0) > 0) ;
      }
      close(c);
   }
   return NULL;
}

static void listener_start(void)
{
   struct sockaddr_in a;
   socklen_t al = sizeof(a);
   pthread_t t;
   int one = 1;
   listen_fd = socket(AF_INET, SOCK_STREAM, 0);
   setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
   memset(&a, 0, sizeof(a));
   a.sin_family = AF_INET;
   a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   bind(listen_fd, (struct sockaddr*)&a, sizeof(a));
   listen(listen_fd, 16);
   getsockname(listen_fd, (struct sockaddr*)&a, &al);
   listen_port = ntohs(a.sin_port);
   pthread_create(&t, NULL, serve, NULL);
   pthread_detach(t);
}
#endif

#if defined(TARGET_TLS)
#include <net/net_socket_ssl.h>
void ssl_socket_log_verify_fail(int m, const char *d, const char *i) { (void)m; (void)d; (void)i; }
void ssl_socket_log_verify_disabled(const char *d) { (void)d; }
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len)
{
   struct addrinfo *addr = NULL;
   int fd; void *ssl; uint8_t buf[64];
   if (listen_fd < 0) { network_init(); listener_start(); ssl_socket_set_verify_mode(2); }
   pthread_mutex_lock(&fuzz_lock); fuzz_data = data; fuzz_len = len; pthread_mutex_unlock(&fuzz_lock);
   fd = socket_init((void**)&addr, listen_port, "127.0.0.1", SOCKET_TYPE_STREAM, AF_INET);
   if (fd < 0 || !addr) return 0;
   ssl = ssl_socket_init(fd, "localhost");
   if (ssl)
   {
      if (ssl_socket_connect(ssl, addr, true, false) >= 0)
      {
         ssl_socket_send_all_blocking(ssl, "x", 1, true);
         ssl_socket_receive_all_blocking(ssl, buf, sizeof(buf));
      }
      ssl_socket_close(ssl);
      ssl_socket_free(ssl);
   }
   freeaddrinfo_retro(addr);
   return 0;
}
#elif defined(TARGET_SMB)
#include <net/net_smb2.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len)
{
   struct rsmb_ctx *c;
   char server[64];
   if (listen_fd < 0) { network_init(); listener_start(); }
   pthread_mutex_lock(&fuzz_lock); fuzz_data = data; fuzz_len = len; pthread_mutex_unlock(&fuzz_lock);
   snprintf(server, sizeof(server), "127.0.0.1");
   c = rsmb_new();
   if (!c) return 0;
   rsmb_set_port(c, (uint16_t)listen_port);
   rsmb_set_credentials(c, "u", "p", "D");
   rsmb_set_timeout(c, 1);
   if (rsmb_connect(c, server, "share") == 0)
   {
      struct rsmb_stat st;
      struct rsmb_dir *d;
      rsmb_stat(c, "a", &st);
      if ((d = rsmb_opendir(c, "/")))
      {
         while (rsmb_readdir(c, d)) ;
         rsmb_closedir(c, d);
      }
   }
   rsmb_free(c);
   return 0;
}
#elif defined(TARGET_NFS)
#include <net/net_nfs3.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len)
{
   struct rnfs_ctx *c;
   if (listen_fd < 0) { network_init(); listener_start(); }
   pthread_mutex_lock(&fuzz_lock); fuzz_data = data; fuzz_len = len; pthread_mutex_unlock(&fuzz_lock);
   c = rnfs_new();
   if (!c) return 0;
   rnfs_set_ports(c, (uint16_t)listen_port, (uint16_t)listen_port);
   rnfs_set_timeout(c, 1);
   if (rnfs_connect(c, "127.0.0.1", "/export") == 0)
   {
      struct rnfs_stat st;
      struct rnfs_dir *d;
      struct rnfs_file *f;
      uint8_t buf[256];
      rnfs_stat(c, "a/b", &st);
      if ((f = rnfs_open(c, "a", RNFS_O_RDONLY)))
      {
         rnfs_read(c, f, buf, sizeof(buf));
         rnfs_close(c, f);
      }
      if ((d = rnfs_opendir(c, "/")))
      {
         while (rnfs_readdir(c, d)) ;
         rnfs_closedir(c, d);
      }
   }
   rnfs_free(c);
   return 0;
}
#elif defined(TARGET_X509)
#include <crypto/x509.h>
#include <time.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len)
{
   struct x509_cert c;
   char info[128];
   const uint8_t *ders[1] = { data };
   size_t lens[1] = { len };
   /* the input as a DER certificate, and as a PEM trust bundle */
   if (x509_parse(&c, data, len) == 0)
   {
      x509_match_hostname(&c, "example.com");
      x509_verify_chain(ders, lens, 1, "example.com", time(NULL), info, sizeof(info));
   }
   x509_trust_load_pem((const char*)data, len);
   return 0;
}
#endif
