/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_icmp.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <net/net_icmp.h>

/* Tier 1: Windows desktop, through the ICMP helper API.  This needs no
 * privileges at all and has shipped since Windows 2000. */
#if defined(_WIN32) && !defined(_XBOX)

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

struct net_icmp_echo
{
   HANDLE   icmp;
   HANDLE   event;
   void     *reply_buf;
   DWORD    reply_len;
   uint32_t dest;
   uint16_t seq;
   int      in_flight;
};

net_icmp_echo_t *net_icmp_echo_open(uint32_t dest_addr)
{
   net_icmp_echo_t *e = (net_icmp_echo_t*)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   e->icmp = IcmpCreateFile();
   if (e->icmp == INVALID_HANDLE_VALUE)
   {
      free(e);
      return NULL;
   }
   e->event = CreateEvent(NULL, TRUE, FALSE, NULL);
   if (!e->event)
   {
      IcmpCloseHandle(e->icmp);
      free(e);
      return NULL;
   }
   e->reply_len = sizeof(ICMP_ECHO_REPLY) + 1500;
   e->reply_buf = malloc(e->reply_len);
   if (!e->reply_buf)
   {
      CloseHandle(e->event);
      IcmpCloseHandle(e->icmp);
      free(e);
      return NULL;
   }
   e->dest = dest_addr;
   return e;
}

int net_icmp_echo_send(net_icmp_echo_t *e, uint16_t seq,
      const void *payload, size_t len)
{
   DWORD ret;
   if (!e || e->in_flight || len > 1400)
      return -1;
   ResetEvent(e->event);
   /* Timeout below only bounds the kernel's own bookkeeping; the caller
    * decides when to give up by ceasing to poll. */
   ret = IcmpSendEcho2(e->icmp, e->event, NULL, NULL,
         (IPAddr)e->dest, (LPVOID)payload, (WORD)len, NULL,
         e->reply_buf, e->reply_len, 4000);
   if (ret == 0 && GetLastError() != ERROR_IO_PENDING)
      return -1;
   e->seq       = seq;
   e->in_flight = 1;
   return 0;
}

int net_icmp_echo_poll(net_icmp_echo_t *e, uint16_t *seq, int *ttl)
{
   ICMP_ECHO_REPLY *rep;
   if (!e || !e->in_flight)
      return -1;
   if (WaitForSingleObject(e->event, 0) != WAIT_OBJECT_0)
      return 0;
   e->in_flight = 0;
   if (IcmpParseReplies(e->reply_buf, e->reply_len) < 1)
      return -1;
   rep = (ICMP_ECHO_REPLY*)e->reply_buf;
   if (rep->Status != IP_SUCCESS)
      return -1;
   if (seq)
      *seq = e->seq;
   if (ttl)
      *ttl = (int)rep->Options.Ttl;
   return 1;
}

void net_icmp_echo_close(net_icmp_echo_t *e)
{
   if (!e)
      return;
   free(e->reply_buf);
   CloseHandle(e->event);
   IcmpCloseHandle(e->icmp);
   free(e);
}

/* Tier 2: Linux and macOS, through ICMP sockets.  Datagram ICMP needs
 * no privileges where net.ipv4.ping_group_range (Linux) permits it and
 * is always available on macOS; a raw socket is tried second for
 * processes that happen to hold the privilege.  With datagram ICMP the
 * kernel owns the echo identifier and strips the IP header on receive;
 * with a raw socket the reply arrives with the IP header attached, so
 * receive parsing carries both shapes. */
#elif defined(__linux__) || defined(__APPLE__)

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

struct net_icmp_echo
{
   int      fd;
   int      raw;       /* raw socket: replies include the IP header */
   uint32_t dest;
   uint16_t ident;
   uint16_t seq;
   int      in_flight;
};

static uint16_t net_icmp_cksum(const void *data, size_t len)
{
   const uint8_t *p = (const uint8_t*)data;
   uint32_t sum     = 0;
   while (len > 1)
   {
      sum += (uint32_t)(((uint16_t)p[0] << 8) | p[1]);
      p   += 2;
      len -= 2;
   }
   if (len)
      sum += (uint32_t)((uint16_t)p[0] << 8);
   while (sum >> 16)
      sum = (sum & 0xffff) + (sum >> 16);
   return (uint16_t)~sum;
}

net_icmp_echo_t *net_icmp_echo_open(uint32_t dest_addr)
{
   int fl;
   net_icmp_echo_t *e = (net_icmp_echo_t*)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   e->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
   if (e->fd < 0)
   {
      e->fd  = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
      e->raw = 1;
   }
   if (e->fd < 0)
   {
      free(e);
      return NULL;
   }
   fl = fcntl(e->fd, F_GETFL, 0);
   if (fl < 0 || fcntl(e->fd, F_SETFL, fl | O_NONBLOCK) < 0)
   {
      close(e->fd);
      free(e);
      return NULL;
   }
   e->dest  = dest_addr;
   e->ident = (uint16_t)(getpid() & 0xffff);
   return e;
}

int net_icmp_echo_send(net_icmp_echo_t *e, uint16_t seq,
      const void *payload, size_t len)
{
   uint8_t pkt[8 + 1400];
   struct sockaddr_in sa;
   uint16_t ck;
   if (!e || e->in_flight || len > 1400)
      return -1;

   memset(pkt, 0, 8);
   pkt[0] = 8;                          /* ICMP_ECHO */
   pkt[4] = (uint8_t)(e->ident >> 8);   /* kernel rewrites this pair on
                                         * datagram sockets; harmless   */
   pkt[5] = (uint8_t)(e->ident & 0xff);
   pkt[6] = (uint8_t)(seq >> 8);
   pkt[7] = (uint8_t)(seq & 0xff);
   if (len)
      memcpy(pkt + 8, payload, len);
   ck     = net_icmp_cksum(pkt, 8 + len);
   pkt[2] = (uint8_t)(ck >> 8);
   pkt[3] = (uint8_t)(ck & 0xff);

   memset(&sa, 0, sizeof(sa));
   sa.sin_family      = AF_INET;
   sa.sin_addr.s_addr = e->dest;
   if (sendto(e->fd, pkt, 8 + len, 0,
         (struct sockaddr*)&sa, sizeof(sa)) != (ssize_t)(8 + len))
      return -1;
   e->seq       = seq;
   e->in_flight = 1;
   return 0;
}

int net_icmp_echo_poll(net_icmp_echo_t *e, uint16_t *seq, int *ttl)
{
   /* Large enough for any reply this handle can match: send caps the
    * payload at 1400, so a raw-socket echo reply tops out at a 60 byte
    * IP header plus 8 byte ICMP header plus payload.  Oversized foreign
    * packets are truncated by recv() and rejected on their leading
    * bytes, which truncation leaves intact. */
   uint8_t buf[1500];
   ssize_t n;
   size_t  off = 0;
   int     got_ttl = -1;
   if (!e || !e->in_flight)
      return -1;
   n = recv(e->fd, buf, sizeof(buf), 0);
   if (n < 0)
      return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
   if (e->raw)
   {
      /* Raw replies carry the IP header; its IHL gives the offset and
       * its TTL field the hop count. */
      if (n < 20)
         return 0;
      off     = (size_t)((buf[0] & 0x0f) * 4);
      got_ttl = buf[8];
   }
   if ((size_t)n < off + 8)
      return 0;
   if (buf[off] == 3)                   /* destination unreachable */
   {
      e->in_flight = 0;
      return -1;
   }
   if (buf[off] != 0)                   /* not an echo reply: keep waiting */
      return 0;
   e->in_flight = 0;
   if (seq)
      *seq = (uint16_t)(((uint16_t)buf[off + 6] << 8) | buf[off + 7]);
   if (ttl)
      *ttl = got_ttl;
   return 1;
}

void net_icmp_echo_close(net_icmp_echo_t *e)
{
   if (!e)
      return;
   close(e->fd);
   free(e);
}

/* Tier 3: everything else.  open() fails and the caller's unreachable-
 * host path carries it from there. */
#else

net_icmp_echo_t *net_icmp_echo_open(uint32_t dest_addr)
{
   (void)dest_addr;
   return NULL;
}

int net_icmp_echo_send(net_icmp_echo_t *e, uint16_t seq,
      const void *payload, size_t len)
{
   (void)e; (void)seq; (void)payload; (void)len;
   return -1;
}

int net_icmp_echo_poll(net_icmp_echo_t *e, uint16_t *seq, int *ttl)
{
   (void)e; (void)seq; (void)ttl;
   return -1;
}

void net_icmp_echo_close(net_icmp_echo_t *e)
{
   (void)e;
}

#endif
