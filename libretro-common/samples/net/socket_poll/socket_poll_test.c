/* socket_poll with more sockets than a Winsock fd_set holds.
 *
 * On Windows socket_poll runs on select(), whose fd_set takes 64
 * sockets (FD_SETSIZE); past that FD_SET dropped them, so a socket
 * beyond the 64th was never reported. Here 150 UDP sockets on the
 * loopback: two get a datagram, and exactly those two, the last
 * among them, are readable; all are writable; with none ready the
 * poll times out. The same checks run on poll() elsewhere. */
#include <stdio.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_socket.h>

#define COUNT 150

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-60s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

#ifdef _WIN32
typedef SOCKET sock_t;
#define BAD_SOCK INVALID_SOCKET
#define CLOSE_SOCK closesocket
#else
#include <unistd.h>
typedef int sock_t;
#define BAD_SOCK (-1)
#define CLOSE_SOCK close
#endif

int main(void)
{
   static sock_t      socks[COUNT];
   static struct pollfd fds[COUNT];
   struct sockaddr_in addr;
   unsigned           i;
   int                n, made = 1;
   const unsigned     hot[2] = { 70, COUNT - 1 };

   if (!network_init())
   {
      printf("socket_poll_test: no network\n");
      return 1;
   }

   for (i = 0; i < COUNT; i++)
   {
      socklen_t len = sizeof(addr);
      socks[i] = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      memset(&addr, 0, sizeof(addr));
      addr.sin_family      = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      if (socks[i] == BAD_SOCK
            || bind(socks[i], (struct sockaddr*)&addr, sizeof(addr)) != 0
            || getsockname(socks[i], (struct sockaddr*)&addr, &len) != 0)
         made = 0;
      /* Each socket sends to itself. */
      if (made && connect(socks[i], (struct sockaddr*)&addr, sizeof(addr)) != 0)
         made = 0;
      fds[i].fd = socks[i];
   }
   check("150 loopback sockets", made);
   if (!made)
      return 1;

   for (i = 0; i < COUNT; i++)
      fds[i].events = POLLIN;
   n = socket_poll(fds, COUNT, 50);
   check("none readable: the poll times out", n == 0);

   for (i = 0; i < 2; i++)
      send(socks[hot[i]], "x", 1, 0);
   n = socket_poll(fds, COUNT, 1000);
   {
      unsigned readable = 0;
      int      right    = 1;
      for (i = 0; i < COUNT; i++)
      {
         int want = (i == hot[0] || i == hot[1]);
         if (fds[i].revents & POLLIN)
            readable++;
         if (!!(fds[i].revents & POLLIN) != want)
            right = 0;
      }
      check("two readable, counted", n == 2 && readable == 2);
      check("the 71st and the 150th, and no other", right);
   }

   for (i = 0; i < COUNT; i++)
      fds[i].events = POLLOUT;
   n = socket_poll(fds, COUNT, 1000);
   {
      unsigned writable = 0;
      for (i = 0; i < COUNT; i++)
         if (fds[i].revents & POLLOUT)
            writable++;
      check("all 150 writable", n == COUNT && writable == COUNT);
   }

   /* Below FD_SETSIZE, the sets on the stack. */
   for (i = 0; i < 8; i++)
      fds[i].events = POLLIN;
   n = socket_poll(fds, 8, 50);
   check("eight sockets, none readable", n == 0);

   for (i = 0; i < COUNT; i++)
      CLOSE_SOCK(socks[i]);

   printf("socket_poll_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
