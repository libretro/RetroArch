/* Copyright  (C) 2010-2022 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_socket.c).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdio.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#ifdef GEKKO
#include <network.h>
#endif

#include <features/features_cpu.h>

#include <net/net_socket.h>

int socket_init(void **address, uint16_t port, const char *server, enum socket_type type)
{
   char port_buf[16];
   struct addrinfo hints = {0};
   struct addrinfo **addrinfo = (struct addrinfo**)address;
   struct addrinfo *addr = NULL;

   if (!network_init())
      goto error;

   switch (type)
   {
      case SOCKET_TYPE_DATAGRAM:
         hints.ai_socktype = SOCK_DGRAM;
         break;
      case SOCKET_TYPE_STREAM:
         hints.ai_socktype = SOCK_STREAM;
         break;
      case SOCKET_TYPE_SEQPACKET:
         /* TODO/FIXME - implement? */
         break;
   }

   if (!server)
      hints.ai_flags = AI_PASSIVE;

   port_buf[0] = '\0';

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);

   if (getaddrinfo_retro(server, port_buf, &hints, addrinfo) != 0)
      goto error;

   addr = (struct addrinfo*)*addrinfo;

   if (!addr)
      goto error;

   return socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

error:
   return -1;
}

int socket_next(void **addrinfo)
{
   struct addrinfo *addr = (struct addrinfo*)*addrinfo;
   if ((*addrinfo = addr = addr->ai_next))
      return socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
   else
      return -1;
}

ssize_t socket_receive_all_nonblocking(int fd, bool *error,
      void *data_, size_t size)
{
   ssize_t ret = recv(fd, (char*)data_, size, 0);

   if (ret > 0)
      return ret;

   if (ret < 0 && isagain((int)ret))
      return 0;

   *error = true;
   return -1;
}

bool socket_receive_all_blocking(int fd, void *data_, size_t size)
{
   const uint8_t *data = (const uint8_t*)data_;

   while (size)
   {
      ssize_t ret = recv(fd, (char*)data, size, 0);

      if (!ret)
         return false;

      if (ret < 0)
      {
         if (!isagain((int)ret))
            return false;
      }
      else
      {
         data += ret;
         size -= ret;
      }
   }

   return true;
}

bool socket_receive_all_blocking_with_timeout(int fd,
      void *data_, size_t size,
      unsigned timeout)
{
   const uint8_t *data    = (const uint8_t*)data_;
   retro_time_t  deadline = cpu_features_get_time_usec();

   if (timeout)
      deadline += (retro_time_t)timeout * 1000000;
   else
      deadline += 5000000;

   while (size)
   {
      ssize_t ret = recv(fd, (char*)data, size, 0);

      if (!ret)
         return false;

      if (ret < 0)
      {
         retro_time_t time_delta;
         fd_set fds;
         struct timeval tv;

         if (!isagain((int)ret))
            return false;

         time_delta = deadline - cpu_features_get_time_usec();

         if (time_delta <= 0)
            return false;

         FD_ZERO(&fds);
         FD_SET(fd, &fds);
         tv.tv_sec  = (unsigned)(time_delta / 1000000);
         tv.tv_usec = (unsigned)(time_delta % 1000000);
         if (socket_select(fd + 1, &fds, NULL, NULL, &tv) <= 0)
            return false;
      }
      else
      {
         data += ret;
         size -= ret;
      }
   }

   return true;
}

bool socket_set_block(int fd, bool block)
{
#if !defined(__PSL1GHT__) && defined(__PS3__) || defined(VITA) || defined(WIIU)
   int i = !block;
   setsockopt(fd, SOL_SOCKET, SO_NBIO, &i, sizeof(int));
   return true;
#elif defined(_WIN32)
   u_long mode = !block;
   return ioctlsocket(fd, FIONBIO, &mode) == 0;
#elif defined(GEKKO)
   u32 set = block;
   return net_ioctl(fd, FIONBIO, &set) >= 0;
#else
   return fcntl(fd, F_SETFL, (fcntl(fd, F_GETFL) & ~O_NONBLOCK) | (block ? 0 : O_NONBLOCK)) == 0;
#endif
}

bool socket_nonblock(int fd)
{
   return socket_set_block(fd, false);
}

int socket_close(int fd)
{
#if defined(_WIN32) && !defined(_XBOX360)
   /* WinSock has headers from the stone age. */
   return closesocket(fd);
#elif !defined(__PSL1GHT__) && defined(__PS3__) || defined(WIIU)
   return socketclose(fd);
#elif defined(VITA)
   return sceNetSocketClose(fd);
#else
   return close(fd);
#endif
}

int socket_select(int nfds, fd_set *readfs, fd_set *writefds,
      fd_set *errorfds, struct timeval *timeout)
{
#if !defined(__PSL1GHT__) && defined(__PS3__)
   return socketselect(nfds, readfs, writefds, errorfds, timeout);
#elif defined(VITA)
   extern int retro_epoll_fd;
   SceNetEpollEvent ev = {0};

   ev.events = SCE_NET_EPOLLIN | SCE_NET_EPOLLHUP;
   ev.data.fd = nfds;

   if((sceNetEpollControl(retro_epoll_fd, SCE_NET_EPOLL_CTL_ADD, nfds, &ev)))
   {
      int ret = sceNetEpollWait(retro_epoll_fd, &ev, 1, 0);
      sceNetEpollControl(retro_epoll_fd, SCE_NET_EPOLL_CTL_DEL, nfds, NULL);
      return ret;
   }
   return 0;
#else
   return select(nfds, readfs, writefds, errorfds, timeout);
#endif
}

bool socket_send_all_blocking(int fd, const void *data_, size_t size,
      bool no_signal)
{
   const uint8_t *data = (const uint8_t*)data_;
   int           flags = no_signal ? MSG_NOSIGNAL : 0;

   while (size)
   {
      ssize_t ret = send(fd, (const char*)data, size, flags);

      if (!ret)
         continue;

      if (ret < 0)
      {
         if (!isagain((int)ret))
            return false;
      }
      else
      {
         data += ret;
         size -= ret;
      }
   }

   return true;
}

bool socket_send_all_blocking_with_timeout(int fd,
      const void *data_, size_t size,
      unsigned timeout, bool no_signal)
{
   const uint8_t *data    = (const uint8_t*)data_;
   int           flags    = no_signal ? MSG_NOSIGNAL : 0;
   retro_time_t  deadline = cpu_features_get_time_usec();

   if (timeout)
      deadline += (retro_time_t)timeout * 1000000;
   else
      deadline += 5000000;

   while (size)
   {
      ssize_t ret = send(fd, (const char*)data, size, flags);

      if (!ret)
         continue;

      if (ret < 0)
      {
         retro_time_t time_delta;
         fd_set fds;
         struct timeval tv;

         if (!isagain((int)ret))
            return false;

         time_delta = deadline - cpu_features_get_time_usec();

         if (time_delta <= 0)
            return false;

         FD_ZERO(&fds);
         FD_SET(fd, &fds);
         tv.tv_sec  = (unsigned)(time_delta / 1000000);
         tv.tv_usec = (unsigned)(time_delta % 1000000);
         if (socket_select(fd + 1, NULL, &fds, NULL, &tv) <= 0)
            return false;
      }
      else
      {
         data += ret;
         size -= ret;
      }
   }

   return true;
}

ssize_t socket_send_all_nonblocking(int fd, const void *data_, size_t size,
      bool no_signal)
{
   const uint8_t *data = (const uint8_t*)data_;
   int           flags = no_signal ? MSG_NOSIGNAL : 0;

   while (size)
   {
      ssize_t ret = send(fd, (const char*)data, size, flags);

      if (!ret)
         break;

      if (ret < 0)
      {
         if (isagain((int)ret))
            break;

         return -1;
      }
      else
      {
         data += ret;
         size -= ret;
      }
   }

   return (ssize_t)((size_t)data - (size_t)data_);
}

bool socket_bind(int fd, void *data)
{
   int yes               = 1;
   struct addrinfo *res  = (struct addrinfo*)data;
#ifdef GEKKO
   net_setsockopt(fd, SOL_SOCKET,
         SO_REUSEADDR, (const char*)&yes, sizeof(int));
#else
   setsockopt(fd, SOL_SOCKET,
         SO_REUSEADDR, (const char*)&yes, sizeof(int));
#endif
   if (bind(fd, res->ai_addr, res->ai_addrlen) < 0)
      return false;
   return true;
}

int socket_connect(int fd, void *data, bool timeout_enable)
{
   struct addrinfo *addr = (struct addrinfo*)data;

#if !defined(_WIN32) && !defined(VITA) && !defined(WIIU) && !defined(_3DS) && !defined(GEKKO)
   if (timeout_enable)
   {
      struct timeval timeout;
      timeout.tv_sec  = 4;
      timeout.tv_usec = 0;

      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
   }
#elif defined(GEKKO) && !defined(WIIU)
   if (timeout_enable)
   {
      struct timeval timeout;
      timeout.tv_sec  = 4;
      timeout.tv_usec = 0;

      net_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
   }
#endif

#ifdef WIIU
   {
      int op = 1;

      setsockopt(fd, SOL_SOCKET, SO_WINSCALE, &op, sizeof(op));

      if (addr->ai_socktype == SOCK_STREAM)
      {
         int recvsz = WIIU_RCVBUF;
         int sendsz = WIIU_SNDBUF;

         setsockopt(fd, SOL_SOCKET, SO_TCPSACK, &op, sizeof(op));
         setsockopt(fd, SOL_SOCKET, 0x10000, &op, sizeof(op));
         setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvsz, sizeof(recvsz));
         setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendsz, sizeof(sendsz));
      }
   }
#endif

   return connect(fd, addr->ai_addr, addr->ai_addrlen);
}

bool socket_connect_with_timeout(int fd, void *data, unsigned timeout)
{
   int res;
   struct addrinfo *addr = (struct addrinfo*)data;

   if (!socket_nonblock(fd))
      return false;

#ifdef WIIU
   {
      int op = 1;

      setsockopt(fd, SOL_SOCKET, SO_WINSCALE, &op, sizeof(op));

      if (addr->ai_socktype == SOCK_STREAM)
      {
         int recvsz = WIIU_RCVBUF;
         int sendsz = WIIU_SNDBUF;

         setsockopt(fd, SOL_SOCKET, SO_TCPSACK, &op, sizeof(op));
         setsockopt(fd, SOL_SOCKET, 0x10000, &op, sizeof(op));
         setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvsz, sizeof(recvsz));
         setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendsz, sizeof(sendsz));
      }
   }
#endif

   res = connect(fd, addr->ai_addr, addr->ai_addrlen);
   if (res)
   {
      fd_set wfd, efd;
      struct timeval tv = {0};

      if (!isinprogress(res) && !isagain(res))
         return false;

      FD_ZERO(&wfd);
      FD_ZERO(&efd);
      FD_SET(fd, &wfd);
      FD_SET(fd, &efd);
      tv.tv_sec = timeout ? timeout : 5;
      if (socket_select(fd + 1, NULL, &wfd, &efd, &tv) <= 0)
         return false;
      if (FD_ISSET(fd, &efd))
         return false;
   }

#ifdef SO_ERROR
   {
      int       error = -1;
      socklen_t errsz = sizeof(error);

      getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&error, &errsz);
      if (error)
         return false;
   }
#endif

   return true;
}

static int domain_get(enum socket_domain type)
{
   switch (type)
   {
      case SOCKET_DOMAIN_INET:
#ifdef VITA
         return SCE_NET_AF_INET;
#else
         return AF_INET;
#endif
      default:
         break;
   }

   return 0;
}

int socket_create(
      const char *name,
      enum socket_domain   domain_type,
      enum socket_type     socket_type,
      enum socket_protocol protocol_type)
{
   int type     = 0;
   int protocol = 0;
   int domain   = domain_get(domain_type);
#ifdef VITA

   switch (socket_type)
   {
      case SOCKET_TYPE_DATAGRAM:
         type = SCE_NET_SOCK_DGRAM;
         break;
      case SOCKET_TYPE_STREAM:
         type = SCE_NET_SOCK_STREAM;
         break;
      case SOCKET_TYPE_SEQPACKET:
         /* TODO/FIXME - implement */
         break;
   }

   switch (protocol_type)
   {
      case SOCKET_PROTOCOL_NONE:
         protocol = 0;
         break;
      case SOCKET_PROTOCOL_TCP:
         protocol = SCE_NET_IPPROTO_TCP;
         break;
      case SOCKET_PROTOCOL_UDP:
         protocol = SCE_NET_IPPROTO_UDP;
         break;
   }

   return sceNetSocket(name, domain, type, protocol);
#else
   switch (socket_type)
   {
      case SOCKET_TYPE_DATAGRAM:
         type = SOCK_DGRAM;
         break;
      case SOCKET_TYPE_STREAM:
         type = SOCK_STREAM;
         break;
      case SOCKET_TYPE_SEQPACKET:
         /* TODO/FIXME - implement */
         break;
   }

   switch (protocol_type)
   {
      case SOCKET_PROTOCOL_NONE:
         protocol = 0;
         break;
      case SOCKET_PROTOCOL_TCP:
         protocol = IPPROTO_TCP;
         break;
      case SOCKET_PROTOCOL_UDP:
         protocol = IPPROTO_UDP;
         break;
   }

   return socket(domain, type, protocol);
#endif
}

void socket_set_target(void *data, socket_target_t *in_addr)
{
   struct sockaddr_in *out_target = (struct sockaddr_in*)data;

   out_target->sin_port   = inet_htons(in_addr->port);
   out_target->sin_family = domain_get(in_addr->domain);
#ifdef VITA
   out_target->sin_addr   = inet_aton(in_addr->server);
#else
#ifdef GEKKO
   out_target->sin_len    = 8;
#endif

   inet_ptrton(AF_INET, in_addr->server, &out_target->sin_addr);

#endif
}
