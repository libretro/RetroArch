/* A server that goes silent must not hold the built-in TLS client:
 * the handshake against a listener that reads the ClientHello and then
 * says nothing, and against one that sends the first bytes of a record
 * and then stops, must both fail once the read bound runs out rather
 * than wait in recv() for good. No TLS server is needed for either.
 *
 * Built with a short SSL_SOCKET_IO_TIMEOUT_MS (see the Makefile). Exit
 * status is the outcome. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>
#include <retro_timers.h>

void ssl_socket_log_verify_fail(int mode_required, const char *domain, const char *info)
{
   (void)mode_required; (void)domain; (void)info;
}
void ssl_socket_log_verify_disabled(const char *domain)
{
   (void)domain;
}

enum stall_mode
{
   STALL_SILENT = 0,  /* nothing after the ClientHello */
   STALL_MID_RECORD   /* a record header and part of its body */
};

static int             srv_fd = -1;
static unsigned short  srv_port;
static enum stall_mode srv_mode;
static int             failures;

/* Takes the ClientHello off the wire: a record header and its body. */
static void srv_read_hello(int fd)
{
   unsigned char hdr[5];
   unsigned char body[512];
   size_t        len;

   if (!socket_receive_all_blocking(fd, hdr, sizeof(hdr)))
      return;
   len = ((size_t)hdr[3] << 8) | hdr[4];
   while (len)
   {
      size_t n = len < sizeof(body) ? len : sizeof(body);
      if (!socket_receive_all_blocking(fd, body, n))
         return;
      len -= n;
   }
}

static void srv_main(void *data)
{
   int i;
   (void)data;
   for (i = 0; i < 2; i++)
   {
      int fd = accept(srv_fd, NULL, NULL);
      if (fd < 0)
         return;
      srv_read_hello(fd);
      if (srv_mode == STALL_MID_RECORD)
      {
         /* a handshake record of 100 bytes, 10 of them sent */
         static const unsigned char part[15] = {
            0x16, 0x03, 0x03, 0x00, 0x64, 0x02, 0x00, 0x00, 0x60, 0x03, 0x03 };
         socket_send_all_blocking(fd, part, sizeof(part), true);
      }
      /* then nothing; the descriptor is left open for the run */
   }
}

static void watchdog(void *data)
{
   (void)data;
   retro_sleep(20000);
   printf("[FAIL] tls_stall: a handshake did not return within 20 s\n");
   fflush(stdout);
   exit(1);
}

static int srv_start(void)
{
   struct sockaddr_in a;
   socklen_t          al = sizeof(a);

   if ((srv_fd = (int)socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return -1;
   memset(&a, 0, sizeof(a));
   a.sin_family      = AF_INET;
   a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (     bind(srv_fd, (struct sockaddr*)&a, sizeof(a))
         || listen(srv_fd, 4)
         || getsockname(srv_fd, (struct sockaddr*)&a, &al))
      return -1;
   srv_port = ntohs(a.sin_port);
   return 0;
}

static void stall_case(enum stall_mode mode, const char *what)
{
   struct addrinfo *addr = NULL;
   retro_time_t     t0;
   int              ms, rc = 0, fd;
   void            *ssl;

   srv_mode = mode;
   fd = socket_init((void**)&addr, srv_port, "127.0.0.1",
         SOCKET_TYPE_STREAM, AF_INET);
   if (fd < 0 || !addr || !(ssl = ssl_socket_init(fd, "localhost")))
   {
      printf("[FAIL] %s: no socket\n", what);
      failures++;
      return;
   }
   t0 = cpu_features_get_time_usec();
   rc = ssl_socket_connect(ssl, addr, true, false);
   ms = (int)((cpu_features_get_time_usec() - t0) / 1000);
   ssl_socket_free(ssl);
   freeaddrinfo_retro(addr);

   if (rc < 0 && ms >= SSL_SOCKET_IO_TIMEOUT_MS && ms < 10000)
      printf("[pass] %s (%d ms)\n", what, ms);
   else
   {
      printf("[FAIL] %s: rc %d after %d ms\n", what, rc, ms);
      failures++;
   }
}

int main(void)
{
   sthread_t *srv, *dog;

   network_init();
   if (srv_start())
   {
      printf("[FAIL] tls_stall: no loopback listener\n");
      return 1;
   }
   if (     !(dog = sthread_create(watchdog, NULL))
         || !(srv = sthread_create(srv_main, NULL)))
      return 1;
   sthread_detach(dog);
   sthread_detach(srv);

   stall_case(STALL_SILENT,
         "a server silent after the ClientHello: the handshake fails");
   stall_case(STALL_MID_RECORD,
         "a server silent mid-record: the handshake fails");

   if (failures)
      return 1;
   printf("[pass] tls_stall\n");
   return 0;
}
