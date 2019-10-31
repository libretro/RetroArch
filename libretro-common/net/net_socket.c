/* Copyright  (C) 2010-2018 The RetroArch team
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

#include <net/net_compat.h>
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
   const uint8_t *data = (const uint8_t*)data_;
   ssize_t         ret = recv(fd, (char*)data, size, 0);

   if (ret > 0)
      return ret;

   if (ret == 0)
   {
      /* Socket closed */
      *error = true;
      return -1;
   }

   if (isagain((int)ret))
      return 0;

   *error = true;
   return -1;
}

int socket_receive_all_blocking(int fd, void *data_, size_t size)
{
   const uint8_t *data = (const uint8_t*)data_;

   while (size)
   {
      ssize_t ret = recv(fd, (char*)data, size, 0);
      if (ret <= 0)
         return false;

      data += ret;
      size -= ret;
   }

   return true;
}

bool socket_nonblock(int fd)
{
#if defined(__CELLOS_LV2__) || defined(VITA) || defined(WIIU)
   int i = 1;
   setsockopt(fd, SOL_SOCKET, SO_NBIO, &i, sizeof(int));
   return true;
#elif defined(_WIN32)
   u_long mode = 1;
   return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
   return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == 0;
#endif
}

int socket_close(int fd)
{
#if defined(_WIN32) && !defined(_XBOX360)
   /* WinSock has headers from the stone age. */
   return closesocket(fd);
#elif defined(__CELLOS_LV2__) || defined(WIIU)
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
#if defined(__CELLOS_LV2__)
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

int socket_send_all_blocking(int fd, const void *data_, size_t size,
      bool no_signal)
{
   const uint8_t *data = (const uint8_t*)data_;

   while (size)
   {
      ssize_t ret = send(fd, (const char*)data, size,
            no_signal ? MSG_NOSIGNAL : 0);
      if (ret <= 0)
      {
         if (isagain((int)ret))
            continue;

         return false;
      }

      data += ret;
      size -= ret;
   }

   return true;
}

ssize_t socket_send_all_nonblocking(int fd, const void *data_, size_t size,
      bool no_signal)
{
   const uint8_t *data = (const uint8_t*)data_;
   ssize_t sent = 0;

   while (size)
   {
      ssize_t ret = send(fd, (const char*)data, size,
            no_signal ? MSG_NOSIGNAL : 0);
      if (ret < 0)
      {
         if (isagain((int)ret))
            break;

         return -1;
      }
      else if (ret == 0)
         break;

      data += ret;
      size -= ret;
      sent += ret;
   }

   return sent;
}

bool socket_bind(int fd, void *data)
{
   int yes               = 1;
   struct addrinfo *res  = (struct addrinfo*)data;
   setsockopt(fd, SOL_SOCKET,
         SO_REUSEADDR, (const char*)&yes, sizeof(int));
   if (bind(fd, res->ai_addr, res->ai_addrlen) < 0)
      return false;
   return true;
}

int socket_connect(int fd, void *data, bool timeout_enable)
{
   struct addrinfo *addr = (struct addrinfo*)data;

#if !defined(_WIN32) && !defined(VITA) && !defined(WIIU) && !defined(_3DS)
   if (timeout_enable)
   {
      struct timeval timeout;
      timeout.tv_sec  = 4;
      timeout.tv_usec = 0;

      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof timeout);
   }
#endif

   return connect(fd, addr->ai_addr, addr->ai_addrlen);
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
