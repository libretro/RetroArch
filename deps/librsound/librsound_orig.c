/*  RSound - A PCM audio client/server
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 * 
 *  RSound is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RSound is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RSound.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#define RSD_EXPOSE_STRUCT
#define RSD_DLL_EXPORT
#include "rsound.h"
#include "../../fifo_buffer.h"

#undef CONST_CAST

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#undef close
#define close(x) closesocket(x)
#define CONST_CAST (const char*)

#else

#define CONST_CAST

#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <poll.h>
#include <time.h>
#include <errno.h> 

// DECnet
#ifndef _WIN32
#ifdef HAVE_DECNET
#include <netdnet/dn.h>
#include <netdnet/dnetdb.h>
#endif
#endif

/* 
 ****************************************************************************   
 Naming convention. Functions for use in API are called rsd_*(),         *
 internal function are called rsnd_*()                                   *
 ****************************************************************************
 */

// Internal enumerations
enum rsd_logtype
{
   RSD_LOG_DEBUG = 0,
   RSD_LOG_WARN,
   RSD_LOG_ERR
};

enum rsd_conn_type
{
   RSD_CONN_TCP = 0x0000,
   RSD_CONN_UNIX = 0x0001,
   RSD_CONN_DECNET = 0x0002,

   RSD_CONN_PROTO = 0x100
};

// Some logging macros.
static void rsnd_log(enum rsd_logtype type, const char *fmt, ...); 
#ifdef DEBUG
#define RSD_DEBUG(fmt, args...) rsnd_log(RSD_LOG_DEBUG, "(%s:%d): " fmt , __FILE__,  __LINE__ , ##args)
#else
#define RSD_DEBUG(fmt, args...) {}
#endif

#define RSD_WARN(fmt, args...) rsnd_log(RSD_LOG_WARN, "(%s:%d): " fmt , __FILE__, __LINE__ , ##args)
#define RSD_ERR(fmt, args...) rsnd_log(RSD_LOG_ERR, "(%s:%d): " fmt , __FILE__, __LINE__ , ##args)

static inline int rsnd_is_little_endian(void);
static inline void rsnd_swap_endian_16(uint16_t * x);
static inline void rsnd_swap_endian_32(uint32_t * x);
static inline int rsnd_format_to_samplesize(enum rsd_format fmt);
static int rsnd_connect_server(rsound_t *rd);
static int rsnd_send_header_info(rsound_t *rd);
static int rsnd_get_backend_info(rsound_t *rd);
static int rsnd_create_connection(rsound_t *rd);
static int rsnd_connect_socket(int fd, const struct sockaddr *addr, socklen_t addr_len);
static ssize_t rsnd_send_chunk(int socket, const void *buf, size_t size, int blocking);
static ssize_t rsnd_recv_chunk(int socket, void *buf, size_t size, int blocking);
static int rsnd_start_thread(rsound_t *rd);
static int rsnd_stop_thread(rsound_t *rd);
static size_t rsnd_get_delay(rsound_t *rd);
static size_t rsnd_get_ptr(rsound_t *rd);
static int rsnd_reset(rsound_t *rd);

// Protocol functions
static int rsnd_send_identity_info(rsound_t *rd);
static int rsnd_close_ctl(rsound_t *rd);
static int rsnd_send_info_query(rsound_t *rd);
static int rsnd_update_server_info(rsound_t *rd);

static int rsnd_poll(struct pollfd *fd, int numfd, int timeout);
static void rsnd_sleep(int msec);

static void* rsnd_cb_thread(void *thread_data);
static void* rsnd_thread(void *thread_data);

#ifdef _WIN32
// Stupid winsock
static int init_count = 0;

static int init_wsock(void)
{
   WSADATA wsaData;
   int retval;

   if ((retval = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0)
   {
      RSD_ERR("Could not start Winsock\n");
      WSACleanup();
      return -1;
   }
   return 0;
}
#endif


// Does some logging
static void rsnd_log(enum rsd_logtype type, const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);

   const char *logtype;
   switch (type)
   {
      case RSD_LOG_DEBUG:
         logtype = "DEBUG";
         break;
      case RSD_LOG_WARN:
         logtype = "WARN";
         break;
      case RSD_LOG_ERR:
         logtype = "ERROR";
         break;
      default:
         logtype = "";
         break;
   }

   char buf[1024];
   vsnprintf(buf, sizeof(buf), fmt, args);
   buf[1023] = '\0';
   va_end(args);

   // Currently only uses stderr. TODO: Make it more generic.
   fprintf(stderr, "(librsound): PID: %d: [%s] %s\n", (int)getpid(), logtype, buf);
}

/* Determine whether we're running big- or little endian */
static inline int rsnd_is_little_endian(void)
{
   uint16_t i = 1;
   return *((uint8_t*)&i);
}

/* Simple functions for swapping bytes */
static inline void rsnd_swap_endian_16(uint16_t * x)
{
   *x = (*x>>8) | (*x<<8);
}

static inline void rsnd_swap_endian_32(uint32_t * x)
{
   *x =  (*x >> 24 ) |
      ((*x<<8) & 0x00FF0000) |
      ((*x>>8) & 0x0000FF00) |
      (*x << 24);
}

static inline int rsnd_format_to_samplesize(enum rsd_format fmt)
{
   switch(fmt)
   {
      case RSD_S32_LE:
      case RSD_S32_BE:
      case RSD_S32_NE:
      case RSD_U32_LE:
      case RSD_U32_BE:
      case RSD_U32_NE:
         return 4;

      case RSD_S16_LE:
      case RSD_U16_LE:
      case RSD_S16_BE:
      case RSD_U16_BE:
      case RSD_S16_NE:
      case RSD_U16_NE:
         return 2;

      case RSD_U8:
      case RSD_S8:
      case RSD_ALAW:
      case RSD_MULAW:
         return 1;

      default:
         return -1;
   }
}

#define RSD_API_DECL
#define RSD_API_CALLTYPE

RSD_API_DECL int RSD_API_CALLTYPE rsd_samplesize(rsound_t *rd)
{
   assert(rd != NULL);
   return rd->samplesize;
}

/* Uses non-blocking IO since it performed more deterministic with poll()/send() */   
#ifdef _WIN32 // Yes, Win32 is a bitch.
static int rsnd_connect_socket(int fd, const struct sockaddr *addr, socklen_t addr_len)
{
   if (connect(fd, addr, addr_len) < 0)
      return -1;

   u_long iMode = 1;
   ioctlsocket(fd, FIONBIO, &iMode);
   return 0;
}
#else
static int rsnd_connect_socket(int fd, const struct sockaddr *addr, socklen_t addr_len)
{
#ifndef __CYGWIN__
   if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
   {
      RSD_ERR("Couldn't set socket to non-blocking ...");
      return -1;
   }
#endif /* Cygwin doesn't seem to like non-blocking I/O ... */

   if (connect(fd, addr, addr_len) < 0 && errno != EINPROGRESS)
      return -1;

   struct pollfd poll_fd = {
      .fd = fd,
      .events = POLLOUT
   };

   rsnd_poll(&poll_fd, 1, 3000);
   if (!(poll_fd.revents & POLLOUT))
      return -1;

   return 0;
}
#endif

/* Creates sockets and attempts to connect to the server. Returns -1 when failed, and 0 when success. */
static int rsnd_connect_server( rsound_t *rd )
{
   RSD_DEBUG("rsnd_connect_server");
   struct addrinfo hints, *res = NULL;
#ifndef _WIN32
   struct sockaddr_un un;
#ifdef HAVE_DECNET
   char * object;
   char * delm;
#endif
#endif

   memset(&hints, 0, sizeof( hints ));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;

#ifndef _WIN32
   if (rd->host[0] == '/')
   {
      rd->conn_type = RSD_CONN_UNIX;
      res = &hints;
      res->ai_family = AF_UNIX;
      res->ai_protocol = 0;
      res->ai_addr = (struct sockaddr*)&un;
      res->ai_addrlen = sizeof(un);
      un.sun_family = res->ai_family;
      strncpy(un.sun_path, rd->host, sizeof(un.sun_path)); 
      un.sun_path[sizeof(un.sun_path)-1] = '\0';
   }
#ifdef HAVE_DECNET
   else if ((delm = strstr(rd->host, "::")) != NULL)
   {
      rd->conn_type = RSD_CONN_DECNET;
      object = delm;

      if ( object[2] == 0 ) /* We have no object info, use default object name */
         object = RSD_DEFAULT_OBJECT;
      else
         object += 2; /* jump over the delm. */

      *delm = 0;

      rd->conn.socket = dnet_conn(rd->host, object, SOCK_STREAM, NULL, 0, NULL, 0);
      rd->conn.ctl_socket = dnet_conn(rd->host, object, SOCK_STREAM, NULL, 0, NULL, 0);

      *delm = ':';

      if ( rd->conn.socket <= 0 || rd->conn.ctl_socket <= 0 )
         return -1;

      /* TODO: set nonblocking mode here */
      return 0;
   }
#endif
   else
#endif
   {
      rd->conn_type = RSD_CONN_TCP;
      if (getaddrinfo(rd->host, rd->port, &hints, &res) != 0)
         goto error;
   }

   rd->conn.socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   rd->conn.ctl_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   if (rd->conn.socket < 0 || rd->conn.ctl_socket < 0)
   {
      RSD_ERR("Getting sockets failed.");
      goto error;
   }

   if (rsnd_connect_socket(rd->conn.socket, res->ai_addr, res->ai_addrlen) < 0)
      goto error;
   if (rsnd_connect_socket(rd->conn.ctl_socket, res->ai_addr, res->ai_addrlen) < 0)
      goto error;

   if (res != NULL && (res->ai_family != AF_UNIX))
      freeaddrinfo(res);

   RSD_DEBUG("rsnd_connect_server completeted!");
   return 0;

   /* Cleanup for errors. */
error:
   RSD_ERR("Connecting to server failed. \"%s\"", rd->host);

   if (res != NULL && (res->ai_family != AF_UNIX))
      freeaddrinfo(res);
   return -1;
}

/* Conjures a WAV-header and sends this to server. Returns -1 when failed, and 0 when success. */
static int rsnd_send_header_info(rsound_t *rd)
{
   /* Defines the size of a wave header */
#define HEADER_SIZE 44
   char *header = calloc(1, HEADER_SIZE);
   if (header == NULL)
   {
      RSD_ERR("Could not allocate memory.");
      return -1;
   }
   uint16_t temp16;
   uint32_t temp32;

   /* These magic numbers represent the position of the elements in the wave header. 
      We can't simply send a wave struct over the network since the compiler is allowed to
      pad our structs as they like, so sizeof(waveheader) might not be similar on two different
      systems. */

#define RATE 24
#define CHANNEL 22
#define FRAMESIZE 34
#define FORMAT 42


   uint32_t temp_rate = rd->rate;
   uint16_t temp_channels = rd->channels;

   uint16_t temp_bits = 8 * rsnd_format_to_samplesize(rd->format);
   uint16_t temp_format = rd->format;

   // Checks the format for native endian which will need to be set properly.
   switch (temp_format)
   {
      case RSD_S16_NE:
         if (rsnd_is_little_endian())
            temp_format = RSD_S16_LE;
         else
            temp_format = RSD_S16_BE;
         break;

      case RSD_U16_NE:
         if (rsnd_is_little_endian())
            temp_format = RSD_U16_LE;
         else
            temp_format = RSD_U16_BE;
         break;
      case RSD_S32_NE:
         if (rsnd_is_little_endian())
            temp_format = RSD_S32_LE;
         else
            temp_format = RSD_S32_BE;
         break;
      case RSD_U32_NE:
         if (rsnd_is_little_endian())
            temp_format = RSD_U32_LE;
         else
            temp_format = RSD_U32_BE;
         break;

      default:
         break;
   }


   /* Since the values in the wave header we are interested in, are little endian (>_<), we need
      to determine whether we're running it or not, so we can byte swap accordingly. 
      Could determine this compile time, but it was simpler to do it this way. */

   // Fancy macros for embedding little endian values into the header.
#define SET32(buf,offset,x) (*((uint32_t*)(buf+offset)) = x)
#define SET16(buf,offset,x) (*((uint16_t*)(buf+offset)) = x)

#define LSB16(x) if ( !rsnd_is_little_endian() ) { rsnd_swap_endian_16(&(x)); }
#define LSB32(x) if ( !rsnd_is_little_endian() ) { rsnd_swap_endian_32(&(x)); }

   // Here we embed in the rest of the WAV header for it to be somewhat valid

   strcpy(header, "RIFF");
   SET32(header, 4, 0);
   strcpy(header+8, "WAVE");
   strcpy(header+12, "fmt ");

   temp32 = 16;
   LSB32(temp32);
   SET32(header, 16, temp32);

   temp16 = 0; // PCM data

   switch(rd->format)
   {
      case RSD_S16_LE:
      case RSD_U8:
         temp16 = 1;
         break;

      case RSD_ALAW:
         temp16 = 6;
         break;

      case RSD_MULAW:
         temp16 = 7;
         break;
   }

   LSB16(temp16);
   SET16(header, 20, temp16);

   // Channels here
   LSB16(temp_channels);
   SET16(header, CHANNEL, temp_channels);
   // Samples per sec
   LSB32(temp_rate);
   SET32(header, RATE, temp_rate);

   temp32 = rd->rate * rd->channels * rsnd_format_to_samplesize(rd->format);
   LSB32(temp32);
   SET32(header, 28, temp32);

   temp16 = rd->channels * rsnd_format_to_samplesize(rd->format);
   LSB16(temp16);
   SET16(header, 32, temp16);

   // Bits per sample
   LSB16(temp_bits);
   SET16(header, FRAMESIZE, temp_bits);

   strcpy(header+36, "data");

   // Do not care about cksize here (impossible to know beforehand). It is used by
   // the server for format.

   LSB16(temp_format);
   SET16(header, FORMAT, temp_format);

   // End static header

   if (rsnd_send_chunk(rd->conn.socket, header, HEADER_SIZE, 1) != HEADER_SIZE)
   {
      free(header);
      return -1;
   }

   free(header);
   return 0;
}

/* Recieves backend info from server that is of interest to the client. (This mini-protocol might be extended later on.) */
static int rsnd_get_backend_info ( rsound_t *rd )
{
#define RSND_HEADER_SIZE 8
#define LATENCY 0
#define CHUNKSIZE 1

   // Header is 2 uint32_t's. = 8 bytes.
   uint32_t rsnd_header[2] = {0};

   if (rsnd_recv_chunk(rd->conn.socket, rsnd_header, RSND_HEADER_SIZE, 1) != RSND_HEADER_SIZE)
   {
      RSD_ERR("Couldn't receive chunk.");
      return -1;
   }

   /* Again, we can't be 100% certain that sizeof(backend_info_t) is equal on every system */

   if (rsnd_is_little_endian())
   {
      rsnd_swap_endian_32(&rsnd_header[LATENCY]);
      rsnd_swap_endian_32(&rsnd_header[CHUNKSIZE]);
   }

   rd->backend_info.latency = rsnd_header[LATENCY];
   rd->backend_info.chunk_size = rsnd_header[CHUNKSIZE];

#define MAX_CHUNK_SIZE 1024 // We do not want larger chunk sizes than this.
   if (rd->backend_info.chunk_size > MAX_CHUNK_SIZE || rd->backend_info.chunk_size <= 0)
      rd->backend_info.chunk_size = MAX_CHUNK_SIZE;

   /* Assumes a default buffer size should it cause problems of being too small */
   if (rd->buffer_size <= 0 || rd->buffer_size < rd->backend_info.chunk_size * 2)
      rd->buffer_size = rd->backend_info.chunk_size * 32;

   if (rd->fifo_buffer != NULL)
      rsnd_fifo_free(rd->fifo_buffer);
   rd->fifo_buffer = rsnd_fifo_new (rd->buffer_size);
   if (rd->fifo_buffer == NULL)
   {
      RSD_ERR("Failed to create fifobuf");
      return -1;
   }

   // Only bother with setting network buffer size if we're doing TCP.
   if (rd->conn_type & RSD_CONN_TCP)
   {
#define MAX_TCP_BUFSIZE (1 << 14)
      int bufsiz = rd->buffer_size;
      if (bufsiz > MAX_TCP_BUFSIZE)
         bufsiz = MAX_TCP_BUFSIZE;

      setsockopt(rd->conn.socket, SOL_SOCKET, SO_SNDBUF, CONST_CAST &bufsiz, sizeof(int));
      bufsiz = rd->buffer_size;
      setsockopt(rd->conn.ctl_socket, SOL_SOCKET, SO_SNDBUF, CONST_CAST &bufsiz, sizeof(int));
      bufsiz = rd->buffer_size;
      setsockopt(rd->conn.ctl_socket, SOL_SOCKET, SO_RCVBUF, CONST_CAST &bufsiz, sizeof(int));

      int flag = 1;
      setsockopt(rd->conn.socket, IPPROTO_TCP, TCP_NODELAY, CONST_CAST &flag, sizeof(int));
      flag = 1;
      setsockopt(rd->conn.ctl_socket, IPPROTO_TCP, TCP_NODELAY, CONST_CAST &flag, sizeof(int));
   }

   // Can we read the last 8 bytes so we can use the protocol interface?
   // This is non-blocking.
   if (rsnd_recv_chunk(rd->conn.socket, rsnd_header, RSND_HEADER_SIZE, 0) == RSND_HEADER_SIZE)
      rd->conn_type |= RSD_CONN_PROTO; 
   else
   {  
      RSD_DEBUG("Failed to get new proto"); 
   }

   // We no longer want to read from this socket.
#ifdef _WIN32
   shutdown(rd->conn.socket, SD_RECEIVE);
#else
   shutdown(rd->conn.socket, SHUT_RD);
#endif

   return 0;
}

/* Makes sure that we're connected and done with wave header handshaking. Returns -1 on error, and 0 on success. 
   This goes for all other functions in use. */
static int rsnd_create_connection(rsound_t *rd)
{
   int rc;

   /* Are we connected to the server? If not, these values have been set to <0, so we make sure that we connect */
   if (rd->conn.socket <= 0 && rd->conn.ctl_socket <= 0)
   {
      rc = rsnd_connect_server(rd);
      if (rc < 0)
      {
         RSD_ERR("connect server failed!");
         rsd_stop(rd);
         return -1;
      }

      /* After connecting, makes really sure that we have a working connection. */
      struct pollfd fd = {
         .fd = rd->conn.socket,
         .events = POLLOUT
      };

      if (rsnd_poll(&fd, 1, 2000) < 0)
      {
         RSD_ERR("rsnd_poll failed!");
         rsd_stop(rd);
         return -1;
      }

      if (!(fd.revents & POLLOUT))
      {
         RSD_ERR("Poll didn't return what we wanted!");
         rsd_stop(rd);
         return -1;
      }
   }
   /* Is the server ready for data? The first thing it expects is the wave header */
   if (!rd->ready_for_data)
   {
      /* Part of the uber simple protocol.
         1. Send wave header.
         2. Recieve backend info like latency and preferred packet size.
         3. Starts the playback thread. */

      rc = rsnd_send_header_info(rd);
      if (rc < 0)
      {
         RSD_ERR("Send header failed!");
         rsd_stop(rd);
         return -1;
      }

      rc = rsnd_get_backend_info(rd);
      if (rc < 0)
      {
         RSD_ERR("Get backend info failed!");
         rsd_stop(rd);
         return -1;
      }

      rc = rsnd_start_thread(rd);
      if (rc < 0)
      {
         RSD_ERR("Starting thread failed!");
         rsd_stop(rd);
         return -1;
      }

      if ((rd->conn_type & RSD_CONN_PROTO) && strlen(rd->identity) > 0)
      {
         rsnd_send_identity_info(rd);
      }

      rd->ready_for_data = 1;
   }

   return 0;
}

/* Sends a chunk over the network. Makes sure that everything is sent if blocking. Returns -1 if connection is lost, non-negative if success.
 * If blocking, and not enough data is recieved, it will return -1. */
static ssize_t rsnd_send_chunk(int socket, const void* buf, size_t size, int blocking)
{
   ssize_t rc = 0;
   size_t wrote = 0;
   ssize_t send_size = 0;
   struct pollfd fd = {
      .fd = socket,
      .events = POLLOUT
   };

   int sleep_time = (blocking) ? 10000 : 0;

#define MAX_PACKET_SIZE 1024

   while (wrote < size)
   {
      if (rsnd_poll(&fd, 1, sleep_time) < 0)
         return -1;

      if (fd.revents & POLLOUT)
      {
         /* We try to limit ourselves to 1KiB packet sizes. */
         send_size = (size - wrote) > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : size - wrote;
         rc = send(socket, (const char*)buf + wrote, send_size, 0);
         if (rc < 0)
         {
            RSD_ERR("Error sending chunk, %s\n", strerror(errno));
            return rc;
         }
         wrote += rc;
      }
      else if (fd.revents & POLLHUP)
      {
         RSD_WARN("*** Remote side hung up! ***");
         return -1;
      }
      else
      {
         /* If server hasn't stopped blocking after 10 secs, then we should probably shut down the stream. */
         if (blocking)
            return -1;
         else
            return wrote;
      }

   }
   return (ssize_t)wrote;
}

/* Recieved chunk. Makes sure that everything is recieved if blocking. Returns -1 if connection is lost, non-negative if success.
 * If blocking, and not enough data is recieved, it will return -1. */
static ssize_t rsnd_recv_chunk(int socket, void *buf, size_t size, int blocking)
{
   ssize_t rc = 0;
   size_t has_read = 0;
   ssize_t read_size = 0;
   struct pollfd fd = {
      .fd = socket,
      .events = POLLIN
   };

   int sleep_time = (blocking) ? 5000 : 0;

   while (has_read < size)
   {
      if (rsnd_poll(&fd, 1, sleep_time) < 0)
      {
         RSD_ERR("Poll failed");
         return -1;
      }

      if (fd.revents & POLLIN)
      {
         read_size = (size - has_read) > MAX_PACKET_SIZE ? MAX_PACKET_SIZE : size - has_read;
         rc = recv(socket, (char*)buf + has_read, read_size, 0);
         if (rc <= 0)
         {
            RSD_ERR("Error receiving chunk, %s\n", strerror(errno));
            return rc;
         }
         has_read += rc;
      }
      else if (fd.revents & POLLHUP)
      {
         RSD_ERR("Server hung up");
         return -1;
      }
      else
      {
         if (blocking)
         {
            RSD_ERR("Block FAIL!");
            return -1;
         }
         else
            return has_read;
      }
   }

   return (ssize_t)has_read;
}

static int rsnd_poll(struct pollfd *fd, int numfd, int timeout)
{
   for(;;)
   {
      if (poll(fd, numfd, timeout) < 0)
      {
         if ( errno == EINTR )
            continue;

         perror("poll");
         return -1;
      }
      return 0;
   }

   // Avoids warning
   return 0;
}

static void rsnd_sleep(int msecs)
{
#ifdef _WIN32
   Sleep(msecs);
#else
   struct timespec tv = {
      .tv_sec = msecs / 1000,
      .tv_nsec = ((long long)msecs * 1000000) % 1000000000
   };
   nanosleep(&tv, NULL);
#endif
}


/* Calculates how many bytes there are in total in the virtual buffer. This is calculated client side.
   It should be accurate enough unless we have big problems with buffer underruns.
   This function is called by rsd_delay() to determine the latency. 
   This function might be changed in the future to correctly determine latency from server. */
static void rsnd_drain(rsound_t *rd)
{
   /* If the audio playback has started on the server we need to use timers. */
   if (rd->has_written)
   {
      int64_t temp, temp2;

      /* Falls back to gettimeofday() when CLOCK_MONOTONIC is not supported */

      /* Calculates the amount of bytes that the server has consumed. */
#if defined(_POSIX_MONOTONIC_CLOCK) && !defined(__APPLE__)
      struct timespec now_tv;
      clock_gettime(CLOCK_MONOTONIC, &now_tv);

      temp = (int64_t)now_tv.tv_sec - (int64_t)rd->start_tv_nsec.tv_sec;

      temp *= rd->rate * rd->channels * rd->samplesize;

      temp2 = (int64_t)now_tv.tv_nsec - (int64_t)rd->start_tv_nsec.tv_nsec;
      temp2 *= rd->rate * rd->channels * rd->samplesize;
      temp2 /= 1000000000;
      temp += temp2;
#else
      struct timeval now_tv;
      gettimeofday(&now_tv, NULL);

      temp = (int64_t)now_tv.tv_sec - (int64_t)rd->start_tv_usec.tv_sec;
      temp *= rd->rate * rd->channels * rd->samplesize;

      temp2 = (int64_t)now_tv.tv_usec - (int64_t)rd->start_tv_usec.tv_usec;
      temp2 *= rd->rate * rd->channels * rd->samplesize;
      temp2 /= 1000000;
      temp += temp2;
#endif
      /* Calculates the amount of data we have in our virtual buffer. Only used to calculate delay. */
      pthread_mutex_lock(&rd->thread.mutex);
      rd->bytes_in_buffer = (int)((int64_t)rd->total_written + (int64_t)rsnd_fifo_read_avail(rd->fifo_buffer) - temp);
      pthread_mutex_unlock(&rd->thread.mutex);
   }
   else
   {
      pthread_mutex_lock(&rd->thread.mutex);
      rd->bytes_in_buffer = rsnd_fifo_read_avail(rd->fifo_buffer);
      pthread_mutex_unlock(&rd->thread.mutex);
   }
}

/* Tries to fill the buffer. Uses signals to determine when the buffer is ready to be filled. Should the thread not be active
   it will treat this as an error. Crude implementation of a blocking FIFO. */ 
static size_t rsnd_fill_buffer(rsound_t *rd, const char *buf, size_t size)
{

   /* Wait until we have a ready buffer */
   for (;;)
   {
      /* Should the thread be shut down while we're running, return with error */
      if (!rd->thread_active)
         return 0;

      pthread_mutex_lock(&rd->thread.mutex);
      if (rsnd_fifo_write_avail(rd->fifo_buffer) >= size)
      {
         pthread_mutex_unlock(&rd->thread.mutex);
         break;
      }
      pthread_mutex_unlock(&rd->thread.mutex);

      /* Sleeps until we can write to the FIFO. */
      pthread_mutex_lock(&rd->thread.cond_mutex);
      pthread_cond_signal(&rd->thread.cond);

      RSD_DEBUG("rsnd_fill_buffer: Going to sleep.");
      pthread_cond_wait(&rd->thread.cond, &rd->thread.cond_mutex);
      RSD_DEBUG("rsnd_fill_buffer: Woke up.");
      pthread_mutex_unlock(&rd->thread.cond_mutex);
   }

   pthread_mutex_lock(&rd->thread.mutex);
   rsnd_fifo_write(rd->fifo_buffer, buf, size);
   pthread_mutex_unlock(&rd->thread.mutex);
   //RSD_DEBUG("fill_buffer: Wrote to buffer.");

   /* Send signal to thread that buffer has been updated */
   //RSD_DEBUG("fill_buffer: Waking up thread.");
   pthread_cond_signal(&rd->thread.cond);

   return size;
}

static int rsnd_start_thread(rsound_t *rd)
{
   int rc;
   if (!rd->thread_active)
   {
      rd->thread_active = 1;
      rc = pthread_create(&rd->thread.threadId, NULL, rd->audio_callback ? rsnd_cb_thread : rsnd_thread, rd);
      if (rc < 0)
      {
         rd->thread_active = 0;
         RSD_ERR("Failed to create thread.");
         return -1;
      }
      return 0;
   }
   else
      return 0;
}

/* Makes sure that the playback thread has been correctly shut down */
static int rsnd_stop_thread(rsound_t *rd)
{
   if (rd->thread_active)
   {

      RSD_DEBUG("Shutting down thread.");

      pthread_mutex_lock(&rd->thread.cond_mutex);
      rd->thread_active = 0;
      pthread_cond_signal(&rd->thread.cond);
      pthread_mutex_unlock(&rd->thread.cond_mutex);

      if (pthread_join(rd->thread.threadId, NULL) < 0)
         RSD_WARN("*** Warning, did not terminate thread. ***");
      else
         RSD_DEBUG("Thread joined successfully.");
      
      return 0;
   }
   else
   {
      RSD_DEBUG("Thread is already shut down.");
      return 0;
   }
}

/* Calculates audio delay in bytes */
static size_t rsnd_get_delay(rsound_t *rd)
{
   int ptr;
   rsnd_drain(rd);
   ptr = rd->bytes_in_buffer;

   /* Adds the backend latency to the calculated latency. */
   ptr += (int)rd->backend_info.latency;

   pthread_mutex_lock(&rd->thread.mutex);
   ptr += rd->delay_offset;
   rd->use_latency = 1;
   RSD_DEBUG("Offset: %d", rd->delay_offset);
   pthread_mutex_unlock(&rd->thread.mutex);

   if (ptr < 0)
      ptr = 0;

   return (size_t)ptr;
}

static size_t rsnd_get_ptr(rsound_t *rd)
{
   int ptr;
   pthread_mutex_lock(&rd->thread.mutex);
   ptr = rsnd_fifo_read_avail(rd->fifo_buffer);
   pthread_mutex_unlock(&rd->thread.mutex);

   return ptr;
}

static int rsnd_send_identity_info(rsound_t *rd)
{
#define RSD_PROTO_MAXSIZE 256
#define RSD_PROTO_CHUNKSIZE 8

   char tmpbuf[RSD_PROTO_MAXSIZE];
   char sendbuf[RSD_PROTO_MAXSIZE];

   snprintf(tmpbuf, RSD_PROTO_MAXSIZE - 1, " IDENTITY %s", rd->identity);
   tmpbuf[RSD_PROTO_MAXSIZE - 1] = '\0';
   snprintf(sendbuf, RSD_PROTO_MAXSIZE - 1, "RSD%5d%s", (int)strlen(tmpbuf), tmpbuf);
   sendbuf[RSD_PROTO_MAXSIZE - 1] = '\0';

   if (rsnd_send_chunk(rd->conn.ctl_socket, sendbuf, strlen(sendbuf), 0) != (ssize_t)strlen(sendbuf))
      return -1;

   return 0;
}

static int rsnd_close_ctl(rsound_t *rd)
{
   if (!(rd->conn_type & RSD_CONN_PROTO))
      return -1;

   struct pollfd fd = {
      .fd = rd->conn.ctl_socket,
      .events = POLLOUT
   };

   if (rsnd_poll(&fd, 1, 0) < 0)
      return -1;

   if (fd.revents & POLLOUT)
   {
      const char *sendbuf = "RSD    9 CLOSECTL";
      if ( send(rd->conn.ctl_socket, sendbuf, strlen(sendbuf), 0) < 0 )
         return -1;
   }
   else if (fd.revents & POLLHUP)
      return 0;

   // Let's wait for reply (or POLLHUP)

   fd.events = POLLIN;
   int index = 0;
   char buf[RSD_PROTO_MAXSIZE*2] = {0};

   for(;;)
   {
      if (rsnd_poll(&fd, 1, 2000) < 0)
         return -1;

      if (fd.revents & POLLHUP)
         break;

      else if (fd.revents & POLLIN)
      {
         const char *subchar;
         int rc;

         // We just read everything in large chunks until we find what we're looking for
         if ((rc = recv(rd->conn.ctl_socket, buf + index, RSD_PROTO_MAXSIZE*2 - 1 - index, 0)) <= 0)
            return -1;

         // Can we find it directly?
         if (strstr(buf, "RSD   12 CLOSECTL OK") != NULL)
            break;
         else if (strstr(buf, "RSD   15 CLOSECTL ERROR") != NULL)
            return -1;

         subchar = strrchr(buf, 'R');
         if (subchar == NULL)
            index = 0;
         else
         {
            memmove(buf, subchar, strlen(subchar) + 1);
            index = strlen(buf);
         }

      }
      else
         return -1;
   }

   close(rd->conn.ctl_socket);
   return 0;
}


// Sends delay info request to server on the ctl socket. This code section isn't critical, and will work if it works. 
// It will never block.
static int rsnd_send_info_query(rsound_t *rd)
{
   char tmpbuf[RSD_PROTO_MAXSIZE];
   char sendbuf[RSD_PROTO_MAXSIZE];

#ifdef _WIN32
   snprintf(tmpbuf, RSD_PROTO_MAXSIZE - 1, " INFO %I64d", (__int64)rd->total_written);
#else
   snprintf(tmpbuf, RSD_PROTO_MAXSIZE - 1, " INFO %lld", (long long int)rd->total_written);
#endif
   tmpbuf[RSD_PROTO_MAXSIZE - 1] = '\0';
   snprintf(sendbuf, RSD_PROTO_MAXSIZE - 1, "RSD%5d%s", (int)strlen(tmpbuf), tmpbuf);
   sendbuf[RSD_PROTO_MAXSIZE - 1] = '\0';

   if (rsnd_send_chunk(rd->conn.ctl_socket, sendbuf, strlen(sendbuf), 0) != (ssize_t)strlen(sendbuf))
      return -1;

   return 0;
}

// We check if there's any pending delay information from the server.
// In that case, we read the packet.
static int rsnd_update_server_info(rsound_t *rd)
{
   ssize_t rc;

   long long int client_ptr = -1;
   long long int serv_ptr = -1;
   char temp[RSD_PROTO_MAXSIZE + 1] = {0};

   // We read until we have the last (most recent) data in the network buffer.
   for (;;)
   {
      const char *substr;
      char *tmpstr;
      memset(temp, 0, sizeof(temp));

      // We first recieve the small header. We just use the larger buffer as it is disposable.
      rc = rsnd_recv_chunk(rd->conn.ctl_socket, temp, RSD_PROTO_CHUNKSIZE, 0);
      if (rc == 0)
         break;
      else if (rc < RSD_PROTO_CHUNKSIZE)
         return -1;

      temp[RSD_PROTO_CHUNKSIZE] = '\0';

      if ( (substr = strstr(temp, "RSD")) == NULL )
         return -1;

      // Jump over "RSD" in header
      substr += 3;

      // The length of the argument message is stored in the small 8 byte header.
      long int len = strtol(substr, NULL, 0);

      // Recieve the rest of the data.
      if (rsnd_recv_chunk(rd->conn.ctl_socket, temp, len, 0) < len)
         return -1;

      // We only bother if this is an INFO message.
      substr = strstr(temp, "INFO");
      if (substr == NULL)
         continue;

      // Jump over "INFO" in header
      substr += 4;

      client_ptr = strtoull(substr, &tmpstr, 0);
      if (client_ptr == 0 || *tmpstr == '\0')
         return -1;

      substr = tmpstr;
      serv_ptr = strtoull(substr, NULL, 0);
      if (serv_ptr <= 0)
         return -1;
   }

   if (client_ptr > 0 && serv_ptr > 0)
   {
      int delay = rsd_delay(rd);
      int delta = (int)(client_ptr - serv_ptr);
      pthread_mutex_lock(&rd->thread.mutex);
      delta += rsnd_fifo_read_avail(rd->fifo_buffer);
      pthread_mutex_unlock(&rd->thread.mutex);

      RSD_DEBUG("Delay: %d, Delta: %d", delay, delta);

      // We only update the pointer if the data we got is quite recent.
      if (rd->total_written - client_ptr < 4 * rd->backend_info.chunk_size && rd->total_written > client_ptr)
      {
         int offset_delta = delta - delay;
         int max_offset = rd->backend_info.chunk_size;
         if (offset_delta < -max_offset)
            offset_delta = -max_offset;
         else if (offset_delta > max_offset)
            offset_delta = max_offset;

         pthread_mutex_lock(&rd->thread.mutex);
         rd->delay_offset += offset_delta;
         pthread_mutex_unlock(&rd->thread.mutex);
         RSD_DEBUG("Changed offset-delta: %d", offset_delta);
      }
   }

   return 0;
}

// Sort of simulates the behavior of pthread_cancel()
#define _TEST_CANCEL() \
   if (!rd->thread_active) \
      break

/* The blocking thread */
static void* rsnd_thread ( void * thread_data )
{
   /* We share data between thread and callable functions */
   rsound_t *rd = thread_data;
   int rc;
   char buffer[rd->backend_info.chunk_size];

   /* Plays back data as long as there is data in the buffer. Else, sleep until it can. */
   /* Two (;;) for loops! :3 Beware! */
   for (;;)
   {
      for(;;)
      {
         _TEST_CANCEL();

         // We ask the server to send its latest backend data. Do not really care about errors atm.
         // We only bother to check after 1 sec of audio has been played, as it might be quite inaccurate in the start of the stream.
         if (rd->use_latency && (rd->conn_type & RSD_CONN_PROTO) && (rd->total_written > rd->channels * rd->rate * rd->samplesize))
         {
            rsnd_send_info_query(rd); 
            rsnd_update_server_info(rd);
         }

         /* If the buffer is empty or we've stopped the stream, jump out of this for loop */
         pthread_mutex_lock(&rd->thread.mutex);
         if (rsnd_fifo_read_avail(rd->fifo_buffer) < rd->backend_info.chunk_size || !rd->thread_active)
         {
            pthread_mutex_unlock(&rd->thread.mutex);
            break;
         }
         pthread_mutex_unlock(&rd->thread.mutex);

         _TEST_CANCEL();
         pthread_mutex_lock(&rd->thread.mutex);
         rsnd_fifo_read(rd->fifo_buffer, buffer, sizeof(buffer));
         pthread_mutex_unlock(&rd->thread.mutex);
         if (rd->event_callback)
            rd->event_callback(rd->event_data);
         rc = rsnd_send_chunk(rd->conn.socket, buffer, sizeof(buffer), 1);

         /* If this happens, we should make sure that subsequent and current calls to rsd_write() will fail. */
         if (rc != (int)rd->backend_info.chunk_size)
         {
            _TEST_CANCEL();
            rsnd_reset(rd);

            /* Wakes up a potentially sleeping fill_buffer() */
            pthread_cond_signal(&rd->thread.cond);

            /* This thread will not be joined, so detach. */
            pthread_detach(pthread_self());
            pthread_exit(NULL);
         }

         /* If this was the first write, set the start point for the timer. */
         if (!rd->has_written)
         {
            pthread_mutex_lock(&rd->thread.mutex);
#if defined(_POSIX_MONOTONIC_CLOCK) && !defined(__APPLE__)
            clock_gettime(CLOCK_MONOTONIC, &rd->start_tv_nsec);
#else
            gettimeofday(&rd->start_tv_usec, NULL);
#endif
            rd->has_written = 1;
            pthread_mutex_unlock(&rd->thread.mutex);
         }

         /* Increase the total_written counter. Used in rsnd_drain() */
         pthread_mutex_lock(&rd->thread.mutex);
         rd->total_written += rc;
         pthread_mutex_unlock(&rd->thread.mutex);

         /* Buffer has decreased, signal fill_buffer() */
         pthread_cond_signal(&rd->thread.cond);

      }

      /* If we're still good to go, sleep. We are waiting for fill_buffer() to fill up some data. */

      if (rd->thread_active)
      {
         // There is a very slim change of getting a deadlock using the cond_wait scheme.
         // This solution is rather dirty, but avoids complete deadlocks at the very least.

         pthread_mutex_lock(&rd->thread.cond_mutex);
         pthread_cond_signal(&rd->thread.cond);

         if (rd->thread_active)
         {
            RSD_DEBUG("Thread going to sleep.");
            pthread_cond_wait(&rd->thread.cond, &rd->thread.cond_mutex);
            RSD_DEBUG("Thread woke up.");
         }

         pthread_mutex_unlock(&rd->thread.cond_mutex);
         RSD_DEBUG("Thread unlocked cond_mutex.");
      }
      /* Abort request, chap. */
      else
      {
         pthread_cond_signal(&rd->thread.cond);
         pthread_exit(NULL);
      }

   }
}

/* Callback thread */
static void* rsnd_cb_thread(void *thread_data)
{
   rsound_t *rd = thread_data;
   size_t read_size = rd->backend_info.chunk_size;
   if (rd->cb_max_size != 0 && rd->cb_max_size < read_size)
      read_size = rd->cb_max_size;

   uint8_t buffer[rd->backend_info.chunk_size];

   while (rd->thread_active)
   {
      size_t has_read = 0;

      while (has_read < rd->backend_info.chunk_size)
      {
         size_t will_read = read_size < rd->backend_info.chunk_size - has_read ? read_size : rd->backend_info.chunk_size - has_read;

         rsd_callback_lock(rd);
         ssize_t ret = rd->audio_callback(buffer + has_read, will_read, rd->cb_data);
         rsd_callback_unlock(rd);

         if (ret < 0)
         {
            rsnd_reset(rd);
            pthread_detach(pthread_self());
            rd->error_callback(rd->cb_data);
            pthread_exit(NULL);
         }

         has_read += ret;

         if (ret < (ssize_t)will_read)
         {
            if ((int)rsd_delay_ms(rd) < rd->max_latency / 2)
            {
               RSD_DEBUG("Callback thread: Requested %d bytes, got %d\n", (int)will_read, (int)ret);
               memset(buffer + has_read, 0, will_read - ret);
               has_read += will_read - ret;
            }
            else
            {
               // The network might do things in large chunks, so it may request large amounts of data in short periods of time.
               // This breaks when the caller cannot buffer up big buffers beforehand, so do short sleeps inbetween.
               // This is somewhat dirty, but I cannot see a better solution.
               rsnd_sleep(1);
            }
         }
      }

      ssize_t ret = rsnd_send_chunk(rd->conn.socket, buffer, rd->backend_info.chunk_size, 1);
      if (ret != (ssize_t)rd->backend_info.chunk_size)
      {
         rsnd_reset(rd);
         pthread_detach(pthread_self());
         rd->error_callback(rd->cb_data);
         pthread_exit(NULL);
      }

      /* If this was the first write, set the start point for the timer. */
      if (!rd->has_written)
      {
#if defined(_POSIX_MONOTONIC_CLOCK) && !defined(__APPLE__)
         clock_gettime(CLOCK_MONOTONIC, &rd->start_tv_nsec);
#else
         gettimeofday(&rd->start_tv_usec, NULL);
#endif
         rd->has_written = 1;
      }

      rd->total_written += rd->backend_info.chunk_size;

      if ((rd->conn_type & RSD_CONN_PROTO) && (rd->total_written > rd->channels * rd->rate * rd->samplesize))
      {
         rsnd_send_info_query(rd); 
         rsnd_update_server_info(rd);
      }

      if (rd->has_written)
         rsd_delay_wait(rd);
   }
   pthread_exit(NULL);
}

static int rsnd_reset(rsound_t *rd)
{
   if (rd->conn.socket != -1)
      close(rd->conn.socket);

   if (rd->conn.ctl_socket != 1)
      close(rd->conn.ctl_socket);

   /* Pristine stuff, baby! */
   pthread_mutex_lock(&rd->thread.mutex);
   rd->conn.socket = -1;
   rd->conn.ctl_socket = -1;
   rd->total_written = 0;
   rd->ready_for_data = 0;
   rd->has_written = 0;
   rd->bytes_in_buffer = 0;
   rd->thread_active = 0;
   rd->delay_offset = 0;
   rd->use_latency = 0;
   pthread_mutex_unlock(&rd->thread.mutex);
   pthread_cond_signal(&rd->thread.cond);

   return 0;
}


RSD_API_DECL int RSD_API_CALLTYPE rsd_stop(rsound_t *rd)
{
   assert(rd != NULL);
   rsnd_stop_thread(rd);

   const char buf[] = "RSD    5 STOP";

   // Do not really care about errors here. 
   // The socket will be closed down in any case in rsnd_reset().
   rsnd_send_chunk(rd->conn.ctl_socket, buf, strlen(buf), 0);

   rsnd_reset(rd);
   return 0;
}

RSD_API_DECL size_t RSD_API_CALLTYPE rsd_write( rsound_t *rsound, const void* buf, size_t size)
{
   assert(rsound != NULL);
   if (!rsound->ready_for_data)
   {
      return 0;
   }

   size_t result;
   size_t max_write = (rsound->buffer_size - rsound->backend_info.chunk_size)/2;

   size_t written = 0;
   size_t write_size;

   /* Makes sure that we can handle arbitrary large write sizes */

   while (written < size)
   {
      write_size = (size - written) > max_write ? max_write : (size - written); 
      result = rsnd_fill_buffer(rsound, (const char*)buf + written, write_size);

      if (result <= 0)
      {
         rsd_stop(rsound);
         return 0;
      }
      written += result;
   }
   return written;
}

RSD_API_DECL int RSD_API_CALLTYPE rsd_start(rsound_t *rsound)
{
   assert(rsound != NULL);
   assert(rsound->rate > 0);
   assert(rsound->channels > 0);
   assert(rsound->host != NULL);
   assert(rsound->port != NULL);

   if (rsnd_create_connection(rsound) < 0)
   {
      return -1;
   }

   return 0;
}

RSD_API_DECL int RSD_API_CALLTYPE rsd_exec(rsound_t *rsound)
{
   assert(rsound != NULL);
   RSD_DEBUG("rsd_exec()");

   // Makes sure we have a working connection
   if (rsound->conn.socket < 0)
   {
      RSD_DEBUG("Calling rsd_start()");
      if (rsd_start(rsound) < 0)
      {
         RSD_ERR("rsd_start() failed!");
         return -1;
      }
   }

   RSD_DEBUG("Closing ctl");
   if (rsnd_close_ctl(rsound) < 0)
      return -1;

   int fd = rsound->conn.socket;
   RSD_DEBUG("Socket: %d", fd);

   rsnd_stop_thread(rsound);

   // Unsets NONBLOCK
#ifdef _WIN32
   u_long iMode = 0;
   ioctlsocket(fd, FIONBIO, &iMode);
#else
   int flags = fcntl(fd, F_GETFL);
   if ( flags < 0 )
   {
      RSD_ERR("Failed to get fcntl flags.");
      rsnd_start_thread(rsound);
      return -1;
   }

   flags &= ~O_NONBLOCK;
   if ( fcntl(fd, F_SETFL, flags) < 0 )
   {
      RSD_ERR("Failed to set fcntl flags.");
      rsnd_start_thread(rsound);
      return -1;
   }
#endif

   // Flush the buffer

   if (rsnd_fifo_read_avail(rsound->fifo_buffer) > 0)
   {
      char buffer[rsnd_fifo_read_avail(rsound->fifo_buffer)];
      rsnd_fifo_read(rsound->fifo_buffer, buffer, sizeof(buffer));
      if (rsnd_send_chunk(fd, buffer, sizeof(buffer), 1) != (ssize_t)sizeof(buffer))
      {
         RSD_DEBUG("Failed flushing buffer!");
         close(fd);
         return -1;
      }
   }

   RSD_DEBUG("Returning from rsd_exec()");
   rsd_free(rsound);
   return fd;
}


/* ioctl()-ish param setting :D */
RSD_API_DECL int RSD_API_CALLTYPE rsd_set_param(rsound_t *rd, enum rsd_settings option, void* param)
{
   assert(rd != NULL);
   assert(param != NULL);

   switch(option)
   {
      case RSD_SAMPLERATE:
         if (*(int*)param > 0)
         {
            rd->rate = *((int*)param);
            break;
         }
         else
            return -1;
      case RSD_CHANNELS:
         if (*(int*)param > 0)
         {
            rd->channels = *((int*)param);
            break;
         }
         else
            return -1;
      case RSD_HOST:
         if (rd->host != NULL)
            free(rd->host);
         rd->host = strdup(param);
         break;
      case RSD_PORT:
         if (rd->port != NULL)
            free(rd->port);
         rd->port = strdup(param);
         break;
      case RSD_BUFSIZE:
         if (*(int*)param > 0)
         {
            rd->buffer_size = *((int*)param);
            break;
         }
         else
            return -1;
         break;
      case RSD_LATENCY:
         rd->max_latency = *((int*)param);
         break;

         // Checks if format is valid.   
      case RSD_FORMAT:
         rd->format = (uint16_t)(*((int*)param));
         if ((rd->samplesize = rsnd_format_to_samplesize(rd->format)) == -1)
         {
            rd->format = RSD_S16_LE;
            rd->samplesize = rsnd_format_to_samplesize(RSD_S16_LE);
            *((int*)param) = (int)RSD_S16_LE;
         }
         break;

      case RSD_IDENTITY:
         strncpy(rd->identity, param, sizeof(rd->identity));
         rd->identity[sizeof(rd->identity)-1] = '\0';
         break;

      default:
         return -1;
   }
   return 0;

}

RSD_API_DECL void RSD_API_CALLTYPE rsd_delay_wait(rsound_t *rd)
{

   /* When called, we make sure that the latency never goes over the time designated in RSD_LATENCY.
      Useful for certain blocking I/O designs where the latency still needs to be quite low.
      Without this, the latency of the stream will depend on how big the network buffers are.
      ( We simulate that we're a low latency sound card ) */

   /* Should we bother with checking latency at all? */
   if (rd->max_latency > 0)
   {
      /* Latency of stream in ms */
      int latency_ms = rsd_delay_ms(rd);


      /* Should we sleep for a while to keep the latency low? */
      if (rd->max_latency < latency_ms)
      {
         int64_t sleep_ms = latency_ms - rd->max_latency;
         RSD_DEBUG("Delay wait: %d ms\n", (int)sleep_ms);
         rsnd_sleep(sleep_ms);
      }
   }
}

RSD_API_DECL size_t RSD_API_CALLTYPE rsd_pointer(rsound_t *rsound)
{
   assert(rsound != NULL);
   int ptr;

   ptr = rsnd_get_ptr(rsound);   

   return ptr;
}

RSD_API_DECL size_t RSD_API_CALLTYPE rsd_get_avail(rsound_t *rd)
{
   assert(rd != NULL);
   int ptr;
   ptr = rsnd_get_ptr(rd);
   return rd->buffer_size - ptr;
}

RSD_API_DECL size_t RSD_API_CALLTYPE rsd_delay(rsound_t *rd)
{
   assert(rd != NULL);
   int ptr = rsnd_get_delay(rd);
   if (ptr < 0)
      ptr = 0;

   return ptr;
}

RSD_API_DECL size_t RSD_API_CALLTYPE rsd_delay_ms(rsound_t* rd)
{
   assert(rd);
   assert(rd->rate > 0 && rd->channels > 0);

   return (rsd_delay(rd) * 1000) / ( rd->rate * rd->channels * rd->samplesize );
}

RSD_API_DECL int RSD_API_CALLTYPE rsd_pause(rsound_t* rsound, int enable)
{
   assert(rsound != NULL);
   if (enable)
      return rsd_stop(rsound);
   else
      return rsd_start(rsound);
}

RSD_API_DECL int RSD_API_CALLTYPE rsd_init(rsound_t** rsound)
{
   assert(rsound != NULL);
   *rsound = calloc(1, sizeof(rsound_t));
   if (*rsound == NULL)
      return -1;

   (*rsound)->conn.socket = -1;
   (*rsound)->conn.ctl_socket = -1;

   pthread_mutex_init(&(*rsound)->thread.mutex, NULL);
   pthread_mutex_init(&(*rsound)->thread.cond_mutex, NULL);
   pthread_mutex_init(&(*rsound)->cb_lock, NULL);
   pthread_cond_init(&(*rsound)->thread.cond, NULL);

   // Assumes default of S16_LE samples.
   int format = RSD_S16_LE;
   rsd_set_param(*rsound, RSD_FORMAT, &format);

   /* Checks if environment variable RSD_SERVER and RSD_PORT are set, and valid */
   char *rsdhost = getenv("RSD_SERVER");
   char *rsdport = getenv("RSD_PORT");
   if (rsdhost != NULL && strlen(rsdhost))
      rsd_set_param(*rsound, RSD_HOST, rsdhost);
   else
      rsd_set_param(*rsound, RSD_HOST, RSD_DEFAULT_HOST);

   if (rsdport != NULL && strlen(rsdport))
      rsd_set_param(*rsound, RSD_PORT, rsdport);
   else
      rsd_set_param(*rsound, RSD_PORT, RSD_DEFAULT_PORT);

#ifdef _WIN32
   if (!init_count)
   {
      if (init_wsock() < 0)
      {
         rsd_free(*rsound);
         return -1;
      }
      init_count++;
   }
#endif

   return 0;
}

RSD_API_DECL int RSD_API_CALLTYPE rsd_simple_start(rsound_t** rsound, const char* host, const char* port, const char* ident,
                     int rate, int channels, enum rsd_format format)
{
   if (rsd_init(rsound) < 0)
      return -1;

   int fmt = format;

   if (host != NULL)
      rsd_set_param(*rsound, RSD_HOST, (void*)host);
   if (port != NULL)
      rsd_set_param(*rsound, RSD_PORT, (void*)port);
   if (ident != NULL)
      rsd_set_param(*rsound, RSD_IDENTITY, (void*)ident);

   if (  rsd_set_param(*rsound, RSD_SAMPLERATE, &rate) < 0 ||
         rsd_set_param(*rsound, RSD_CHANNELS, &channels) < 0 ||
         rsd_set_param(*rsound, RSD_FORMAT, &fmt) < 0 )
   {
      rsd_free(*rsound);
      return -1;
   }

   if (rsd_start(*rsound) < 0)
   {
      rsd_free(*rsound);
      return -1;
   }

   return 0;
}

RSD_API_DECL void RSD_API_CALLTYPE rsd_set_callback(rsound_t *rsound, rsd_audio_callback_t audio_cb,
      rsd_error_callback_t err_cb, size_t max_size, void *userdata)
{
   assert(rsound != NULL);

   rsound->audio_callback = audio_cb;
   rsound->error_callback = err_cb;
   rsound->cb_max_size = max_size;
   rsound->cb_data = userdata;

   if (rsound->audio_callback)
      assert(rsound->error_callback);
}

RSD_API_DECL void RSD_API_CALLTYPE rsd_set_event_callback(rsound_t *rsound, rsd_event_callback_t event_cb,
      void *userdata)
{
   assert(rsound != NULL);

   rsound->event_callback = event_cb;
   rsound->event_data = userdata;
}

RSD_API_DECL void RSD_API_CALLTYPE rsd_callback_lock(rsound_t *rsound)
{
   pthread_mutex_lock(&rsound->cb_lock);
}

RSD_API_DECL void RSD_API_CALLTYPE rsd_callback_unlock(rsound_t *rsound)
{
   pthread_mutex_unlock(&rsound->cb_lock);
}

RSD_API_DECL int RSD_API_CALLTYPE rsd_free(rsound_t *rsound)
{
   assert(rsound != NULL);
   if (rsound->fifo_buffer)
      rsnd_fifo_free(rsound->fifo_buffer);
   if (rsound->host)
      free(rsound->host);
   if (rsound->port)
      free(rsound->port);

   int err;

   if ((err = pthread_mutex_destroy(&rsound->thread.mutex)) != 0)
   {
      RSD_WARN("Error: %s\n", strerror(err));
      return -1;
   }

   if ((err = pthread_mutex_destroy(&rsound->thread.cond_mutex)) != 0)
   {
      RSD_WARN("Error: %s\n", strerror(err));
      return -1;
   }

   if ((err = pthread_mutex_destroy(&rsound->cb_lock)) != 0)
   {
      RSD_WARN("Error: %s\n", strerror(err));
      return -1;
   }

   if ((err = pthread_cond_destroy(&rsound->thread.cond)) != 0)
   {
      RSD_WARN("Error: %s\n", strerror(err));
      return -1;
   }

   free(rsound);

   return 0;
}

