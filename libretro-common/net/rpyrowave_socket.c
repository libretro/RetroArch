/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/rpyrowave_socket.h>
#include <stdlib.h>
#include <limits.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET rpyrowave_native_socket_t;
#define RPYROWAVE_SOCKET_INVALID INVALID_SOCKET
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
typedef int rpyrowave_native_socket_t;
#define RPYROWAVE_SOCKET_INVALID (-1)
#endif

struct rpyrowave_socket
{
   rpyrowave_native_socket_t handle;
};

struct rpyrowave_endpoint
{
   rpyrowave_socket_t *prepared;
   rpyrowave_native_socket_t handle;
   unsigned listening;
   int state;
};

static void rpyrowave_socket_close(rpyrowave_native_socket_t handle)
{
   if (handle == RPYROWAVE_SOCKET_INVALID) return;
#if defined(_WIN32)
   closesocket(handle);
#else
   close(handle);
#endif
}

static int rpyrowave_socket_pending(void)
{
#if defined(_WIN32)
   int error = WSAGetLastError();
   return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
   return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

static int rpyrowave_socket_read(void *user, void *data, size_t bytes)
{
   rpyrowave_socket_t *s = (rpyrowave_socket_t*)user;
   int received;
   if (!bytes) return 0;
   if (bytes > INT_MAX) bytes = INT_MAX;
   received = (int)recv(s->handle, (char*)data, (int)bytes, 0);
   if (received > 0) return received;
   if (!received) return -2;
   return rpyrowave_socket_pending() ? 0 : -1;
}

static int rpyrowave_socket_write(void *user, const void *data, size_t bytes)
{
   rpyrowave_socket_t *s = (rpyrowave_socket_t*)user;
   int sent;
   int flags = 0;
   if (!bytes) return 0;
   if (bytes > INT_MAX) bytes = INT_MAX;
#if defined(MSG_NOSIGNAL)
   flags = MSG_NOSIGNAL;
#endif
   sent = (int)send(s->handle, (const char*)data, (int)bytes, flags);
   if (sent > 0) return sent;
   if (!sent) return -1;
   return rpyrowave_socket_pending() ? 0 : -1;
}

static int rpyrowave_socket_configure(rpyrowave_native_socket_t descriptor)
{
   int enabled = 1;
   struct linger linger_options;
#if defined(_WIN32)
   u_long nonblocking = 1;
#else
   int flags;
#endif
#if defined(_WIN32)
   if (ioctlsocket(descriptor, FIONBIO, &nonblocking)) return 0;
#else
   flags = fcntl(descriptor, F_GETFL, 0);
   if (flags < 0 || fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) < 0) return 0;
#endif
   if (setsockopt(descriptor, IPPROTO_TCP, TCP_NODELAY, (const char*)&enabled, sizeof(enabled))) return 0;
   linger_options.l_onoff = 0;
   linger_options.l_linger = 0;
   if (setsockopt(descriptor, SOL_SOCKET, SO_LINGER, (const char*)&linger_options, sizeof(linger_options))) return 0;
#if !defined(_WIN32) && defined(SO_NOSIGPIPE)
   if (setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&enabled, sizeof(enabled))) return 0;
#elif !defined(_WIN32) && !defined(MSG_NOSIGNAL)
   return 0;
#endif
   return 1;
}

rpyrowave_socket_t *rpyrowave_socket_new(uintptr_t handle)
{
   rpyrowave_socket_t *s;
#if defined(_WIN32)
   if (handle == (uintptr_t)INVALID_SOCKET) return NULL;
#else
   if (handle > INT_MAX) return NULL;
#endif
   s = (rpyrowave_socket_t*)malloc(sizeof(*s));
   if (!s) return NULL;
   s->handle = (rpyrowave_native_socket_t)handle;
   if (!rpyrowave_socket_configure(s->handle))
   {
      free(s);
      return NULL;
   }
   return s;
}

void rpyrowave_socket_io(rpyrowave_socket_t *s, rpyrowave_stream_io_t *io)
{
   if (!io) return;
   io->user = s;
   io->read = s ? rpyrowave_socket_read : NULL;
   io->write = s ? rpyrowave_socket_write : NULL;
}

uintptr_t rpyrowave_socket_handle(const rpyrowave_socket_t *s)
{
   return s ? (uintptr_t)s->handle : (uintptr_t)-1;
}

void rpyrowave_socket_free(rpyrowave_socket_t *s)
{
   if (!s) return;
   rpyrowave_socket_close(s->handle);
   free(s);
}

static int rpyrowave_endpoint_connect_pending(void)
{
#if defined(_WIN32)
   int error = WSAGetLastError();
   return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINTR;
#else
   return errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

rpyrowave_endpoint_t *rpyrowave_endpoint_new(const void *address,
      size_t address_bytes, unsigned listening)
{
   const struct sockaddr *native = (const struct sockaddr*)address;
   rpyrowave_endpoint_t *p;
   size_t required;
   if (!address || address_bytes < sizeof(struct sockaddr) || listening > 1) return NULL;
   if (native->sa_family == AF_INET) required = sizeof(struct sockaddr_in);
   else if (native->sa_family == AF_INET6) required = sizeof(struct sockaddr_in6);
   else return NULL;
   if (address_bytes < required) return NULL;
   p = (rpyrowave_endpoint_t*)calloc(1, sizeof(*p));
   if (!p) return NULL;
   p->handle = RPYROWAVE_SOCKET_INVALID;
   p->prepared = (rpyrowave_socket_t*)malloc(sizeof(*p->prepared));
   if (!p->prepared) goto error;
   p->prepared->handle = RPYROWAVE_SOCKET_INVALID;
   p->listening = listening;
   p->handle = socket(native->sa_family, SOCK_STREAM, IPPROTO_TCP);
   if (p->handle == RPYROWAVE_SOCKET_INVALID || !rpyrowave_socket_configure(p->handle)) goto error;
   if (listening)
   {
      if (bind(p->handle, native, (int)required) || listen(p->handle, 1)) goto error;
   }
   else if (!connect(p->handle, native, (int)required)) p->state = RPYROWAVE_ENDPOINT_READY;
   else if (!rpyrowave_endpoint_connect_pending()) goto error;
   return p;
error:
   rpyrowave_endpoint_free(p);
   return NULL;
}

int rpyrowave_endpoint_step(rpyrowave_endpoint_t *p, unsigned readiness,
      rpyrowave_socket_t **connected)
{
   if (connected) *connected = NULL;
   if (!p || !connected || !p->prepared || p->state == RPYROWAVE_ENDPOINT_ERROR
         || readiness & ~7U) return RPYROWAVE_ENDPOINT_ERROR;
   if (p->state != RPYROWAVE_ENDPOINT_READY)
   {
      if (p->listening)
      {
         rpyrowave_native_socket_t accepted;
         if (readiness & RPYROWAVE_ENDPOINT_EXCEPTION) goto error;
         if (!(readiness & RPYROWAVE_ENDPOINT_READABLE)) return RPYROWAVE_ENDPOINT_PENDING;
         accepted = accept(p->handle, NULL, NULL);
         if (accepted == RPYROWAVE_SOCKET_INVALID)
         {
            if (rpyrowave_socket_pending()) return RPYROWAVE_ENDPOINT_PENDING;
            goto error;
         }
         if (!rpyrowave_socket_configure(accepted))
         {
            rpyrowave_socket_close(accepted);
            goto error;
         }
         rpyrowave_socket_close(p->handle);
         p->handle = accepted;
      }
      else
      {
         int error = -1;
#if defined(_WIN32)
         int bytes = sizeof(error);
#else
         socklen_t bytes = sizeof(error);
#endif
         if (!(readiness & (RPYROWAVE_ENDPOINT_WRITABLE | RPYROWAVE_ENDPOINT_EXCEPTION)))
            return RPYROWAVE_ENDPOINT_PENDING;
         if (getsockopt(p->handle, SOL_SOCKET, SO_ERROR, (char*)&error, &bytes)
               || bytes != sizeof(error) || error) goto error;
      }
   }
   p->prepared->handle = p->handle;
   *connected = p->prepared;
   p->prepared = NULL;
   p->handle = RPYROWAVE_SOCKET_INVALID;
   p->state = RPYROWAVE_ENDPOINT_READY;
   return RPYROWAVE_ENDPOINT_READY;
error:
   p->state = RPYROWAVE_ENDPOINT_ERROR;
   rpyrowave_socket_close(p->handle);
   p->handle = RPYROWAVE_SOCKET_INVALID;
   return RPYROWAVE_ENDPOINT_ERROR;
}

uintptr_t rpyrowave_endpoint_handle(const rpyrowave_endpoint_t *p)
{
   return p ? (uintptr_t)p->handle : (uintptr_t)-1;
}

void rpyrowave_endpoint_free(rpyrowave_endpoint_t *p)
{
   if (!p) return;
   rpyrowave_socket_close(p->handle);
   rpyrowave_socket_free(p->prepared);
   free(p);
}
