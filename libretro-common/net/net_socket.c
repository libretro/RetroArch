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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#include <compat/msvc.h>
#endif

#include <features/features_cpu.h>

#include <net/net_socket.h>

int socket_init(void **address, uint16_t port, const char *server,
      enum socket_type type, int family)
{
   char port_buf[6];
   struct addrinfo hints      = {0};
   struct addrinfo **addrinfo = (struct addrinfo**)address;
   struct addrinfo *addr      = NULL;

   if (!family)
#if defined(HAVE_SOCKET_LEGACY) || defined(WIIU)
      family = AF_INET;
#else
      family = AF_UNSPEC;
#endif

   hints.ai_family = family;

   switch (type)
   {
      case SOCKET_TYPE_DATAGRAM:
         hints.ai_socktype = SOCK_DGRAM;
         break;
      case SOCKET_TYPE_STREAM:
         hints.ai_socktype = SOCK_STREAM;
         break;
      default:
         return -1;
   }

   if (!server)
      hints.ai_flags = AI_PASSIVE;

   if (!network_init())
      return -1;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   hints.ai_flags |= AI_NUMERICSERV;

   if (getaddrinfo_retro(server, port_buf, &hints, addrinfo))
      return -1;

   addr = *addrinfo;
   if (!addr)
      return -1;

   return socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
}

int socket_next(void **address)
{
   struct addrinfo **addrinfo = (struct addrinfo**)address;
   struct addrinfo *addr      = *addrinfo;

   if ((*addrinfo = addr = addr->ai_next))
      return socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

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
      int timeout)
{
   const uint8_t *data    = (const uint8_t*)data_;
   retro_time_t  deadline = cpu_features_get_time_usec();

   if (timeout > 0)
      deadline += (retro_time_t)timeout * 1000;
   else
      deadline += 5000000;

   while (size)
   {
      ssize_t ret = recv(fd, (char*)data, size, 0);

      if (!ret)
         return false;

      if (ret < 0)
      {
         int _timeout;
         bool ready = true;

         if (!isagain((int)ret))
            return false;

         _timeout = (int)((deadline - cpu_features_get_time_usec()) / 1000);
         if (_timeout <= 0)
            return false;

         if (!socket_wait(fd, &ready, NULL, _timeout) || !ready)
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
#if defined(_WIN32)
   u_long i = !block;

   return !ioctlsocket(fd, FIONBIO, &i);
#elif defined(__PS3__) || defined(VITA) || defined(WIIU)
   int i = !block;

   return !setsockopt(fd, SOL_SOCKET, SO_NBIO, &i, sizeof(i));
#elif defined(GEKKO)
   u32 i = !block;

   return !net_ioctl(fd, FIONBIO, &i);
#else
   int flags = fcntl(fd, F_GETFL);

   if (block)
      flags &= ~O_NONBLOCK;
   else
      flags |= O_NONBLOCK;

   return !fcntl(fd, F_SETFL, flags);
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
#elif defined(__PS3__) || defined(WIIU)
   return socketclose(fd);
#elif defined(VITA)
   return sceNetSocketClose(fd);
#else
   return close(fd);
#endif
}

int socket_select(int nfds, fd_set *readfds, fd_set *writefds,
      fd_set *errorfds, struct timeval *timeout)
{
#if defined(__PS3__)
   return socketselect(nfds, readfds, writefds, errorfds, timeout);
#elif defined(VITA)
   int i, j;
   fd_set rfds, wfds, efds;
   int epoll_fd;
   SceNetEpollEvent *events     = NULL;
   int              event_count = 0;
   int              timeout_us  = -1;
   int              ret         = -1;

   if (nfds < 0 || nfds > 1024)
      return SCE_NET_ERROR_EINVAL;
   if (timeout && (timeout->tv_sec < 0 || timeout->tv_usec < 0))
      return SCE_NET_ERROR_EINVAL;

   epoll_fd = sceNetEpollCreate("socket_select", 0);
   if (epoll_fd < 0)
      return SCE_NET_ERROR_ENOMEM;

   FD_ZERO(&rfds);
   FD_ZERO(&wfds);
   FD_ZERO(&efds);

   for (i = 0; i < nfds; i++)
   {
      if (readfds && FD_ISSET(i, readfds))
         event_count++;
      else if (writefds && FD_ISSET(i, writefds))
         event_count++;
      else if (errorfds && FD_ISSET(i, errorfds))
         event_count++;
   }

#define ALLOC_EVENTS(count) \
   events = (SceNetEpollEvent*)calloc((count), sizeof(*events)); \
   if (!events) \
   { \
      ret = SCE_NET_ERROR_ENOMEM; \
      goto done; \
   }

   if (event_count)
   {
      ALLOC_EVENTS(event_count)

      for (i = 0, j = 0; i < nfds && j < event_count; i++)
      {
         SceNetEpollEvent *event = &events[j];

         if (readfds && FD_ISSET(i, readfds))
            event->events |= SCE_NET_EPOLLIN;
         if (writefds && FD_ISSET(i, writefds))
            event->events |= SCE_NET_EPOLLOUT;

         if (event->events || (errorfds && FD_ISSET(i, errorfds)))
         {
            event->data.fd = i;

            ret = sceNetEpollControl(epoll_fd, SCE_NET_EPOLL_CTL_ADD,
               i, event);
            if (ret < 0)
            {
               switch (ret)
               {
                  case SCE_NET_ERROR_EBADF:
                  case SCE_NET_ERROR_ENOMEM:
                     break;
                  default:
                     ret = SCE_NET_ERROR_EBADF;
                     break;
               }
               goto done;
            }

            j++;
         }
      }

      memset(events, 0, event_count * sizeof(*events));

      /* Keep a copy of the original sets for lookup later. */
      if (readfds)
         memcpy(&rfds, readfds, sizeof(rfds));
      if (writefds)
         memcpy(&wfds, writefds, sizeof(wfds));
      if (errorfds)
         memcpy(&efds, errorfds, sizeof(efds));
   }
   else
   {
      /* Necessary to work with epoll wait. */
      event_count = 1;
      ALLOC_EVENTS(1)
   }

#undef ALLOC_EVENTS

   if (readfds)
      FD_ZERO(readfds);
   if (writefds)
      FD_ZERO(writefds);
   if (errorfds)
      FD_ZERO(errorfds);

   /* Vita's epoll takes a microsecond timeout parameter. */
   if (timeout)
      timeout_us = (int)(timeout->tv_usec + (timeout->tv_sec * 1000000));

   ret = sceNetEpollWait(epoll_fd, events, event_count, timeout_us);
   if (ret <= 0)
      goto done;

#define EPOLL_FD_SET(op, in_set, out_set) \
   if ((event->events & (op)) && FD_ISSET(event->data.fd, (in_set))) \
   { \
      FD_SET(event->data.fd, (out_set)); \
      j++; \
   }

   for (i = 0, j = 0; i < ret; i++)
   {
      SceNetEpollEvent *event = &events[i];

      /* Sanity check */
      if (event->data.fd < 0 || event->data.fd >= nfds)
         continue;

      EPOLL_FD_SET(SCE_NET_EPOLLIN,  &rfds, readfds)
      EPOLL_FD_SET(SCE_NET_EPOLLOUT, &wfds, writefds)
      EPOLL_FD_SET(SCE_NET_EPOLLERR, &efds, errorfds)
   }

   ret = j;

#undef EPOLL_FD_SET

done:
   free(events);
   sceNetEpollDestroy(epoll_fd);

   return ret;
#else
   return select(nfds, readfds, writefds, errorfds, timeout);
#endif
}

#ifdef NETWORK_HAVE_POLL
int socket_poll(struct pollfd *fds, unsigned nfds, int timeout)
{
#if defined(_WIN32)
   return WSAPoll(fds, nfds, timeout);
#elif defined(VITA)
   int i, j;
   int epoll_fd;
   SceNetEpollEvent *events     = NULL;
   int              event_count = (int)nfds;
   int              ret         = -1;

   if (event_count < 0)
      return SCE_NET_ERROR_EINVAL;

   epoll_fd = sceNetEpollCreate("socket_poll", 0);
   if (epoll_fd < 0)
      return SCE_NET_ERROR_ENOMEM;

#define ALLOC_EVENTS(count) \
   events = (SceNetEpollEvent*)calloc((count), sizeof(*events)); \
   if (!events) \
   { \
      ret = SCE_NET_ERROR_ENOMEM; \
      goto done; \
   }

   if (event_count)
   {
      ALLOC_EVENTS(event_count)

      for (i = 0; i < event_count; i++)
      {
         struct pollfd    *fd    = &fds[i];
         SceNetEpollEvent *event = &events[i];

         fd->revents = 0;

         if (fd->fd < 0)
            continue;

         event->events  = fd->events;
         event->data.fd = fd->fd;

         ret = sceNetEpollControl(epoll_fd, SCE_NET_EPOLL_CTL_ADD,
            fd->fd, event);
         if (ret < 0)
            goto done;
      }

      memset(events, 0, event_count * sizeof(*events));
   }
   else
   {
      /* Necessary to work with epoll wait. */
      event_count = 1;
      ALLOC_EVENTS(1)
   }

#undef ALLOC_EVENTS

   /* Vita's epoll takes a microsecond timeout parameter. */
   if (timeout > 0)
      timeout *= 1000;

   ret = sceNetEpollWait(epoll_fd, events, event_count, timeout);
   if (ret <= 0)
      goto done;

   for (i = 0, j = 0; i < ret; i++)
   {
      unsigned k;
      SceNetEpollEvent *event = &events[i];

      /* Sanity check */
      if (event->data.fd < 0)
         continue;

      for (k = 0; k < nfds; k++)
      {
         struct pollfd *fd = &fds[k];

         if (fd->fd == event->data.fd)
         {
            fd->revents = event->events;
            j++;
            break;
         }
      }
   }

   ret = j;

done:
   free(events);
   sceNetEpollDestroy(epoll_fd);

   return ret;
#elif defined(_3DS)
   int i;
   int timeout_quotient;
   int timeout_remainder;
   int ret = -1;

#define TIMEOUT_DIVISOR 100
   if (timeout <= TIMEOUT_DIVISOR)
      return poll(fds, nfds, timeout);

   timeout_quotient = timeout / TIMEOUT_DIVISOR;
   for (i = 0; i < timeout_quotient; i++)
   {
      ret = poll(fds, nfds, TIMEOUT_DIVISOR);

      /* Success or error. */
      if (ret)
         return ret;
   }

   timeout_remainder = timeout % TIMEOUT_DIVISOR;
   if (timeout_remainder)
      ret = poll(fds, nfds, timeout_remainder);

   return ret;
#undef TIMEOUT_DIVISOR

#elif defined(GEKKO)
   return net_poll(fds, nfds, timeout);
#else
   return poll(fds, nfds, timeout);
#endif
}
#endif

bool socket_wait(int fd, bool *rd, bool *wr, int timeout)
{
#ifdef NETWORK_HAVE_POLL
   struct pollfd fds = {0};

   NET_POLL_FD(fd, &fds);

   if (rd && *rd)
   {
      NET_POLL_EVENT(POLLIN, &fds);
      *rd = false;
   }
   if (wr && *wr)
   {
      NET_POLL_EVENT(POLLOUT, &fds);
      *wr = false;
   }

   if (socket_poll(&fds, 1, timeout) < 0)
      return false;

   if (rd && NET_POLL_HAS_EVENT(POLLIN, &fds))
      *rd = true;
   if (wr && NET_POLL_HAS_EVENT(POLLOUT, &fds))
      *wr = true;

   return !NET_POLL_HAS_EVENT((POLLERR | POLLNVAL), &fds);
#else
   fd_set rfd, wfd, efd;
   struct timeval tv, *ptv = NULL;

   FD_ZERO(&rfd);
   FD_ZERO(&wfd);
   FD_ZERO(&efd);

   if (rd && *rd)
   {
      FD_SET(fd, &rfd);
      *rd = false;
   }
   if (wr && *wr)
   {
      FD_SET(fd, &wfd);
      *wr = false;
   }
   FD_SET(fd, &efd);

   if (timeout >= 0)
   {
      tv.tv_sec  = (unsigned)timeout / 1000;
      tv.tv_usec = ((unsigned)timeout % 1000) * 1000;
      ptv = &tv;
   }

   if (socket_select(fd + 1, &rfd, &wfd, &efd, ptv) < 0)
      return false;

   if (rd && FD_ISSET(fd, &rfd))
      *rd = true;
   if (wr && FD_ISSET(fd, &wfd))
      *wr = true;

   return !FD_ISSET(fd, &efd);
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
      int timeout, bool no_signal)
{
   const uint8_t *data    = (const uint8_t*)data_;
   int           flags    = no_signal ? MSG_NOSIGNAL : 0;
   retro_time_t  deadline = cpu_features_get_time_usec();

   if (timeout > 0)
      deadline += (retro_time_t)timeout * 1000;
   else
      deadline += 5000000;

   while (size)
   {
      ssize_t ret = send(fd, (const char*)data, size, flags);

      if (!ret)
         continue;

      if (ret < 0)
      {
         int _timeout;
         bool ready = true;

         if (!isagain((int)ret))
            return false;

         _timeout = (int)((deadline - cpu_features_get_time_usec()) / 1000);
         if (_timeout <= 0)
            return false;

         if (!socket_wait(fd, NULL, &ready, _timeout) || !ready)
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
   struct addrinfo *addr = (struct addrinfo*)data;

   {
      int on = 1;

      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
         (char*)&on, sizeof(on));
   }

   return !bind(fd, addr->ai_addr, addr->ai_addrlen);
}

int socket_connect(int fd, void *data)
{
   struct addrinfo *addr = (struct addrinfo*)data;

#ifdef WIIU
   {
      int op = 1;

      setsockopt(fd, SOL_SOCKET, SO_WINSCALE, &op, sizeof(op));

      if (addr->ai_socktype == SOCK_STREAM)
      {
         int recvsz = WIIU_RCVBUF;
         int sendsz = WIIU_SNDBUF;

         setsockopt(fd, SOL_SOCKET, SO_TCPSACK, &op, sizeof(op));
         setsockopt(fd, SOL_SOCKET, SO_RUSRBUF, &op, sizeof(op));
         setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvsz, sizeof(recvsz));
         setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendsz, sizeof(sendsz));
      }
   }
#endif

   return connect(fd, addr->ai_addr, addr->ai_addrlen);
}

bool socket_connect_with_timeout(int fd, void *data, int timeout)
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
         setsockopt(fd, SOL_SOCKET, SO_RUSRBUF, &op, sizeof(op));
         setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvsz, sizeof(recvsz));
         setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendsz, sizeof(sendsz));
      }
   }
#endif

   res = connect(fd, addr->ai_addr, addr->ai_addrlen);
   if (res)
   {
      bool ready = true;

      if (!isinprogress(res) && !isagain(res))
         return false;

      if (timeout <= 0)
         timeout = 5000;

      if (!socket_wait(fd, NULL, &ready, timeout) || !ready)
         return false;
   }

#if defined(GEKKO)
   /* libogc does not have getsockopt implemented */
   res = connect(fd, addr->ai_addr, addr->ai_addrlen);
   if (res < 0 && -res != EISCONN)
      return false;
#elif defined(_3DS)
   /* libctru getsockopt does not return expected value */
   if ((connect(fd, addr->ai_addr, addr->ai_addrlen) < 0) && errno != EISCONN)
      return false;
#elif defined(WIIU)
   /* On WiiU, getsockopt() returns -1 and sets lastsocketerr() (Wii's
    * equivalent to errno) to 16. */
   if ((connect(fd, addr->ai_addr, addr->ai_addrlen) == -1)
         && socketlasterr() != SO_EISCONN)
      return false;
#else
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

int socket_create(
      const char *name,
      enum socket_domain   domain_type,
      enum socket_type     socket_type,
      enum socket_protocol protocol_type)
{
   int domain   = 0;
   int type     = 0;
   int protocol = 0;

   switch (domain_type)
   {
      case SOCKET_DOMAIN_INET:
         domain = AF_INET;
         break;
      default:
         break;
   }

   switch (socket_type)
   {
      case SOCKET_TYPE_DATAGRAM:
         type = SOCK_DGRAM;
         break;
      case SOCKET_TYPE_STREAM:
         type = SOCK_STREAM;
         break;
      case SOCKET_TYPE_SEQPACKET:
      default:
         /* TODO/FIXME - implement */
         break;
   }

   switch (protocol_type)
   {
      case SOCKET_PROTOCOL_TCP:
         protocol = IPPROTO_TCP;
         break;
      case SOCKET_PROTOCOL_UDP:
         protocol = IPPROTO_UDP;
         break;
      case SOCKET_PROTOCOL_NONE:
      default:
         break;
   }

#ifdef VITA
   return sceNetSocket(name, domain, type, protocol);
#else
   return socket(domain, type, protocol);
#endif
}

void socket_set_target(void *data, socket_target_t *in_addr)
{
   struct sockaddr_in *out_target = (struct sockaddr_in*)data;

#ifdef GEKKO
   out_target->sin_len          = 8;
#endif
   switch (in_addr->domain)
   {
      case SOCKET_DOMAIN_INET:
         out_target->sin_family = AF_INET;
         break;
      default:
         out_target->sin_family = 0;
         break;
   }
   out_target->sin_port         = htons(in_addr->port);
   inet_pton(AF_INET, in_addr->server, &out_target->sin_addr);
}
