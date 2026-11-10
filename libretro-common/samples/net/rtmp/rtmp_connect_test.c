/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: MIT
 */
#include <net/net_socket.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef CONNECT_FORCE_SELECT
#undef NETWORK_HAVE_POLL
#endif

#ifdef _WIN32
#define CONNECT_PENDING_ERROR WSAEWOULDBLOCK
#define CONNECT_REFUSED_ERROR WSAECONNREFUSED
#else
#define CONNECT_PENDING_ERROR EINPROGRESS
#define CONNECT_REFUSED_ERROR ECONNREFUSED
#endif

static int connect_result, connect_error, ready_result, kernel_error;
static unsigned connects, queries, polls, nonblocks;
static bool nonblock_failed, query_failed, short_query, error_ready;

static void set_error(int value)
{
#ifdef _WIN32
   WSASetLastError(value);
#else
   errno = value;
#endif
}

static int get_error(void)
{
#ifdef _WIN32
   return WSAGetLastError();
#else
   return errno;
#endif
}

static int test_connect(int fd, const struct sockaddr *address, socklen_t size)
{
   assert(fd == 99 && address && size == sizeof(struct sockaddr_in));
   connects++;
   set_error(connect_error);
   return connect_result;
}

static int test_getsockopt(int fd, int level, int option, char *value, socklen_t *size)
{
   assert(fd == 99 && level == SOL_SOCKET && option == SO_ERROR);
   assert(*size == sizeof(int));
   queries++;
   if (query_failed)
      return -1;
   memcpy(value, &kernel_error, sizeof(kernel_error));
   if (short_query)
      --*size;
   return 0;
}

#ifdef _WIN32
static int test_ioctl(int fd, long command, u_long *value)
{
   assert(fd == 99 && (u_long)command == (u_long)FIONBIO && *value == 1);
   nonblocks++;
   return nonblock_failed ? -1 : 0;
}
#define ioctlsocket test_ioctl
#else
static int test_fcntl(int fd, int command, ...)
{
   assert(fd == 99);
   if (command == F_SETFL)
   {
      va_list args;
      int flags;
      va_start(args, command);
      flags = va_arg(args, int);
      va_end(args);
      assert(flags & O_NONBLOCK);
      nonblocks++;
      return nonblock_failed ? -1 : 0;
   }
   assert(command == F_GETFL);
   return 0;
}
#define fcntl test_fcntl
#endif

static int test_select(int nfds, fd_set *readable, fd_set *writable,
      fd_set *errors, struct timeval *timeout)
{
   (void)nfds;
   assert(timeout && !timeout->tv_sec && !timeout->tv_usec);
   assert(writable && errors);
   polls++;
   if (readable)
      FD_ZERO(readable);
   FD_ZERO(writable);
   FD_ZERO(errors);
   if (ready_result > 0)
   {
      if (error_ready)
         FD_SET(99, errors);
      else
         FD_SET(99, writable);
   }
   return ready_result;
}

#if defined(NETWORK_HAVE_POLL) && !defined(_WIN32)
static int test_poll(struct pollfd *fds, nfds_t count, int timeout)
{
   assert(count == 1 && !timeout && fds->fd == 99);
   assert(fds->events == POLLOUT);
   polls++;
   fds->revents = ready_result > 0 ? (error_ready ? POLLERR : POLLOUT) : 0;
   return ready_result;
}
#define poll test_poll
#endif

#define connect test_connect
#define getsockopt test_getsockopt
#define select test_select
#include "../../../net/net_socket.c"

bool network_init(void)
{
   assert(!"connect stage must not initialize networking");
   return false;
}

int getaddrinfo_retro(const char *node, const char *service,
      struct addrinfo *hints, struct addrinfo **result)
{
   (void)node;
   (void)service;
   (void)hints;
   (void)result;
   assert(!"connect stage must not resolve names");
   return -1;
}

retro_time_t cpu_features_get_time_usec(void)
{
   assert(!"connect stage must not own a clock");
   return 0;
}

static void reset(void)
{
   connects = queries = polls = nonblocks = 0;
   connect_result = -1;
   connect_error = CONNECT_PENDING_ERROR;
   ready_result = kernel_error = 0;
   nonblock_failed = query_failed = short_query = error_ready = false;
}

int main(void)
{
   struct sockaddr_in socket_address;
   struct addrinfo address;
   memset(&socket_address, 0, sizeof(socket_address));
   memset(&address, 0, sizeof(address));
   address.ai_addr = (struct sockaddr*)&socket_address;
   address.ai_addrlen = sizeof(socket_address);
   reset();
   assert(socket_connect_begin(99, &address) == SOCKET_CONNECT_PENDING);
   assert(connects == 1 && nonblocks == 1 && !polls && !queries);
   assert(socket_connect_step(99, &address) == SOCKET_CONNECT_PENDING);
   assert(polls == 1 && !queries && connects == 1);
   ready_result = 1;
   assert(socket_connect_step(99, &address) == SOCKET_CONNECT_READY);
   assert(polls == 2 && queries == 1 && connects == 1);
   reset();
   ready_result = 1;
   error_ready = true;
   kernel_error = CONNECT_REFUSED_ERROR;
   assert(socket_connect_step(99, &address) == SOCKET_CONNECT_ERROR);
   assert(queries == 1 && get_error() == CONNECT_REFUSED_ERROR);
   reset();
   ready_result = -1;
   assert(socket_connect_step(99, &address) == SOCKET_CONNECT_ERROR);
   assert(polls == 1 && !queries);
   reset();
   ready_result = 1;
   query_failed = true;
   assert(socket_connect_step(99, &address) == SOCKET_CONNECT_ERROR);
   query_failed = false;
   short_query = true;
   assert(socket_connect_step(99, &address) == SOCKET_CONNECT_ERROR);
   reset();
   connect_result = 0;
   assert(socket_connect_begin(99, &address) == SOCKET_CONNECT_READY);
   assert(connects == 1 && !polls);
   reset();
   connect_error = CONNECT_REFUSED_ERROR;
   assert(socket_connect_begin(99, &address) == SOCKET_CONNECT_ERROR);
   reset();
   nonblock_failed = true;
   assert(socket_connect_begin(99, &address) == SOCKET_CONNECT_ERROR);
   assert(!connects);
   reset();
   assert(socket_connect_begin(-1, &address) == SOCKET_CONNECT_ERROR);
   assert(socket_connect_begin(99, NULL) == SOCKET_CONNECT_ERROR);
   assert(socket_connect_step(-1, &address) == SOCKET_CONNECT_ERROR);
   assert(socket_connect_step(99, NULL) == SOCKET_CONNECT_ERROR);
   assert(!connects && !polls && !nonblocks && !queries);
   puts("PASS incremental TCP connect: one-shot start, zero-time probe and SO_ERROR");
   return 0;
}
