/* Wii sockets, run in Dolphin by run-dolphin.sh: Dolphin's IOS uses
 * the host's sockets, so the console talks to itself over loopback. */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <gekko/net.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s (errno %d)", what, errno); \
         failures++; \
      } \
   } while (0)

static struct sockaddr_in loopback(unsigned port)
{
   struct sockaddr_in a;
   memset(&a, 0, sizeof(a));
   a.sin_len         = sizeof(a);
   a.sin_family      = AF_INET;
   a.sin_port        = htons(port);
   a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   return a;
}

static void test_text(void)
{
   struct in_addr a;
   char s[INET_ADDRSTRLEN];
   CHECK(inet_aton("192.168.1.20", &a) && a.s_addr == 0xc0a80114u
         && !strcmp(inet_ntoa(a), "192.168.1.20")
         && !inet_aton("1.2.3", &a) && !inet_aton("1.2.3.256", &a)
         && !inet_aton("1.2.3.4x", &a)
         && inet_pton(AF_INET, "10.0.0.1", &a) == 1 && a.s_addr == 0x0a000001u
         && inet_ntop(AF_INET, &a, s, sizeof(s)) && !strcmp(s, "10.0.0.1")
         && !inet_ntop(AF_INET, &a, s, 4)
         && inet_addr("bad") == INADDR_NONE,
         "net: address text");
}

/* 0 once connected, else the error: connect again once the socket is
 * writable, as libretro-common does (Dolphin's SO_ERROR is the host's,
 * unconverted). */
static int nb_connect(unsigned port)
{
   struct sockaddr_in a = loopback(port);
   struct pollfd p;
   int fd = socket(AF_INET, SOCK_STREAM, 0), err;
   fcntl(fd, F_SETFL, O_NONBLOCK);
   if (connect(fd, (struct sockaddr*)&a, sizeof(a)) == 0)
      err = 0;
   else if (errno != EINPROGRESS && errno != EALREADY)
      err = errno;
   else
   {
      p.fd      = fd;
      p.events  = POLLOUT;
      p.revents = 0;
      if (poll(&p, 1, 2000) != 1)
         err = ETIMEDOUT;
      else if (connect(fd, (struct sockaddr*)&a, sizeof(a)) == 0
            || errno == EISCONN)
         err = 0;
      else
         err = errno;
   }
   close(fd);
   return err;
}

static void test_tcp(void)
{
   struct sockaddr_in a = loopback(47811), peer;
   socklen_t len = sizeof(peer);
   struct pollfd p;
   fd_set rd;
   struct timeval tv;
   char buf[64];
   int one = 1, srv, cli, con, n;

   srv = socket(AF_INET, SOCK_STREAM, 0);
   CHECK(srv >= 0 && setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one,
            sizeof(one)) == 0
         && bind(srv, (struct sockaddr*)&a, sizeof(a)) == 0
         && listen(srv, 2) == 0, "tcp: listen");
   cli = socket(AF_INET, SOCK_STREAM, 0);
   CHECK(cli >= 0 && connect(cli, (struct sockaddr*)&a, sizeof(a)) == 0,
         "tcp: connect");
   con = accept(srv, (struct sockaddr*)&peer, &len);
   CHECK(con >= 0 && peer.sin_family == AF_INET
         && peer.sin_addr.s_addr == htonl(INADDR_LOOPBACK),
         "tcp: accept, with the peer's address");
   setsockopt(cli, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

   CHECK(send(cli, "hello", 5, 0) == 5
         && recv(con, buf, sizeof(buf), 0) == 5 && !memcmp(buf, "hello", 5),
         "tcp: send, recv");
   CHECK(write(con, "back", 4) == 4 && read(cli, buf, sizeof(buf)) == 4
         && !memcmp(buf, "back", 4), "tcp: write, read on the descriptor");

   CHECK(fcntl(cli, F_SETFL, fcntl(cli, F_GETFL) | O_NONBLOCK) == 0
         && (fcntl(cli, F_GETFL) & O_NONBLOCK)
         && recv(cli, buf, sizeof(buf), 0) == -1 && errno == EAGAIN,
         "tcp: non-blocking recv with nothing there");
   p.fd     = cli;
   p.events = POLLIN;
   CHECK(poll(&p, 1, 50) == 0 && !p.revents, "tcp: poll times out");
   send(con, "x", 1, 0);
   CHECK(poll(&p, 1, 1000) == 1 && (p.revents & POLLIN), "tcp: poll sees data");
   FD_ZERO(&rd);
   FD_SET(cli, &rd);
   tv.tv_sec  = 1;
   tv.tv_usec = 0;
   CHECK(select(cli + 1, &rd, NULL, NULL, &tv) == 1 && FD_ISSET(cli, &rd)
         && recv(cli, buf, 1, 0) == 1 && buf[0] == 'x', "tcp: select");

   close(con);
   n = 0;
   fcntl(cli, F_SETFL, 0);
   CHECK(recv(cli, buf, sizeof(buf), 0) == 0, "tcp: end of stream");
   CHECK(close(cli) == 0 && close(srv) == 0, "tcp: close");
   CHECK(send(cli, "x", 1, 0) == -1 && errno == EBADF, "tcp: closed is gone");
   (void)n;

   a   = loopback(47812);
   cli = socket(AF_INET, SOCK_STREAM, 0);
   CHECK(connect(cli, (struct sockaddr*)&a, sizeof(a)) == -1
         && errno == ECONNREFUSED, "tcp: refused");
   close(cli);

   /* As libretro-common connects with a timeout. */
   a   = loopback(47811);
   srv = socket(AF_INET, SOCK_STREAM, 0);
   setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
   bind(srv, (struct sockaddr*)&a, sizeof(a));
   listen(srv, 2);
   CHECK(nb_connect(47811) == 0, "tcp: non-blocking connect");
   CHECK(nb_connect(47812) == ECONNREFUSED, "tcp: non-blocking refused");
   close(srv);
}

static void test_udp(void)
{
   struct sockaddr_in a = loopback(47813), from;
   socklen_t len = sizeof(from);
   char buf[32];
   int u1 = socket(AF_INET, SOCK_DGRAM, 0);
   int u2 = socket(AF_INET, SOCK_DGRAM, 0);
   CHECK(u1 >= 0 && u2 >= 0 && bind(u1, (struct sockaddr*)&a, sizeof(a)) == 0,
         "udp: bind");
   CHECK(sendto(u2, "dgram", 5, 0, (struct sockaddr*)&a, sizeof(a)) == 5
         && recvfrom(u1, buf, sizeof(buf), 0, (struct sockaddr*)&from,
            &len) == 5 && !memcmp(buf, "dgram", 5)
         && from.sin_addr.s_addr == htonl(INADDR_LOOPBACK) && from.sin_port,
         "udp: sendto, recvfrom with the sender");
   close(u1);
   close(u2);
}

int main(void)
{
   struct hostent *h;
   int ret = gk_net_init(10000);
   CHECK(ret == 0 && gk_net_address(), "net: up, with an address");
   gk_debug_printf("     address %08x", (unsigned)gk_net_address());
   CHECK(socket(AF_INET, SOCK_STREAM, 0) >= 3, "net: sockets are descriptors");
   test_text();
   h = gethostbyname("localhost");
   CHECK(h && h->h_addrtype == AF_INET && h->h_length == 4 && h->h_addr_list[0]
         && *(uint32_t*)h->h_addr_list[0] == htonl(INADDR_LOOPBACK),
         "net: gethostbyname");
   CHECK(send(0, "x", 1, 0) == -1 && errno == ENOTSOCK, "net: not a socket");
   test_tcp();
   test_udp();
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   return 0;
}
