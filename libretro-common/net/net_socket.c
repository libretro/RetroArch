/* Copyright  (C) 2010-2016 The RetroArch team
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
#include <net/net_compat.h>
#include <net/net_socket.h>

int socket_init(void **address, uint16_t port, const char *server, enum socket_type type)
{
   char port_buf[16]     = {0};
   struct addrinfo hints = {0};
   struct addrinfo **addrinfo = (struct addrinfo**)address;
   struct addrinfo *addr = NULL;
   

   if (!network_init())
      goto error;

#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family = AF_INET;
#else
   hints.ai_family = AF_UNSPEC;
#endif

   switch (type)
   {
      case SOCKET_TYPE_DATAGRAM:
         hints.ai_socktype = SOCK_DGRAM;
         break;
      case SOCKET_TYPE_STREAM:
         hints.ai_socktype = SOCK_STREAM;
         break;
   }

   if (!server)
      hints.ai_flags = AI_PASSIVE;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);

   if (getaddrinfo_retro(server, port_buf, &hints, addrinfo) < 0)
      goto error;

   addr = (struct addrinfo*)*addrinfo;

   if (!addr)
      goto error;

   return socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);

error:
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
#if defined(__CELLOS_LV2__) || defined(VITA)
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
#elif defined(__CELLOS_LV2__)
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
   SceNetEpollEvent ev = {0};

   ev.events = PSP2_NET_EPOLLIN | PSP2_NET_EPOLLHUP;
   ev.data.fd = nfds;

   if((sceNetEpollControl(retro_epoll_fd, PSP2_NET_EPOLL_CTL_ADD, nfds, &ev)))
   {
      int ret = sceNetEpollWait(retro_epoll_fd, &ev, 1, 0);
      sceNetEpollControl(retro_epoll_fd, PSP2_NET_EPOLL_CTL_DEL, nfds, NULL);
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
         if (!isagain(ret))
            continue;

         return false;
      }

      data += ret;
      size -= ret;
   }

   return true;
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

#ifndef _WIN32
#ifndef VITA
   if (timeout_enable)
   {
      struct timeval timeout;
      timeout.tv_sec  = 4;
      timeout.tv_usec = 0;

      setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof timeout);
   }
#endif
#endif

   return connect(fd, addr->ai_addr, addr->ai_addrlen);
}
